#include <glib.h>
#include <agent.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "util.h"
#include "callbacks.h"
#include "global.h"

gboolean
exchange_credentials(NiceAgent *agent, guint stream_id, gpointer data) {
  g_debug("exchange_credentials(): candidate gathering done\n");

  gint retval;
  gboolean executed;

  // publish local credentials
  gchar publish_cmd[] = "./niceexchange.sh 0 publish dummy";
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


  // lookup remote credentials
  gchar lookup_cmd[] = "./niceexchange.sh 0 lookup dummy";
  if(is_caller)
    lookup_cmd[18] = '1';

  gchar *stdout;
  retval = execute_sync(lookup_cmd, NULL, &stdout, NULL);
  if(retval != 0) {
    g_critical("niceexchange lookup returned a non-zero return value (%i)!", retval);
 
    g_main_loop_unref(gloop);
    g_object_unref(agent);

    exit(1);
  }
  parse_remote_data(agent, stream_id, 1, stdout, strlen(stdout));

  g_free(stdout);
  g_debug("lookup remote credentials done\n");


  // unpublish local credentials
  /*
    TODO: do this after successfull connection

  gchar unpublish_cmd[] = "./niceexchange.sh 0 unpublish dummy";
  if(is_caller)
    unpublish_cmd[18] = '1';

  retval = execute_sync(unpublish_cmd, NULL, NULL, NULL);
  if(retval != 0) {
    g_critical("niceexchange unpublish returned a non-zero return value (%i)!", retval);
 
    g_main_loop_unref(gloop);
    g_object_unref(agent);

    exit(1);
  }
  g_print("unpublish local credentials done\n");
  */

  g_print("candidate gathering done\n");
}


void
attach_send_callback(NiceAgent *agent, guint stream_id, guint component_id, guint state) {
  if (state == NICE_COMPONENT_STATE_READY) {
    GIOChannel* io_stdin;
    io_stdin = g_io_channel_unix_new(fileno(stdin));

    g_io_add_watch(io_stdin, G_IO_IN, send_data, agent);
  }

  if (state == NICE_COMPONENT_STATE_FAILED) {
    g_main_loop_quit (gloop);
  }
}

void
attach_send_callback_reliable(NiceAgent *agent, guint stream_id, guint component_id, gpointer data) {
  GIOChannel* io_stdin;
  io_stdin = g_io_channel_unix_new(fileno(stdin));

  g_io_add_watch(io_stdin, G_IO_IN, send_data, agent);
}

gboolean
send_data(GIOChannel *source, GIOCondition cond, gpointer agent_ptr) {
  NiceAgent *agent = agent_ptr;
  gsize max_len = 10240;
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

void
recv_data(NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer data) {
  g_printf("%.*s", len, buf);
  fflush(stdout);
}
