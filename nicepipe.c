#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <glib.h>
#include <gio/gio.h>

#include <agent.h>

#include "global.h"
#include "callbacks.h"
#include "util.h"

guint stun_port = 3478;
gchar* stun_host = NULL;
gint* is_caller = NULL;
gboolean not_reliable = FALSE;

gint max_size = 8;
gboolean verbose = FALSE;
gboolean beep = FALSE;
GOptionEntry all_options[] =
{
  { "stun_port", 'p', 0, G_OPTION_ARG_INT, &stun_port,
    "STUN server port (default: 3478)", "p" },
  { "stun_host", 's', 0, G_OPTION_ARG_STRING, &stun_host,
    "STUN server host (e.g. stunserver.org)", "s" },
  { "iscaller", 'c', 0, G_OPTION_ARG_INT, &is_caller,
    "1: is caller, 0 if not", "c" },
  { "not-reliable", 'u', 0, G_OPTION_ARG_INT, &not_reliable,
    "do not use pseudo TCP connection", "NULL" },
  { NULL }
};

#define G_LOG_DOMAIN    ((gchar*) 0)

GMainLoop *gloop;

guint nice_stream_id;

void parse_argv(int argc, char *argv[]);
void setup_glib();
NiceAgent* setup_libnice();

int
main(int argc, char *argv[]) {
  parse_argv(argc, argv);
  g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, log_stderr, NULL);

  setup_glib();
  
  NiceAgent *agent;
  agent = setup_libnice();

  // Connect to signals
  g_signal_connect(G_OBJECT(agent), "candidate-gathering-done", G_CALLBACK(exchange_credentials), NULL);

  if(not_reliable)
    g_signal_connect(G_OBJECT(agent), "component-state-changed",  G_CALLBACK(attach_send_callback), NULL);
  else
    g_signal_connect(G_OBJECT(agent), "reliable-transport-writable",  G_CALLBACK(attach_send_callback_reliable), NULL);

  nice_agent_attach_recv(agent, nice_stream_id, 1, g_main_loop_get_context(gloop), recv_data, NULL);

  g_debug("Starting to gather candidates...\n");
  if (!nice_agent_gather_candidates(agent, nice_stream_id)) {
    g_critical("Failed to start candidate gathering\n");
    
    g_main_loop_unref(gloop);
    g_object_unref(agent);

    exit(1);
  }

  // run async task using main loop
  g_main_loop_run(gloop);

  g_main_loop_unref(gloop);
  g_object_unref(agent);

  return EXIT_SUCCESS;
}


void
parse_argv(int argc, char *argv[]) {
  GOptionContext *context;
  GError* error = NULL;

  context = g_option_context_new("");
  g_option_context_add_main_entries(context, all_options, NULL);
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    g_print("option parsing failed: %s\n", error->message);
    g_error_free(error);
    exit(1);
  }

  g_option_context_free(context);
}


void
setup_glib() {
  gloop = g_main_loop_new(NULL, FALSE);
}


NiceAgent*
setup_libnice() {
  NiceAgent *agent;

  // create new agent
  if(not_reliable)
    agent = nice_agent_new(g_main_loop_get_context(gloop),
      NICE_COMPATIBILITY_RFC5245);    
  else
    agent = nice_agent_new_reliable(g_main_loop_get_context(gloop),
      NICE_COMPATIBILITY_RFC5245);
  if (agent == NULL) {
    g_critical("Failed to create NICE agent\n");
    exit(1);
  }

  // setup STUN server
  if(stun_host != NULL) {
    gchar* stun_addr;

    if(resolve_hostname(stun_host, &stun_addr)) {
      g_object_set(G_OBJECT(agent), "stun-server", stun_addr, NULL);
      g_object_set(G_OBJECT(agent), "stun-server-port", stun_port, NULL);
      g_debug("Using STUN server %s(%s):%i\n", stun_host, stun_addr, stun_port);

      g_free(stun_addr);
    }
    else {
      g_critical("Error resolving stun hostname '%s'\n", stun_host);
      g_object_unref(agent);

      exit(1);
    }
  }

  // setup who's caller and callee
  if(is_caller)
    g_debug("This instance is the caller\n");
  else
    g_debug("This instance is the callee\n");
  g_object_set(G_OBJECT(agent), "controlling-mode", is_caller, NULL);

  // add a communication stream
  nice_stream_id = nice_agent_add_stream(agent, 1);
  if (nice_stream_id == 0) {
    g_critical("Error adding NICE stream!\n");
    g_object_unref(agent);

    exit(1);
  }

  return agent;
}