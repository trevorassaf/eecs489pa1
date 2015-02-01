#pragma once

#include <sys/types.h>

#define NETIMG_WIDTH  640
#define NETIMG_HEIGHT 480

#define NETIMG_MAXFNAME    256  // including terminating NULL
#define NETIMG_PORTSEP     ':'

#define NETIMG_VERS 0x2

#define NETIMG_QRY 0x1
#define NETIMG_RPY 0x2

#define NETIMG_FOUND 1
#define NETIMG_NFOUND 0
#define NETIMG_EVERS -1
#define NETIMG_ESIZE -2
#define NETIMG_EBUSY -3

#define NETIMG_QLEN       10 
#define NETIMG_LINGER      2
#define NETIMG_NUMSEG     50
#define NETIMG_MSS      1440
#define NETIMG_USLEEP 250000    // 250 ms

#define MAX_RPEERS 6

/**
 * Header for all packets.
 */
struct packet_header_t {
  char vers, type;
};

struct iqry_t {
  packet_header_t header;
  char iq_name[NETIMG_MAXFNAME];
};

struct imsg_t {
  packet_header_t header;
  char im_found;
  char im_depth;    // in bytes, not bits as returned by LTGA.GetPixelDepth()
  char im_format;
  char im_width;
  char im_height; 
  char im_adepth;   // not used
  char im_rle;      // not used
};

/**
 * Packet formats for peer redirection.
 */
struct peer_addr {
  uint32_t ipv4;
  uint16_t port, reserved;
};

// TODO change this to use packet_header_t
struct message_header {
  char vers, type;
  uint16_t num_peers;
};
