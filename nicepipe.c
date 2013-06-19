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
#include "nice.h"

guint stun_port = 3478;
gchar* stun_host = NULL;
gint* is_caller = NULL;
gboolean not_reliable = FALSE;
gchar* remote_hostname = NULL;

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
GTimer* keepalive_timer = NULL;

guint output_fd;
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
  keepalive_timer = g_timer_new();
  g_timer_stop(keepalive_timer);

  // Connect to signals
  g_signal_connect(G_OBJECT(agent), "candidate-gathering-done", G_CALLBACK(exchange_credentials), NULL);

  if(not_reliable)
    g_signal_connect(G_OBJECT(agent), "component-state-changed",  G_CALLBACK(attach_stdin2send_callback), keepalive_timer);
  else
    g_signal_connect(G_OBJECT(agent), "reliable-transport-writable",  G_CALLBACK(attach_stdin2send_callback_reliable), keepalive_timer);

  output_fd = 1;
  nice_agent_attach_recv(agent, nice_stream_id, 1, g_main_loop_get_context(gloop), recv_data2fd, keepalive_timer);

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

  if(remote_hostname == NULL) {
    g_critical("No remote hostname given! (Please use -h)");
    exit(1);
  }

  g_option_context_free(context);
}


void
setup_glib() {
  gloop = g_main_loop_new(NULL, FALSE);
}

