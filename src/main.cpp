#include <string>
#include <stdio.h>         // fprintf(), perror(), fflush()
#include <stdlib.h>        // atoi()
#include <assert.h>        // assert()
#include <sys/types.h>     // u_short
#include <arpa/inet.h>     // htons(), inet_ntoa()
#include <iostream>
#include <vector>
#include <algorithm>

#include "packets.h"
#include "Service.h"
#include "ServiceBuilder.h"
#include "ServerBuilder.h"
#include "Connection.h"
#include "SocketException.h"
#include "P2pTable.h"
#include "ImageNetwork.h"
#include "ltga.h"

/**
 * FQDN field pair.
 */
typedef struct {
  std::string name;     // host domain name 
  uint16_t port;        // port in host-byte-order 
} fqdn;

/**
 *  parseMaxPeers()
 *  - Parse max-peers cli param.
 */
void parseMaxPeers(char* cli_input, size_t& max_peers) {
  max_peers = atoi(cli_input); 
  net_assert(max_peers > 1, "Invalid 'max-peers' specified.");
}

/**
 * parseRemoteAddress()
 * - Parse remote-address cli param.
 */
void parseRemoteAddress(char* cli_input, fqdn& remote) {
  // Parse cli string for delimiter
  std::string fqdn_name_with_port(cli_input); 
  size_t delim_idx = fqdn_name_with_port.find(PR_PORT_DELIMITER);

  // Fail due to absent fqdn delimiter
  if (delim_idx == std::string::npos) {
    fprintf(stderr, "Malformed FQDN. Must be of the form: <fqdn>:<port>\n"); 
    exit(1);
  }
 
  // Configure fqdn
  remote.name = fqdn_name_with_port.substr(0, delim_idx);
  remote.port = atoi(fqdn_name_with_port.substr(delim_idx + 1).c_str());

  // Fail due to bad port
  net_assert(remote.port != 0, "Malformed port number. Port must be unsigned short.");
}

/**
 * parseCliOption()
 * - Return cli option indicated by this input string.
 * - Fail if 'cli_input' doesn't agree with expected format.
 */
CliOption parseCliOption(char* cli_input) {
  net_assert(*cli_input == PR_CLI_OPT_PREFIX, "Cli option must begin with '-', ex. '-p'"); 
  net_assert(*(cli_input + 2) == '\0', "Cli options must be 2 chars long, ex. '-p'");
  
  switch (*(cli_input + 1)) {
    case PR_ADDRESS_FLAG:
      return P;
    case PR_MAXPEERS_FLAG:
      return N;
    default:
      fprintf(stderr, "Invalid cli flag\n");
      exit(1);
  }
}

/**
 * setCliParam()
 * - Parse specified cli param from cli-input-string.
 */
void setCliParam(CliOption opt, char* cli_param_str, size_t& max_peers, fqdn& remote) {
  switch (opt) {
    case P:
      parseRemoteAddress(cli_param_str, remote);
      break;
    case N:
      parseMaxPeers(cli_param_str, max_peers);
      break;
    default:
      fprintf(stderr, "Bad CliOption enum value\n");
      exit(1);
  }
}

/**
 * connectToFirstPeer()
 * - Establish connection to first peer. Enables address/port for resuse.
 * @peer remote_fqdn : fqdn of target peer 
 */
Connection connectToFirstPeer(const fqdn& remote_fqdn) {
  ServerBuilder builder;
  return builder
      .setRemoteDomainName(remote_fqdn.name)
      .setRemotePort(remote_fqdn.port)
      .enableAddressReuse()
      .build();
}

/**
 * autoJoin()
 * - Supply peer with list of other peers to join.
 * @param connection : network connection to requesting peer node  
 * @param p2p_table : local table of connected peers 
 * @param message_type : type of message to include in the header
 */
