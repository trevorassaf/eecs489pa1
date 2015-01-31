#pragma once

#define MAX_RPEERS 6
/**
 * Packet formats for peer redirection.
 */
struct autojoin_peer {
  int ipv4;
  short port, reserved;
};

struct autojoin_msg {
  char vers, type;
  short num_peers;
};
