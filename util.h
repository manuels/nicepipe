#ifndef __UTIL_H__
#define __UTIL_H__

#include <glib.h>
#include <agent.h>

gboolean resolve_hostname(gchar* hostname, gchar** out_addr);
gint execute_sync(gchar *cmd, gchar* stdin, gchar** stdout, gchar** stderr);
void local_credentials_to_string(NiceAgent *agent, guint stream_id, guint component_id, gchar** out);
void parse_remote_data(NiceAgent *agent, guint stream_id, guint component_id, char *line, gsize len);
NiceCandidate* parse_candidate(char *scand, guint stream_id);

void publish_local_credentials(NiceAgent* agent, guint stream_id);
void unpublish_local_credentials(NiceAgent* agent, guint stream_id);
void lookup_remote_credentials(NiceAgent* agent, guint stream_id);

void log_stderr(const gchar *log_domain,
                      GLogLevelFlags log_level,
                      const gchar *message,
                      gpointer user_data);

gboolean
parse_packet(gchar* buffer, gsize *buf_len, gchar* packet, gsize* packet_len);

gboolean exit_if_child_exited(gpointer data);

#endif
