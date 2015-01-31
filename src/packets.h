#pragma once

#include <sys/types.h>

#define MAX_RPEERS 6

/**
 * Packet formats for peer redirection.
 */
struct peer_addr {
  uint32_t ipv4;
  uint16_t port, reserved;
};

struct message_header {
  char vers, type;
  uint16_t num_peers;
};