void autoJoin(
  const Connection connection,
  const P2pTable& p2p_table,
  MessageType message_type) 
{
  // Compose redirect message
  peering_response_header_t join_header;
  join_header.header.vers = PM_VERS;
  join_header.header.type = message_type;

  size_t num_join_peers = 
      p2p_table.getNumPeers() < PR_MAXPEERS 
          ? p2p_table.getNumPeers() 
          : PR_MAXPEERS;
  join_header.num_peers = num_join_peers;

  size_t join_header_len = sizeof(join_header);
  std::string message((char *) &join_header, join_header_len); 

  std::unordered_set<int> peering_fds = p2p_table.getPeeringFdSet();
  auto iter = peering_fds.begin();

  for (size_t i = 0; i < num_join_peers; ++i) {
    // Fail because we ran out of fds
    assert(iter != peering_fds.end());

    int fd = *iter++;
    const Connection& connection = p2p_table.fetchConnectionByFd(fd);

    peer_addr_t peer = 
        { connection.getRemoteIpv4(), connection.getRemotePort(), 0 };
    size_t peer_len = sizeof(peer);

    message += std::string((char *) &peer, peer_len);
  }

  // Send auto-join message to requesting peer
  size_t remaining_chars = message.size(); 
  while (remaining_chars) {
    remaining_chars = connection.write(message);
    message = message.substr(message.size() - remaining_chars);
  }
}

/**
 * acceptPeerRequest()
 * - Local peer table has room, therefore accomodate the requesting peer.
 * - Send 'autojoin message' contain the 'welcome' header.  
 * @param connection : network connection to requesting peer node  
 * @param p2p_table : local table of connected peers 
 */
void acceptPeerRequest(const Connection& connection, P2pTable& p2p_table) {
  try {
    autoJoin(connection, p2p_table, WELCOME);
  } catch (SocketException& e) {
    fprintf(
        stderr,
        "Failed while accepting peering request from %s:%d\n",
        connection.getRemoteDomainName().c_str(),
        connection.getRemotePort()
    ); 
    
    connection.close();
    return;
  } 
  
  // Notify user of new peer
  fprintf(
      stderr,
      "\nConnected from %s:%d\n",
      connection.getRemoteDomainName().c_str(),
      connection.getRemotePort()
  ); 

  // Add new node to peer-table
  p2p_table.registerConnectedPeer(connection);
}

/**
 * redirectPeer()
 * - Local peer table is out of space, therefore redirect requesting peer.
 * - Send 'autojoin message' contain the 'redirect' header.  
 * @param connection : network connection to requesting peer node  
 * @param p2p_table : local table of connected peers 
 */
void redirectPeer(const Connection& connection, P2pTable& p2p_table) {
  try {
    autoJoin(connection, p2p_table, REDIRECT);
  } catch (SocketException& e) {
    fprintf(
        stderr,
        "Failed while redirecting peering request from %s:%d\n",
        connection.getRemoteDomainName().c_str(),
        connection.getRemotePort()
    ); 

    connection.close();
    return;
  } 
  
  // Notify user of redirected peer
  fprintf(
      stderr,
      "Peer table full: %s:%d redirected\n",
      connection.getRemoteDomainName().c_str(),
      connection.getRemotePort()
  ); 

  connection.close();
}

/**
 * isImageQuery()
 * - Returns true iff header indicates an image query.
 * @param header : header of incoming packet  
 */
bool isImageQuery(const packet_header_t& header) {
  return header.vers == NETIMG_VERS && header.type == NETIMG_QRY;
}

/**
 * isImageTransfer()
 * - Returns true iff header indicates an image transfer.
 * @param header : header of incoming packet  
 */
bool isImageTransfer(const packet_header_t& header) {
  return header.vers == PM_VERS && header.type == PM_SEARCH;
}

/**
 * sendImsgPacket()
 * - Send imsg_t to specified target.
 * @param packet : imsg_t to send
 * @param connection : target to which to send packet
 */
void sendImsgPacket(const imsg_t& packet, const Connection* connection) {
  std::string message( (char *) &packet, sizeof(packet));
  while (message.size()) {
    size_t bytes_remaining = connection->write(message);
    message = message.substr(message.size() - bytes_remaining);
  }
}

/**
 * rejectImageQuery()
 * - Send rejection message to querying client.
 * @param connection : querying client  
 */
void rejectImageQuery(const Connection* connection) {
  // Assemble imsg packet
  imsg_t imsg_packet;
  imsg_packet.header = {NETIMG_VERS, NETIMG_RPY};
  imsg_packet.im_found = NETIMG_EBUSY;

  // Send query rejection
  sendImsgPacket(imsg_packet, connection);
}

