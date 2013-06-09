#ifndef __CALLBACKS_H__
#define __CALLBACKS_H__

gboolean exchange_credentials(NiceAgent *agent, guint stream_id, gpointer data);
void attach_stdin2send_callback(NiceAgent *agent, guint stream_id, guint component_id, guint state);
void attach_stdin2send_callback_reliable(NiceAgent *agent, guint stream_id, guint component_id, gpointer data);
gboolean send_data(GIOChannel *source, GIOCondition cond, gpointer agent_ptr);
void recv_data2stdout(NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer data);

void start_server(NiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer server_ptr);
void start_server_reliable(NiceAgent *agent, guint stream_id, guint component_id, gpointer data);

void recv_data2fd(NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer data);

#endif
