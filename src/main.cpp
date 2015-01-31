#include <string>
#include <stdio.h>         // fprintf(), perror(), fflush()
#include <stdlib.h>        // atoi()
#include <assert.h>        // assert()
#include <sys/types.h>     // u_short
#include <arpa/inet.h>     // htons(), inet_ntoa()
#include <iostream>

#include "packets.h"
#include "Service.h"
#include "ServiceBuilder.h"
#include "ServerBuilder.h"
#include "Connection.h"
#include "SocketException.h"
#include "P2pTable.h"

#define PR_MAXPEERS 6
#define PR_MAXFQDN 256

#define PR_ADDRESS_FLAG 'p'
#define PR_PORT_DELIMITER ':'
#define PR_MAXPEERS_FLAG 'n'
#define PR_CLI_OPT_PREFIX '-'

#define PR_QLEN   10 
#define PR_LINGER 2

#define PM_VERS 0x1

#define net_assert(err, errmsg) { if ((!err)) { perror(errmsg); assert((err)); } }

/**
 * Message type codes.
 */
enum MessageType {
  WELCOME  = 0x1,
  REDIRECT = 0x2
};

/**
 * Cli option types.
 */
enum CliOption {P, N};

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
  message_header join_header;
  join_header.vers = PM_VERS;
  join_header.type = message_type;

  size_t num_join_peers = 
      p2p_table.getNumPeers() < MAX_RPEERS 
          ? p2p_table.getNumPeers() 
          : MAX_RPEERS;
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

    fprintf(
        stderr,
        "Here's a peer to join with: %s:%d\n",
        connection.getRemoteDomainName().c_str(),
        connection.getRemotePort()
    );

    peer_addr peer = 
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
      "Connected from %s:%d\n",
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
 * handleJoinRequest()
 * - Accept peering request, if space available. Redirect, otherwise.
 * @param service : service listening for incoming connections
 * @param p2p_table : peer node registry
 */
void handleJoinRequest(const Service& service, P2pTable& p2p_table) {
 
  // Accept connection
  const Connection connection = service.accept();

  if (p2p_table.isFull()) {
    // This node is saturated, redurect client peer
    redirectPeer(connection, p2p_table);
  } else {
    // This node is unsaturated, accept the peering request 
    acceptPeerRequest(connection, p2p_table);
  }
}

/**
 * handleIncomingMessage()
 * - Process message from peer.
 * @param service_port : port that service is running on (host-byte-order).
 *    All outgoing connections should bind to this local port.
 * @param fd : socket fd
 * @param p2p_table : peer node registry
 */
void handleIncomingMessage(uint16_t service_port, int fd, P2pTable& p2p_table) {
  
  // Read header from peer
  const Connection& connection = p2p_table.fetchConnectionByFd(fd);

  fprintf(
      stderr,
      "Received ack from %s:%d\n",
      connection.getRemoteDomainName().c_str(),
      connection.getRemotePort()
  );
  
  message_header header;
  size_t message_header_len = sizeof(header);
  std::string message;
  
  while (message.size() < message_header_len) {
    message += connection.read();
  }

  // Deserialize message and transform fields to host-byte-order
  memcpy(&header, message.c_str(), message_header_len);
  header.num_peers = header.num_peers;

  // Read auto-join nodes from peer
  if (header.num_peers) {
    // Fail due to negative 'num-peers'
    assert(header.num_peers > 0);
    
    size_t peer_addr_size = sizeof(peer_addr);
    size_t message_body_len = peer_addr_size * header.num_peers;
    
    message = message.substr(message_header_len);
    
    while (message.size() != message_body_len) {
      message += connection.read();
    }

    const char* message_cstr = message.c_str();
    std::vector<peer_addr> peers_to_join;
    peers_to_join.reserve(header.num_peers);
   
    // Bind outgoing connections to the same port that this node
    // is lisetening on
    ServerBuilder server_builder;
    server_builder
      .setLocalPort(service_port)
      .enableAddressReuse();

    fprintf(stderr, "\twhich is peered with %i peers:\n", header.num_peers);

    // Deserialize message body
    for (size_t i = 0; i < message_body_len; i += peer_addr_size) {
      peer_addr peer;
      memcpy(&peer, message_cstr + i, peer_addr_size);
      peers_to_join.push_back(peer);

      // Fetch domain name of recommended peer
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
        throw SocketException("Failed to fetch remote host domain name"); 
      }
      
      // Report join peer
      fprintf(
          stderr,
          "\t\t%s:%d\n",
          domain_name,
          peer.port
      );
    }

    // Attempt to saturate peer table with recommended peers
    size_t num_available_peers = p2p_table.getMaxPeers() - p2p_table.getNumPeers();
    size_t num_peers_to_process = 
        (num_available_peers < header.num_peers)
            ? num_available_peers
            : header.num_peers;

    for (size_t i = 0; i < num_peers_to_process; ++i) {
      const peer_addr peer = peers_to_join[i];

      // Connect to peer
      Connection connection = server_builder
          .setRemoteIpv4Address(peer.ipv4)
          .setRemotePort(peer.port)
          .build();
      
      // Put connection in p2p table in pending state
      p2p_table.registerPendingPeer(connection);
    }
  }
}

/**
 * runP2pService()
 * - Activate p2p service.
 * @param service : service listening for incoming connections
 * @param p2p_table : peer node registry
 */
void runP2pService(const Service& service, P2pTable& p2p_table) {
  
  // Main program event-loop 
  do {
    // Compose fd-set and find max-fd
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(service.getFd(), &rset);
    
    int max_fd = service.getFd();
    const std::unordered_set<int> peering_fds = p2p_table.getPeeringFdSet(); 

    for (int fd : peering_fds) {
      // Make new max-fd, if greater
      if (fd > max_fd) {
        max_fd = fd;
      }
      
      // Add fd to fd-set
      FD_SET(fd, &rset);
    }

    // Wait on peer nods for incoming traffic
    if (select(max_fd + 1, &rset, NULL, NULL, NULL) == -1) {
      throw SocketException("Failed while waiting on peer nodes");
    }

    // Handle incoming connection
    if (FD_ISSET(service.getFd(), &rset)) {
      handleJoinRequest(service, p2p_table);
    }

    // Handle messages from peer nodes
    for (int fd : peering_fds) {
      if (FD_ISSET(fd, &rset)) {
        handleIncomingMessage(service.getPort(), fd, p2p_table);
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
        "Connected to peer %s:%d\n",
        server.getRemoteDomainName().c_str(),
        server.getRemotePort() 
    );

    // Add user-specified peer to table (first one)
    peer_table.registerPendingPeer(server);
  }

  // Allow other peers to connect 
  ServiceBuilder service_builder;
  Service service = service_builder
      .setPort(port)
      .setBacklog(PR_QLEN)
      .enableAddressReuse()
      .enableLinger(PR_LINGER)
      .build();

  // Report service info
  fprintf(
      stderr,
      "This peer address is %s:%d\n",
      service.getDomainName().c_str(),
      service.getPort()
  );

  runP2pService(service, peer_table);

  service.close();

  return 0;
}