/**
 * returnImageToClient()
 * - Stream local image to client.
 * @param image_pixels : image pixel array (null if no image)
 * @param image_packet : packet containing image data
 * @param image_size : dimensions of image
 * @param img_net : image network
 */
void returnImageToClient(
  const char* image_pixels,
  const imsg_t& image_packet,
  long image_size,
  ImageNetwork& img_net
) {
  
  const Connection& image_client = img_net.getImageClient();

  // Send image packet 
  size_t packet_size = sizeof(imsg_t);
  std::string image_packet_str( (char *) &image_packet, packet_size);
  size_t remaining_packet_bytes = packet_size;

  while (remaining_packet_bytes) {
    remaining_packet_bytes = image_client.write(image_packet_str); 
    image_packet_str = image_packet_str.substr(image_packet_str.size() - remaining_packet_bytes);
  }

  // Send image (if found) in chunks of 'segsize'
  if (image_pixels) {
    size_t segsize = image_size / NETIMG_NUMSEG;
    segsize = segsize < NETIMG_MSS ? NETIMG_MSS : segsize;
    std::string image_pixels_str(image_pixels, image_size);
    size_t segment_bytes_remaining = 0;

    while (image_pixels_str.size()) {
      segsize = std::min(segsize, image_pixels_str.size());
      segment_bytes_remaining = image_client.write(image_pixels_str.substr(0, segsize));
      
      size_t segment_bytes_sent = segsize - segment_bytes_remaining;

      // Notify user of segment send
      fprintf(
          stderr,
          "\tSending image segment: size: %d, sent:%d\n",
          (int) image_pixels_str.size(),
          (int) (segment_bytes_sent)
      );
      
      // Consume sent bytes
      image_pixels_str = image_pixels_str.substr(segment_bytes_sent);
      
      // Throttle segement sends
      usleep(NETIMG_USLEEP);
    }
  }

  // Close connection and prepare image-network to service next image query
  img_net.invalidateImageClient();
}

/**
 * queryNetwork()
 * - Initiate or forward image query to the p2p network. Registers packet as 'seen.'
 *   Doesn't send packet if this node has seen this packet previously 
 * @param iqry_packet : packet for image query (network-byte-order fields)
 * @param img_net : image network
 */
void queryNetwork(const p2p_image_query_t& p2p_query, ImageNetwork& img_net) {

  // Drop this packet if we've seen it already
  if (img_net.hasSeenP2pImageQuery(p2p_query)) {
    // Report dropped packet
    fprintf(
      stderr,
      "\tDropped packet due to duplication! (file: %s, search-id: %i, ipv4: %i, port: %i)\n",
      p2p_query.file_name,
      ntohs(p2p_query.search_id),
      ntohl(p2p_query.orig_peer.ipv4),
      ntohs(p2p_query.orig_peer.port)
    );

    return;
  }

  // Add this query to the image-network's history
  img_net.addImageQuery(p2p_query); 

  // Forward query to connected peers
  const std::vector<const Connection*> connected_peers = img_net.getP2pTable().fetchConnectedPeers();
  std::cout << "\n\npeer-table size: " << img_net.getP2pTable().getNumPeers() << 
      ", # connected peers: " << connected_peers.size() << std::endl;

  for (const Connection* peer : connected_peers) {
    // Report forwarded image
    fprintf(
        stderr,
        "\tForwarding p2p image query to %s:%d\n",
        peer->getRemoteDomainName().c_str(),
        peer->getRemotePort()
    );

    // Send query to connected peer
    std::string message( (char *) &p2p_query, sizeof(p2p_query));
    while(message.size()) {
      size_t bytes_remaining = peer->write(message); 
      message = message.substr(message.size() - bytes_remaining);
    }
  }
}

/**
 * genOriginalP2pImageQuery()
 * - Assemble packet for querying the p2p network for an image.
 * @param iqry_packet : initial query packet sent by client
 * @param image_net : data for this image-network node
 * @return p2p_image_query_t : packet for querying the p2p image-network 
 */
const p2p_image_query_t genOriginalP2pImageQuery(
  const iqry_t& iqry_packet,
  ImageNetwork& image_net
) {
  // Packet header
  p2p_image_query_t p2p_query;
  p2p_query.header.vers = PM_VERS;
  p2p_query.header.type = PM_SEARCH;

  // Packet body
  p2p_query.search_id = htons(image_net.genSearchId());

  p2p_query.orig_peer.ipv4 = htonl(image_net.getImageClient().getRemoteIpv4());
  p2p_query.orig_peer.port = htons(image_net.getImageClient().getRemotePort());

  memcpy(p2p_query.file_name, iqry_packet.iq_name, NETIMG_MAXFNAME);

  return p2p_query;
}

