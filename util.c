#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "global.h"

static const gchar *candidate_type_name[] = {"host", "srflx", "prflx", "relay"};

void
log_stderr(const gchar *log_domain,
            GLogLevelFlags log_level,
            const gchar *message,
            gpointer user_data) {
  if(log_level < G_LOG_LEVEL_DEBUG)
    fprintf(stderr, "%i: %s", is_caller, message);
}

gboolean
resolve_hostname(gchar* hostname, gchar** out_addr) {
  GList* resolved_addresses;
  GResolver* resolver;

  resolver = g_resolver_get_default();
  resolved_addresses = g_resolver_lookup_by_name(resolver, hostname,
    NULL, NULL);

  if(resolved_addresses == NULL) {
    *out_addr = NULL;
    g_object_unref(resolver);
    return FALSE;
  }

  *out_addr = g_inet_address_to_string(resolved_addresses->data);

  g_list_free(resolved_addresses);
  g_object_unref(resolver);

  return TRUE;
}


gint
execute_sync(gchar *cmd, gchar* stdin, gchar** stdout, gchar** stderr) {
  GPid pid;
  gboolean spawned;
  gint exit_status;
  gint stdio[3];
  gint i;

  gchar **env = g_get_environ();
  env = g_environ_setenv(env, "NICE_REMOTE_HOSTNAME", remote_hostname, TRUE);
  gchar** argv;
  gint argc;

  // parse command line to argv array
  if(!g_shell_parse_argv(cmd, &argc, &argv, NULL)) {
    g_critical("Error parsing command line '%s'", cmd);

    exit(1);
  }

  g_debug("Executing '%s'\n", cmd);
  // spawn process
  spawned = g_spawn_async_with_pipes(".", argv, env, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL,
    &pid, &stdio[0], &stdio[1], &stdio[2], NULL);
  g_assert(spawned);

  // write stdin data
  if(stdin)
    write(stdio[0], stdin, strlen(stdin));
  close(stdio[0]);

  // wait for process to finish
  waitpid(pid, &exit_status, 0);
  g_spawn_close_pid(pid);

  gsize max_size = 10240;
  gsize bytes_read;

  // read stdout
  if(stdout) {
    gchar* buf = g_malloc(max_size*sizeof(gchar));

    bytes_read = read(stdio[1], buf, sizeof(gchar)*max_size);
    g_assert(bytes_read != max_size); // let's assume the output won't be larger than max_size-1
    *stdout = g_malloc0(bytes_read);
    memcpy(*stdout, buf, bytes_read);

    g_free(buf);
  }

  // read stderr
  if(stderr) {
    gchar* buf = g_malloc(max_size*sizeof(gchar));

    bytes_read = read(stdio[2], buf, sizeof(gchar)*max_size);
    g_assert(bytes_read != max_size); // let's assume the output won't be larger than max_size-1
    *stderr = g_malloc0(bytes_read);
    memcpy(*stderr, buf, bytes_read);

    g_free(buf);
  }

  close(stdio[1]);
  close(stdio[2]);

  return exit_status;
}


void
local_credentials_to_string(NiceAgent *agent, guint stream_id, guint component_id, gchar** out) {
  gchar buf[1024];
  gchar tmp[1024];
  gchar *local_ufrag = NULL;
  gchar *local_password = NULL;
  gchar ipaddr[INET6_ADDRSTRLEN];
  GSList *cands = NULL, *item;

  if(!nice_agent_get_local_credentials(agent, stream_id,
      &local_ufrag, &local_password)) {
    g_critical("Error reading local credentials!");
    g_object_unref(agent);
    exit(1);
  }

  cands = nice_agent_get_local_candidates(agent, stream_id, component_id);
  if(cands == NULL) {
    g_critical("Error reading local candidates!");
    g_object_unref(agent);
    exit(1);
  }

  sprintf(buf, "%s %s", local_ufrag, local_password);

  for (item = cands; item; item = item->next) {
    NiceCandidate *c = (NiceCandidate *)item->data;

    nice_address_to_string(&c->addr, ipaddr);

    // (foundation),(prio),(addr),(port),(type)
    sprintf(tmp, " %s,%u,%s,%u,%s",
        c->foundation,
        c->priority,
        ipaddr,
        nice_address_get_port(&c->addr),
        candidate_type_name[c->type]);
    strcat(buf, tmp);
  }
  sprintf(tmp, "\n");
  strcat(buf, tmp);

  g_free(local_ufrag);
  g_free(local_password);
  g_slist_free_full(cands, (GDestroyNotify)&nice_candidate_free);

  *out = g_malloc(strlen(buf)+1);
  strcpy(*out, buf);
}


