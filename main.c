/*
 * Copyright 2013 Manuel Schölling (manuel.schoelling@gmx.de)
 * Copyright 2013 University of Chicago
 *  Contact: Bryce Allen
 * Copyright 2013 Collabora Ltd.
 *  Contact: Youness Alaoui
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Alternatively, the contents of this file may be used under the terms of the
 * the GNU Lesser General Public License Version 2.1 (the "LGPL"), in which
 * case the provisions of LGPL are applicable instead of those above. If you
 * wish to allow use of your version of this file only under the terms of the
 * LGPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replace
 * them with the notice and other provisions required by the LGPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the LGPL.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <gio/gio.h>

#include <agent.h>

static GMainLoop *gloop;
static GIOChannel* io_stdin;

static guint nice_stream_id;

struct options_struct {
	char* stun_addr;
	guint stun_port;
  gboolean is_caller;
};
struct options_struct options;


static const gchar *candidate_type_name[] = {"host", "srflx", "prflx", "relay"};

static const gchar *state_name[] = {"disconnected", "gathering", "connecting",
                                    "connected", "ready", "failed"};

static int print_local_data_to_fid(FILE* fid, NiceAgent *agent, guint stream_id, guint component_id);
static int parse_remote_data(NiceAgent *agent, guint stream_id, guint component_id, char *line);
static void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer data);
static void cb_new_selected_pair(NiceAgent *agent, guint stream_id,	guint component_id,
	gchar *lfoundation, gchar *rfoundation, gpointer data);
static void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
  guint len, gchar *buf, gpointer data);
static void cb_attach_send_callback(NiceAgent *agent, guint stream_id, guint component_id, gpointer data);
static gboolean stdin_remote_info_cb (GIOChannel *source, GIOCondition cond, gpointer data);
static gboolean stdin_send_data_cb (GIOChannel *source, GIOCondition cond, gpointer data);

gboolean parse_argv(struct options_struct* options, int argc, char *argv[]);


int
main(int argc, char *argv[]) {
  NiceAgent *agent;

  parse_argv(&options, argc, argv);

  gloop = g_main_loop_new(NULL, FALSE);
  io_stdin = g_io_channel_unix_new(fileno(stdin));

  // Create the nice agent
  agent = nice_agent_new_reliable(g_main_loop_get_context (gloop), NICE_COMPATIBILITY_RFC5245);
  if (agent == NULL)
    g_error("Failed to create agent");

  // Set the STUN settings and controlling mode
  if (options.stun_addr) {
    g_object_set(G_OBJECT(agent), "stun-server", options.stun_addr, NULL);
    g_object_set(G_OBJECT(agent), "stun-server-port", options.stun_port, NULL);
  }
  g_object_set(G_OBJECT(agent), "controlling-mode", options.is_caller, NULL);

  // Connect to the signals
  g_signal_connect(G_OBJECT(agent), "candidate-gathering-done", G_CALLBACK(cb_candidate_gathering_done), NULL);
  g_signal_connect(G_OBJECT(agent), "new-selected-pair",        G_CALLBACK(cb_new_selected_pair), NULL);
  g_signal_connect(G_OBJECT(agent), "reliable-transport-writable",  G_CALLBACK(cb_attach_send_callback), NULL);

  // Create a new stream with one component
  nice_stream_id = nice_agent_add_stream(agent, 1);
  if (nice_stream_id == 0)
    g_error("Failed to add stream");

  // Attach to the component to receive the data
  // Without this call, candidates cannot be gathered
  nice_agent_attach_recv(agent, nice_stream_id, 1, g_main_loop_get_context(gloop), cb_nice_recv, NULL);


  // Start gathering local candidates
  if (!nice_agent_gather_candidates(agent, nice_stream_id))
    g_error("Failed to start candidate gathering");

  g_debug("waiting for candidate-gathering-done signal...");

  // Run the mainloop. Everything else will happen asynchronously
  // when the candidates are done gathering.
  g_main_loop_run (gloop);

  g_main_loop_unref(gloop);
  g_object_unref(agent);
  g_io_channel_unref (io_stdin);

  return EXIT_SUCCESS;
}

gboolean
parse_argv(struct options_struct* options, int argc, char *argv[]) {
  options->stun_addr = NULL;
  options->stun_port;

  // Parse arguments
  if (argc > 4 || argc < 2 || argv[1][1] != '\0') {
    fprintf(stderr, "Usage: %s 0|1 stun_addr [stun_port]\n", argv[0]);
    return EXIT_FAILURE;
  }
  options->is_caller = argv[1][0] - '0';
  if (options->is_caller != 0 && options->is_caller != 1) {
    fprintf(stderr, "Usage: %s 0|1 stun_addr [stun_port]\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (argc > 2) {
  	char* stun_hostname = argv[2];

	  GList* resolved_addresses;
	  GResolver* resolver;
	  resolver = g_resolver_get_default ();
	  resolved_addresses = g_resolver_lookup_by_name(resolver, stun_hostname, NULL, NULL);
	  if(resolved_addresses == NULL)
	  	g_error("Error resolving stun hostname");
		options->stun_addr = g_inet_address_to_string(resolved_addresses->data);

    if (argc > 3)
      options->stun_port = atoi(argv[3]);
    else
      options->stun_port = 3478;

    g_debug("Using stun server '[%s]:%u'\n", options->stun_addr, options->stun_port);
  }
}

static void
cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer data) {
  g_debug("SIGNAL candidate gathering done\n");

  GPid pid;
  gint child_stdin;
  int child_exit_status;
  gboolean spawned;

  gchar iscaller[] = "0";
  if(options.is_caller)
  	iscaller[0] = '1';

  // Publish own credentials
  gchar *argv_publish[] = {"./niceexchange.sh", iscaller, "publish", "dummy", NULL};  
  spawned = g_spawn_async_with_pipes(".", argv_publish, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL,
  	&pid, &child_stdin, NULL, NULL, NULL);
  g_assert(spawned);

  print_local_data_to_fid(fdopen(child_stdin, "w"), agent, stream_id, 1);

  close(child_stdin);
  waitpid(pid, &child_exit_status, 0);
  g_assert(child_exit_status == 0);
	g_spawn_close_pid(pid);

	// Lookup remote credentials
	gint child_stdout;
  gchar *argv_lookup[] = {"./niceexchange.sh", iscaller, "lookup", "dummy", NULL};  
  spawned = g_spawn_async_with_pipes(".", argv_lookup, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL,
  	&pid, NULL, &child_stdout, NULL, NULL);
  g_assert(spawned);

	static GIOChannel* io_stdout;
  io_stdout = g_io_channel_unix_new(child_stdout);
  g_io_add_watch(io_stdout, G_IO_IN, stdin_remote_info_cb, agent);

  waitpid(pid, &child_exit_status, 0);
  g_assert(child_exit_status == 0);
	g_spawn_close_pid(pid);
	// TODO:
  //g_io_channel_unref(io_stdout);
  //close(child_stdout);
}

static gboolean
stdin_remote_info_cb(GIOChannel *source, GIOCondition cond, gpointer data) {
  NiceAgent *agent = data;
  gchar *line = NULL;
  int rval;
  gboolean ret = TRUE;

  if (g_io_channel_read_line(source, &line, NULL, NULL, NULL) ==
      G_IO_STATUS_NORMAL) {
    // Parse remote candidate list and set it on the agent
    rval = parse_remote_data(agent, nice_stream_id, 1, line);
    if (rval == EXIT_SUCCESS) {
      // Return FALSE so we stop listening to stdin since we parsed the
      // candidates correctly
      ret = FALSE;
      g_debug("waiting for state READY or FAILED signal...");
    } else {
      g_error("ERROR: failed to parse remote data\n");
    }
    g_free (line);
  }

  return ret;
}

static void cb_attach_send_callback(NiceAgent *agent, guint stream_id, guint component_id, gpointer data) {
  g_io_add_watch(io_stdin, G_IO_IN, stdin_send_data_cb, agent);
}

static gboolean
stdin_send_data_cb(GIOChannel *source, GIOCondition cond, gpointer agent_ptr) {
  NiceAgent *agent = agent_ptr;
  gsize max_len = 1024;
  gchar *data = g_malloc(max_len);
  gsize len;

  gint total = 0;
  do {
  	if(g_io_channel_read_chars(source, data, max_len, &len, NULL) == G_IO_STATUS_NORMAL)
	    total += nice_agent_send(agent, nice_stream_id, 1, len, data);
	  else
	  	break;
  }
  while(len < max_len);

  g_free(data);
  return TRUE;
}

static void
cb_new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id, gchar *lfoundation,
	  gchar *rfoundation, gpointer data) {
  g_debug("SIGNAL: selected pair %s %s", lfoundation, rfoundation);
}

static void
cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer data) {
  if (len == 1 && buf[0] == '\0')
    g_main_loop_quit (gloop);
  printf("%.*s", len, buf);
  fflush(stdout);
}

static NiceCandidate *
parse_candidate(char *scand, guint stream_id) {
  NiceCandidate *cand = NULL;
  NiceCandidateType ntype;
  gchar **tokens = NULL;
  guint i;

  tokens = g_strsplit (scand, ",", 5);
  for (i = 0; tokens && tokens[i]; i++);
  if (i != 5)
    goto end;

  for (i = 0; i < G_N_ELEMENTS (candidate_type_name); i++) {
    if (strcmp(tokens[4], candidate_type_name[i]) == 0) {
      ntype = i;
      break;
    }
  }
  if (i == G_N_ELEMENTS (candidate_type_name))
    goto end;

  cand = nice_candidate_new(ntype);
  cand->component_id = 1;
  cand->stream_id = stream_id;
  cand->transport = NICE_CANDIDATE_TRANSPORT_UDP;
  strncpy(cand->foundation, tokens[0], NICE_CANDIDATE_MAX_FOUNDATION);
  cand->priority = atoi (tokens[1]);

  if (!nice_address_set_from_string(&cand->addr, tokens[2])) {
    g_message("failed to parse addr: %s", tokens[2]);
    nice_candidate_free(cand);
    cand = NULL;
    goto end;
  }

  nice_address_set_port(&cand->addr, atoi (tokens[3]));

 end:
  g_strfreev(tokens);

  return cand;
}


static int
print_local_data_to_fid(FILE *fid, NiceAgent *agent, guint stream_id, guint component_id) {
  int result = EXIT_FAILURE;
  gchar *local_ufrag = NULL;
  gchar *local_password = NULL;
  gchar ipaddr[INET6_ADDRSTRLEN];
  GSList *cands = NULL, *item;

  if (!nice_agent_get_local_credentials(agent, stream_id,
      &local_ufrag, &local_password))
    goto end;

  cands = nice_agent_get_local_candidates(agent, stream_id, component_id);
  if (cands == NULL)
    goto end;

  fprintf(fid, "%s %s", local_ufrag, local_password);

  for (item = cands; item; item = item->next) {
    NiceCandidate *c = (NiceCandidate *)item->data;

    nice_address_to_string(&c->addr, ipaddr);

    // (foundation),(prio),(addr),(port),(type)
    fprintf(fid, " %s,%u,%s,%u,%s",
        c->foundation,
        c->priority,
        ipaddr,
        nice_address_get_port(&c->addr),
        candidate_type_name[c->type]);
  }
  fprintf(fid, "\n");
  result = EXIT_SUCCESS;

 end:
  if (local_ufrag)
    g_free(local_ufrag);
  if (local_password)
    g_free(local_password);
  if (cands)
    g_slist_free_full(cands, (GDestroyNotify)&nice_candidate_free);

	fclose(fid);

  return result;
}


static int
parse_remote_data(NiceAgent *agent, guint stream_id,
    guint component_id, char *line) {
  GSList *remote_candidates = NULL;
  gchar **line_argv = NULL;
  const gchar *ufrag = NULL;
  const gchar *passwd = NULL;
  int result = EXIT_FAILURE;
  int i;

  line_argv = g_strsplit_set (line, " \t\n", 0);
  for (i = 0; line_argv && line_argv[i]; i++) {
    if (strlen (line_argv[i]) == 0)
      continue;

    // first two args are remote ufrag and password
    if (!ufrag) {
      ufrag = line_argv[i];
    } else if (!passwd) {
      passwd = line_argv[i];
    } else {
      // Remaining args are serialized canidates (at least one is required)
      NiceCandidate *c = parse_candidate(line_argv[i], stream_id);

      if (c == NULL) {
        g_message("failed to parse candidate: %s", line_argv[i]);
        goto end;
      }
      remote_candidates = g_slist_prepend(remote_candidates, c);
    }
  }
  if (ufrag == NULL || passwd == NULL || remote_candidates == NULL) {
    g_message("line must have at least ufrag, password, and one candidate");
    goto end;
  }

  if (!nice_agent_set_remote_credentials(agent, stream_id, ufrag, passwd)) {
    g_message("failed to set remote credentials");
    goto end;
  }

  // Note: this will trigger the start of negotiation.
  if (nice_agent_set_remote_candidates(agent, stream_id, component_id,
      remote_candidates) < 1) {
    g_message("failed to set remote candidates");
    goto end;
  }

  result = EXIT_SUCCESS;

 end:
  if (line_argv != NULL)
    g_strfreev(line_argv);
  if (remote_candidates != NULL)
    g_slist_free_full(remote_candidates, (GDestroyNotify)&nice_candidate_free);

  return result;
}