/**
 * handleClientImageQuery()
 * - Register image query and search for image. Begin search locally.
 *   If this node can't find the requested image, then the node queries
 *   the p2p network for the image.
 * @param connection : querying client
 * @param img_net : image network
 */
void handleClientImageQuery(const iqry_t& iqry_packet, ImageNetwork& img_net) {
  LTGA image;
  imsg_t image_packet;
  memset(&image_packet, 0, sizeof(image_packet));
  long image_size;
  
  if (imgdb_loadimg(
        iqry_packet.iq_name,
        &image,
        &image_packet,
        &image_size) == NETIMG_FOUND
  ) {
    // Notify user that local node has image
    fprintf(
        stderr,
        "\tFound %s locally! Sending back to %s:%d...\n",
        iqry_packet.iq_name,
        img_net.getImageClient().getRemoteDomainName().c_str(),
        img_net.getImageClient().getRemotePort());

    // Send image back to client
    returnImageToClient( (char *) image.GetPixels(), image_packet, image_size, img_net); 
  } else {
    // Notify user of network query
    fprintf(
        stderr,
        "\tCouldn't find %s locally -- querying network...\n",
        iqry_packet.iq_name
    );

    // Forward query to network because we couldn't find it locally
    const p2p_image_query_t p2p_query_packet = genOriginalP2pImageQuery(iqry_packet, img_net);
    queryNetwork(p2p_query_packet, img_net);
  }
}

/**
 * handleImageTransfer()
 * - Receive image from transfering peer and forward result to querying client.
 * @param connection : peer that's transfering image  
 * @param img_net : image network
 */
void handleImageTransfer(const Connection* connection, ImageNetwork& img_net) {}

/**
 * handleP2pImageQuery()
 * - Process p2p traffic on the image-network. 2 cases:
 * @param header : header for p2p image packet
 * @param peer_client : connection to the querying peer 
 * @param img_net : image network data
 */
void handleP2pImageQuery(
  const p2p_image_query_t& header,
  const Connection& peer_client,
  ImageNetwork& img_net
) {
    
}

/**
 * handleImageTraffic()
 * - Accept image requests from netimg clients and receive image
 *   transfers from other peers.
 * @param img_net : image network data 
 */
void handleImageTraffic(ImageNetwork& img_net) {

  // Accept connection (client or peer)
  const Service& img_service = img_net.getImageService();
  const Connection* connection = img_service.acceptNew();

  // Report image connection
  fprintf(
      stderr,
      "\nReceived image connection from %s:%d\n",
      connection->getRemoteDomainName().c_str(),
      connection->getRemotePort()
  ); 

  // Read packet header
  packet_header_t header;
  size_t packet_header_len = sizeof(header);
  std::string message;

  while (message.size() < packet_header_len) {
    message += connection->read();
  }

  // Deserialize packet header
  memcpy(&header, message.c_str(), packet_header_len);

  // Determine packet header
  if (isImageQuery(header)) {
    // Reject query if we're already handling one...
    if (img_net.hasImageClient()) {
      // Notify user of query rejection
      fprintf(
        stderr,
        "\tRejecting image query because we're already servicing client %s:%d\n",
        img_net.getImageClient().getRemoteDomainName().c_str(),
        img_net.getImageClient().getRemotePort()
      );
      
      rejectImageQuery(connection);

      // Close connection
      connection->close();
      delete connection;
    } else {
      // Notify user of image query acceptance 
      fprintf(
        stderr,
        "\tServicing image query.\n"
      );

      // Finish reading iqry_t
      iqry_t iqry_packet;
      size_t iqry_packet_len = sizeof(iqry_packet);
      memset(&iqry_packet, 0, iqry_packet_len);
      iqry_packet.header = header;
      size_t packet_header_len = sizeof(header);
      size_t packet_body_len = iqry_packet_len - packet_header_len;

      while (message.size() < packet_body_len) {
        message += connection->read();
      }

      // Deserialize message body
      memcpy(&iqry_packet, message.c_str(), packet_body_len);
      
      // Report image query
      fprintf(
        stderr,
        "\tSearching for image: %s\n",
        iqry_packet.iq_name
      );

      img_net.setImageClient(connection);

      handleClientImageQuery(iqry_packet, img_net); 
    }
  } else if (isImageTransfer(header)) {

    // Notify user of image transfer
    fprintf(stderr, "\tReceiving image transfer!\n");
    
    // Handle image transfer
    handleImageTransfer(connection, img_net); 
    
    // Close connection
    connection->close();
    delete connection;

  } else {
    // Unrecognized packet, notify user
    fprintf(
        stderr,
        "\nWARNING: unrecognized packet header: <vers: %c, type: %c>\n",
        header.vers,
        header.type
    );

    // Close connection
    connection->close();
    delete connection;
  }
}

