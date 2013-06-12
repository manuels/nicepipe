#include <glib.h>
#include <gio/gio.h>
#include <agent.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "util.h"
#include "callbacks.h"
#include "global.h"

gboolean
exchange_credentials(NiceAgent *agent, guint stream_id, gpointer data) {
  g_debug("exchange_credentials(): candidate gathering done\n");

  publish_local_credentials(agent, stream_id);
  lookup_remote_credentials(agent, stream_id);

  pipe_stdio_to_hook("NICE_PIPE_BEFORE");

  g_debug("candidate gathering done\n");
}

void
start_server(NiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer server_ptr) {
  GSocketService* server = (GSocketService*) server_ptr;
  g_debug("Server starts listening.\n");

  if(is_caller)
    g_socket_service_start(server);
  else
    setup_client(agent);

  pipe_stdio_to_hook("NICE_PIPE_AFTER");
  g_message("Connection to %s established.\n", remote_hostname);
}

void
start_server_reliable(NiceAgent *agent, guint stream_id, guint component_id, gpointer server_ptr) {
  GSocketService* server = (GSocketService*) server_ptr;
  g_debug("Server starts listening.\n");

  if(is_caller)
    g_socket_service_start(server);
  else
    setup_client(agent);

  pipe_stdio_to_hook("NICE_PIPE_AFTER");
  g_message("Connection to %s established.\n", remote_hostname);
}

void
attach_stdin2send_callback(NiceAgent *agent, guint stream_id, guint component_id, guint state) {
  if (state == NICE_COMPONENT_STATE_READY) {
    unpublish_local_credentials(agent, stream_id);
    GIOChannel* io_stdin;
    io_stdin = g_io_channel_unix_new(fileno(stdin));

    g_io_add_watch(io_stdin, G_IO_IN, send_data, agent);
  }

  if (state == NICE_COMPONENT_STATE_FAILED) {
    g_main_loop_quit (gloop);
  }
}

void
attach_stdin2send_callback_reliable(NiceAgent *agent, guint stream_id, guint component_id, gpointer data) {
  unpublish_local_credentials(agent, stream_id);

  GIOChannel* io_stdin;
  io_stdin = g_io_channel_unix_new(fileno(stdin));

  g_io_add_watch(io_stdin, G_IO_IN, send_data, agent);
}

gboolean
send_data(GIOChannel *source, GIOCondition cond, gpointer agent_ptr) {
  static char buffer[10240];
  struct iovec io;
  char crap[10240];
  const gsize max_size = 10240;
  gsize len;

  NiceAgent *agent = agent_ptr;
  struct msghdr msgh;
  struct cmsghdr *cmsg;

  gint res;
  do {
    io.iov_base = buffer;
    io.iov_len = max_size;
    memset(&msgh, 0, sizeof(msgh));
    msgh.msg_iov = &io;
    msgh.msg_iovlen = 5;
    msgh.msg_control = &crap;
    msgh.msg_controllen = sizeof(crap);

    int sock = g_io_channel_unix_get_fd(source);
    
    res = recvmsg(sock, &msgh, MSG_DONTWAIT);
    if(res > -1) {
      g_debug("recvmsg: %i\n", res);
      res = nice_agent_send(agent, nice_stream_id, 1, res, buffer);
      g_debug("nice_agent_send: %i\n", res);
    }
    else {
      if(errno != EAGAIN && errno != EWOULDBLOCK) {
        g_critical("Error sending: recvmsg() = %i, errno=%i\n", res, errno);
        g_main_loop_quit(gloop);
        break;
      }
      if(errno == EAGAIN || errno == EWOULDBLOCK)
        break;
    }
  }
  while(len < max_size);

  return TRUE;
}

void
recv_data2fd(NiceAgent *agent, guint stream_id, guint component_id, guint len,
    gchar *buf, gpointer data) {
  unpublish_local_credentials(agent, stream_id);
  g_debug("recv_data2fd(fd=%u, len=%u)\n", output_fd, len);
  write(output_fd, buf, len);
//  syncfs(output_fd);
}
