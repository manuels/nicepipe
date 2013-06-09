#ifndef __GLOBAL_H__
#define __GLOBAL_H__

extern GMainLoop *gloop;
extern guint nice_stream_id;

extern gboolean not_reliable;
extern guint stun_port;
extern gchar* stun_host;
extern gint* is_caller;
extern guint output_fd;
#endif