/**
 * handlePeeringClient()
 * - Accept peering request, if space available. Redirect, otherwise.
 * @param service : service listening for incoming connections
 * @param p2p_table : peer node registry
 */
void handlePeeringClient(const Service& p2p_service, P2pTable& p2p_table) {
  
  // Accept connection
  const Connection connection = p2p_service.accept();

  if (p2p_table.isFull()) {
    // This node is saturated, redurect client peer
    redirectPeer(connection, p2p_table);
  } else {
    // This node is unsaturated, accept the peering request 
    acceptPeerRequest(connection, p2p_table);
  }
}

/**
 * handleRecommendedPeerTraffic()
 * - Process peering responses (2 cases):
 *   1. Remote peer accepted our peering attempt.
 *   2. Remote peer rejected our peering attempt (redirection)
 * @param header : header for peering_response_packet  
 * @param buffer : buffer string (CAUTION: could have data on it)
 * @param connection : remote peer
 * @param img_net : image network data
 */
void handleRecommendedPeerTraffic(
  const peering_response_header_t& header,
  std::string buffer,
  const Connection& connection,
  ImageNetwork& img_net
) {

  // Exit early if there are no recommended peers
  if (!header.num_peers) {
    return; 
  }

  // Notify user of subsequent peers
  fprintf(stderr, "\tRecommended peer list:\n");

  // Read recommended peer packets from the wire
  size_t peer_addr_size = sizeof(peer_addr_t);
  size_t message_body_len = peer_addr_size * header.num_peers;
  
  while (buffer.size() < message_body_len) {
    buffer += connection.read();
  }

  // Validate message length
  if (buffer.size() != message_body_len) {
    // Fail because we received more bytes in the 'recommended-peer-message'
    // than expected
    fprintf(
        stderr,
        "\tERROR: invalid recommended peer message length. Expected: %zi, received: %zi\n",
        message_body_len,
        buffer.size()
    );
    exit(1);
  }

  P2pTable& p2p_table = img_net.getP2pTable();
  
  const char* message_cstr = buffer.c_str();
  std::vector<peer_addr_t> recommended_peers;
  recommended_peers.reserve(header.num_peers);

  size_t num_available_peers = 0;

  // Deserialize recommended peer-addr packets
  for (size_t i = 0; i < message_body_len; i += peer_addr_size) {
    peer_addr_t peer;
    memcpy(&peer, message_cstr + i, peer_addr_size);
    recommended_peers.push_back(peer);

    // Determine domain name of recommended peer
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(peer.port);
    addr.sin_addr.s_addr = htonl(peer.ipv4);
    
    char domain_name[PR_MAXFQDN + 1];
    memset(domain_name, 0, PR_MAXFQDN + 1);
    if (::getnameinfo(
      (struct sockaddr *) &addr,
      sizeof(addr),
      domain_name,
      PR_MAXFQDN,
      NULL,
      0,
      0) == -1)
    {
      throw SocketException("Failed to fetch domain name of recommended peer"); 
    }
    
    // Report peering attempt 
    std::string connection_type_str;
    if (p2p_table.isConnectedPeer(peer.ipv4, peer.port)) {
      connection_type_str = CONNECTED_MSG;
    } else if (p2p_table.isPendingPeer(peer.ipv4, peer.port)) {
      connection_type_str = PENDING_MSG; 
    } else if (p2p_table.isRejectedPeer(peer.ipv4, peer.port)) {
      connection_type_str = REJECTED_MSG; 
    } else {
      ++num_available_peers;
      connection_type_str = AVAILABLE_MSG;
    }

    fprintf(
        stderr,
        "\t\t%s:%d (%s)\n",
        domain_name,
        peer.port,
        connection_type_str.c_str()
    );
  }

  if (p2p_table.isFull()) {
    // Notify user that this node is saturated and that 
    // it can't peer with additional nodes (exit early)
    fprintf(
      stderr,
      "\tPeer table is full with %zi peers -- subsequent peering requests will be redirected.\n",
      p2p_table.getNumPeers()
    );
    
    return; /* skip recommended peers because we're full... */
  }

  // Notify user that we have additional room and can
  // peer with additional nodes.
  fprintf(
    stderr,
    "\tPeer table has room -- number of available peers: %zi\n",
    p2p_table.getMaxPeers() - p2p_table.getNumPeers()
  );

  if (!num_available_peers) {
    // Notify user that there aren't any available nodes 
    // to peer with in the list of recommended peers
    fprintf(stderr, "\tNo available peers to query...\n");

    return; /* skip recommended peers because none are available */
  }

  // Notify user that nodes are available and that we will 
  // now try to peer with them
  fprintf(stderr, "\tAttempting to peer with recommended nodes...\n");
  
  // Bind outgoing connections to the same port that
  // our p2p service is running on
  ServerBuilder server_builder;
  server_builder
    .setLocalPort(img_net.getP2pService().getPort())
    .enableAddressReuse();

  // Attempt to peer with recommended nodes 
  auto remote_iter = recommended_peers.begin();

  while (!p2p_table.isFull() && remote_iter != recommended_peers.end()) {
    // Skip peer if we're already connected/peering or 
    // we've been rejected by it recently
    if (!p2p_table.hasRemote(remote_iter->ipv4, remote_iter->port)) {
      try {
        // Connect to peer
        Connection new_peer = server_builder
            .setRemoteIpv4Address(remote_iter->ipv4)
            .setRemotePort(remote_iter->port)
            .build();
        
        // Put connection in p2p table in 'pending' state
        p2p_table.registerPendingPeer(new_peer);

        // Report peering attempt 
        fprintf(
            stderr,
            "\t\tConnecting to peer %s:%d (recommended by %s:%d)\n",
            new_peer.getRemoteDomainName().c_str(),
            new_peer.getRemotePort(),
            connection.getRemoteDomainName().c_str(),
            connection.getRemotePort()
        );

      } catch (const BusyAddressSocketException& e) {
        // Notify user that this node and its remote peer attempted
        // to connect simultaneously and that the remote connected
        // first => erase this socket
        fprintf(
            stderr,
            "WARNING: peers attempted to connect simulatenously (case 4). Killing local connection."
        ); 

// TODO find a way to close this socket... (probably need dynamic memory...)
      }
    }
    ++remote_iter;
  }

  // Report new staturation level of peer-table
  if (p2p_table.isFull()) {
    fprintf(stderr, "\tPeer table full! Skipping remaining recommended peers...\n");
  } else {
    fprintf(
        stderr,
        "\tPeer table still has vacancies -- number of open slots: %zi\n",
        p2p_table.getMaxPeers() - p2p_table.getNumPeers()
    );
  }

}