void
parse_remote_data(NiceAgent *agent, guint stream_id, guint component_id, char *line, gsize len) {
  GSList *remote_candidates = NULL;
  gchar **line_argv = NULL;
  const gchar *ufrag = NULL;
  const gchar *passwd = NULL;
  int result = EXIT_FAILURE;
  int i;

  g_assert(line[len] == '\0'); // Make sure string is null-terminated

  line_argv = g_strsplit_set(line, " \t\n", 0);
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
        g_critical("failed to parse candidate: %s", line_argv[i]);

        if (line_argv != NULL)
          g_strfreev(line_argv);
        if (remote_candidates != NULL)
          g_slist_free_full(remote_candidates, (GDestroyNotify)&nice_candidate_free);
        exit(1);
      }
      remote_candidates = g_slist_prepend(remote_candidates, c);
    }
  }
  if (ufrag == NULL || passwd == NULL || remote_candidates == NULL) {
    g_critical("line must have at least ufrag, password, and one candidate");

    if (line_argv != NULL)
      g_strfreev(line_argv);
    if (remote_candidates != NULL)
      g_slist_free_full(remote_candidates, (GDestroyNotify)&nice_candidate_free);
    exit(1);
  }

  if (!nice_agent_set_remote_credentials(agent, stream_id, ufrag, passwd)) {
    g_critical("failed to set remote credentials");

    if (line_argv != NULL)
      g_strfreev(line_argv);
    if (remote_candidates != NULL)
      g_slist_free_full(remote_candidates, (GDestroyNotify)&nice_candidate_free);
    exit(1);
  }

  // Note: this will trigger the start of negotiation.
  if (nice_agent_set_remote_candidates(agent, stream_id, component_id,
      remote_candidates) < 1) {
    g_critical("failed to set remote candidates");

    if (line_argv != NULL)
      g_strfreev(line_argv);
    if (remote_candidates != NULL)
      g_slist_free_full(remote_candidates, (GDestroyNotify)&nice_candidate_free);
    exit(1);
  }
}


