#include <stdlib.h>

#include "nice.h"
#include "global.h"

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