/**
 * handleP2pTraffic()
 * - Process message from peer.
 * @param service_port : port that service is running on (host-byte-order).
 *    All outgoing connections should bind to this local port.
 * @param service_port : port that the p2p service is runnning on 
 * @param fd : socket fd
 * @param img_net : image network 
 */
void handleP2pTraffic(uint16_t service_port, int fd, ImageNetwork& img_net) {
  
  // WARNING: 'connection' can't be reference because we will delete the original
  // and access this connection again.
  P2pTable& p2p_table = img_net.getP2pTable();
  const Connection connection = p2p_table.fetchConnectionByFd(fd);

  // Report response from peer 
  fprintf(
      stderr,
      "\nReceived p2p message from %s:%d\n",
      connection.getRemoteDomainName().c_str(),
      connection.getRemotePort()
  );
  
  // Read packet header from wire to determine message type
  packet_header_t packet_header;
  size_t packet_header_len = sizeof(packet_header);
  std::string message;
  
  while (message.size() < packet_header_len) {
    message += connection.read();
  }

  // Deserialize packet-header and consume packet data in buffer  
  memcpy(&packet_header, message.c_str(), packet_header_len);
  message = message.substr(packet_header_len);

  // Validate packet version
  if (packet_header.vers != PM_VERS) {
    // Report invalid version number
    fprintf(
        stderr,
        "ERROR: invalid packet version number(%i) received from peer %s:%d\n",
        packet_header.vers,
        connection.getRemoteDomainName().c_str(),
        connection.getRemotePort()
    ); 
    exit(1);
  }

  // Determine type of traffic
  if (packet_header.type == WELCOME || packet_header.type == REDIRECT) {
    // Report peering response message
    fprintf(stderr, "\tPeering response received!\n");

    // Register connection state change
    switch (packet_header.type) {
      // Connection was accepted by peer
      case WELCOME:
        // Transfer 'connection' state from 'pending' -> 'connected'
        p2p_table.registerConnectedPeer(connection);
       
        // Report connection
        fprintf(stderr, "\tConnection accepted!\n");
        break;
      
      // Peering request was rejected by peer
      case REDIRECT:
        // Transfer 'connection' state from 'pending' -> 'rejected'
        p2p_table.registerRejectedPeer(connection.getFd());  

        // Report redirection
        fprintf(stderr, "\tConnection attempt rejected!\n");
        break;

      // Report invalid packet type, then fail
      default:
        fprintf(
            stderr,
            "ERROR: invalid peering response packet type(%u) received!\n",
            packet_header.type
        );
        exit(1);
    }

    // Read remainder of peering_response_header_t packet
    peering_response_header_t resp_header;
    size_t bytes_remaining = sizeof(resp_header) - packet_header_len;
    
    while (message.size() < bytes_remaining) {
      message += connection.read();
    }
    
    // Deserialize peering response packet and consume peering_response_header
    // packet data in buffer
    resp_header.header = packet_header;

    memcpy(&resp_header.num_peers, message.c_str(), bytes_remaining);
    message = message.substr(bytes_remaining);

    // Report number of recommended peers
    fprintf(
        stderr,
        "\tNumber of recommended peers: %i\n",
        resp_header.num_peers 
    );

    // Validate peering_message_t message length
    if (!resp_header.num_peers && message.size()) {
      // No peers were recommended, so there shouldn't be any 
      // additional bytes on the wire...
      fprintf(
          stderr,
          "\tERROR: invalid peer response message length. Expected: %zi, received: %zi\n",
          bytes_remaining,
          bytes_remaining + message.size()
      );
      exit(1);
    }

    // Handle additional peers, if provided
    if (resp_header.num_peers) {
      handleRecommendedPeerTraffic(resp_header, message, connection, img_net);
    }

  } else if (packet_header.type == SEARCH) {
    // Report p2p image query traffic
    fprintf(stderr, "\tImage query received!\n");
    
    // Read remainder of p2p_image_query_t packet
    size_t bytes_remaining = sizeof(p2p_image_query_t) - message.size();
    while (message.size() < bytes_remaining) {
      message += connection.read();
    }

    // Validate packet length
    if (bytes_remaining != message.size()){
      // Shouldn't be any more bytes on the wire at the end 
      // of a p2p image query packet...
      fprintf(
          stderr,
          "\tERROR: invalid image query message length. Expected: %zi, received: %zi\n",
          bytes_remaining,
          message.size()
      );
      exit(1);
    }

    // Deserialize image query packet
    p2p_image_query_t image_query;
    image_query.header = packet_header;

    memcpy(&image_query.search_id, message.c_str(), bytes_remaining);

    // Report p2p image query specifics
    fprintf(
        stderr,
        "\tImage query: peer(orig-ipv4: %u, orig-port: %u) is querying for %s with search-id: %u\n",
        image_query.orig_peer.ipv4,
        image_query.orig_peer.port,
        image_query.file_name,
        image_query.search_id
    );
   
    handleP2pImageQuery(image_query, connection, img_net); 

  } else {
    // Report invalid packet type
    fprintf(
        stderr,
        "ERROR: invalid packet type(%i) received!\n",
        packet_header.type
    );
  }
}