NiceCandidate*
parse_candidate(char *scand, guint stream_id) {
  NiceCandidate *cand = NULL;
  NiceCandidateType ntype;
  gchar **tokens = NULL;
  guint i;

  tokens = g_strsplit(scand, ",", 5);
  for(i = 0; tokens && tokens[i]; i++);
  if (i != 5)
    goto end;

  for (i = 0; i < G_N_ELEMENTS(candidate_type_name); i++) {
    if (strcmp(tokens[4], candidate_type_name[i]) == 0) {
      ntype = i;
      break;
    }
  }
  if (i == G_N_ELEMENTS(candidate_type_name))
    goto end;

  cand = nice_candidate_new(ntype);
  cand->component_id = 1;
  cand->stream_id = stream_id;
  cand->transport = NICE_CANDIDATE_TRANSPORT_UDP;
  strncpy(cand->foundation, tokens[0], NICE_CANDIDATE_MAX_FOUNDATION);
  cand->priority = atoi(tokens[1]);

  if(!nice_address_set_from_string(&cand->addr, tokens[2])) {
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

void
publish_local_credentials(NiceAgent* agent, guint stream_id) {
  gint retval;

  // publish local credentials
  gchar publish_cmd[1024];
  g_snprintf(publish_cmd, sizeof(publish_cmd), "./niceexchange.sh 0 %s publish dummy", remote_hostname);
  if(is_caller)
    publish_cmd[18] = '1';

  gchar *stdin;
  local_credentials_to_string(agent, stream_id, 1, &stdin);

  retval = execute_sync(publish_cmd, stdin, NULL, NULL);
  if(retval != 0) {
    g_critical("niceexchange publish returned a non-zero return value (%i)!", retval);
 
    g_main_loop_unref(gloop);
    g_object_unref(agent);

    exit(1);
  }
  g_free(stdin);
  g_debug("published local credentials\n");
}

void
unpublish_local_credentials(NiceAgent* agent, guint stream_id) {
  guint retval;

  g_debug("lookup remote credentials done\n");
  gchar unpublish_cmd[1024];
  g_snprintf(unpublish_cmd, sizeof(unpublish_cmd), "./niceexchange.sh 0 %s unpublish dummy", remote_hostname);
  if(is_caller)
    unpublish_cmd[18] = '1';

  retval = execute_sync(unpublish_cmd, NULL, NULL, NULL);
  if(retval != 0) {
    g_critical("niceexchange unpublish returned a non-zero return value (%i)!", retval);
 
    g_main_loop_unref(gloop);
    g_object_unref(agent);

    exit(1);
  }
  g_debug("unpublish local credentials done\n");
}

void
lookup_remote_credentials(NiceAgent* agent, guint stream_id) {
  guint retval;

  gchar lookup_cmd[1024];
  g_snprintf(lookup_cmd, sizeof(lookup_cmd), "./niceexchange.sh 0 %s lookup dummy", remote_hostname);
  if(is_caller)
    lookup_cmd[18] = '1';

  gchar *stdout;
  gchar *stderr;
  retval = execute_sync(lookup_cmd, NULL, &stdout, &stderr);
  if(retval != 0) {
    g_critical("niceexchange lookup returned a non-zero return value (%i)!", retval);
    if(stderr != NULL)
      g_critical("This was written to stderr:\n%s", stderr);
    g_free(stdout);
    g_free(stderr);
 
    g_main_loop_unref(gloop);
    g_object_unref(agent);

    exit(1);
  }
  parse_remote_data(agent, stream_id, 1, stdout, strlen(stdout));
  g_free(stdout);
  g_free(stderr);
}

void
pipe_stdio_to_hook(const gchar* envvar_name) {
  gchar** argv;
  gint argc;
  gchar **env = g_get_environ();
  env = g_environ_setenv(env, "NICE_REMOTE_HOSTNAME", remote_hostname, TRUE);

  gchar* cmd = g_getenv(envvar_name);
  g_debug("pipe_stdio_to_hook('%s')=%s\n", envvar_name, cmd);
  if(cmd == NULL || strlen(cmd) == 0)
    return;

  // parse command line to argv array
  if(!g_shell_parse_argv(cmd, &argc, &argv, NULL)) {
    g_critical("Error parsing command line '%s'", cmd);

    exit(1);
  }

  gboolean spawned;
  GPid pid;
  gint stdio[2];
  GError* error = NULL;

  g_debug("Executing '%s'\n", cmd);
  spawned = g_spawn_async_with_pipes(".", argv, env, G_SPAWN_CHILD_INHERITS_STDIN, NULL, NULL,
    &pid, NULL, NULL, NULL, &error);

  if(error != NULL) {
    g_critical("Error executing '%s': %s", cmd, error->message);
  }
  g_assert(spawned);
}

gboolean
parse_packet(gchar* buffer, gsize *buf_len, gchar* packet, gsize* packet_len) {
  gchar ip_ver = buffer[0] & 0xf0;
  switch(ip_ver) {
    case 0x40:
      *packet_len = buffer[2] < 8 | buffer[3];
    break;
    case 0x60:
    //break;
    default:
      g_critical("Unknown packet type (%x)!", ip_ver);
      exit(1);
  }
  if(*buf_len >= *packet_len) {
    memcpy(packet, buffer, *packet_len);
    *buf_len -= *packet_len;
    return TRUE;
  }
  return FALSE;
}
