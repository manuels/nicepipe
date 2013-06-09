#ifndef __CALLBACKS_H__
#define __CALLBACKS_H__

gboolean exchange_credentials(NiceAgent *agent, guint stream_id, gpointer data);
void attach_send_callback(NiceAgent *agent, guint stream_id, guint component_id, guint state);
void attach_send_callback_reliable(NiceAgent *agent, guint stream_id, guint component_id, gpointer data);
gboolean send_data(GIOChannel *source, GIOCondition cond, gpointer agent_ptr);
void recv_data(NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer data);

#endif