/**
 * runImageNetwork()
 * - Activate p2p service.
 * @param ImageNetwork : image network
 */
void runImageNetwork(
  ImageNetwork& img_net
) {

  // Fetch network elements
  const Service& img_service = img_net.getImageService();
  const Service& p2p_service = img_net.getP2pService(); 
  P2pTable& p2p_table = img_net.getP2pTable();

  // Main program event-loop 
  do {
    // Assemble fd-set and find max-fd
    fd_set rset;
    FD_ZERO(&rset);
    
    // Register p2p and img services
    FD_SET(p2p_service.getFd(), &rset);
    FD_SET(img_service.getFd(), &rset);
    int max_fd = std::max(p2p_service.getFd(), img_service.getFd());
    
    const std::unordered_set<int> peering_fds = p2p_table.getPeeringFdSet(); 

    for (int fd : peering_fds) {
      // Make new max-fd, if greater
      if (fd > max_fd) {
        max_fd = fd;
      }
      
      // Add fd to fd-set
      FD_SET(fd, &rset);
    }

    // Wait on p2p/img services and peer nodes for incoming traffic
    if (select(max_fd + 1, &rset, NULL, NULL, NULL) == -1) {
      throw SocketException("Failed while selecting sockets with traffic");
    }

    // Handle incoming connection
    if (FD_ISSET(p2p_service.getFd(), &rset)) {
      handlePeeringClient(p2p_service, p2p_table);
    }
    
    // Handle incoming connection
    if (FD_ISSET(img_service.getFd(), &rset)) {
      handleImageTraffic(img_net);
    }

    // Handle messages from peer nodes
    for (int fd : peering_fds) {
      if (FD_ISSET(fd, &rset)) {
        handleP2pTraffic(p2p_service.getPort(), fd, img_net);
      } 
    }

  } while (true);
}

int main(int argc, char** argv) {
  
  // Parse command line arguments
  size_t max_peers = PR_MAXPEERS;
  fqdn remote_fqdn; 

  if (argc == 3) {
    setCliParam(parseCliOption(*(argv + 1)), *(argv + 2), max_peers, remote_fqdn);
  } else if (argc == 5) {
    // Parse cli options
    CliOption first_cli_option = parseCliOption(*(argv + 1));
    CliOption second_cli_option = parseCliOption(*(argv + 3));
    net_assert(first_cli_option != second_cli_option, "Can't specify the same cli option twice");

    // Set cli params
    setCliParam(first_cli_option, *(argv + 2), max_peers, remote_fqdn);
    setCliParam(second_cli_option, *(argv + 4), max_peers, remote_fqdn);
  } else if (argc != 1) {
    fprintf(stderr, "Invalid cli args: ./peer [-p <fqdn>:<port>] [-n <max peers>]\n"); 
    exit(1);
  }
 
  P2pTable peer_table(max_peers);
  uint16_t port = 0;

  // Connect to peer, if instructed by user. 
  if (!remote_fqdn.name.empty()) {
    Connection server = connectToFirstPeer(remote_fqdn);
    port = server.getLocalPort();

    // Report server info
    fprintf(
        stderr,
        "Connecting to peer %s:%d\n",
        server.getRemoteDomainName().c_str(),
        server.getRemotePort() 
    );

    // Add user-specified peer to table (first one)
    peer_table.registerPendingPeer(server);
  }

  // Allow other peers to connect 
  ServiceBuilder p2p_service_builder;
  Service p2p_service = p2p_service_builder
      .setPort(port)
      .setBacklog(PR_QLEN)
      .enableAddressReuse()
      .enableLinger(PR_LINGER)
      .build();

  // Report p2p service info
  fprintf(
      stderr,
      "\nThis peer address is %s:%d\n",
      p2p_service.getDomainName().c_str(),
      p2p_service.getPort()
  );

  // Allow clients to query images and for peers
  // to deliver images
  ServiceBuilder img_service_builder;
  Service img_service =  img_service_builder
    .setBacklog(PR_QLEN)
    .enableAddressReuse()
    .enableLinger(PR_LINGER)
    .build();
  
  // Report image service info
  fprintf(
      stderr,
      "\nThis image server address is %s:%d\n",
      img_service.getDomainName().c_str(),
      img_service.getPort()
  );

  ImageNetwork img_net(
      img_service,
      p2p_service,
      peer_table);

  runImageNetwork(img_net);

  img_service.close();
  p2p_service.close();

  return 0;
}
