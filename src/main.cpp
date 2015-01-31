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

#define PR_QLEN 10 

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
 * FQDN fields.
 */
typedef struct {
  std::string name;     // host domain name 
  u_short port;    // port in network byte order
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
    fprintf(stderr, "Malformed FQDN. Must be of the form: <fqdn>:<port>"); 
    exit(1);
  }
 
  // Configure fqdn
  remote.name = fqdn_name_with_port.substr(0, delim_idx);
  remote.port = (u_short) htons(atoi(fqdn_name_with_port.substr(delim_idx + 1).c_str()));

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
      fprintf(stderr, "Invalid cli flag");
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
      fprintf(stderr, "Bad CliOption enum value");
      exit(1);
  }
}

/**
 * spawnPeerListener()
 * - Initialize service to listen for peering requests. If a peer 
 *     was not specified in the cli params, then 'port' should be 0
 *     and the socket should be initialized with an ephemeral port.
 * @param port : port number in network-byte-order
 */
Service spawnPeerListener(u_short port) {
  ServiceBuilder builder; 
  return builder
      .setPort(port)
      .enableAddressReuse()
      .setBacklog(PR_QLEN)
      .build();
}

/**
 * connectToPeer()
 * - Establish connection to remote.
 * @peer remote_fqdn : fqdn of target peer 
 */
Connection connectToPeer(const fqdn& remote_fqdn) {
  ServerBuilder builder;
  return builder
      .setDomainName(remote_fqdn.name)
      .setPort(remote_fqdn.port)
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
  autojoin_msg join_msg;
  join_msg.vers = PM_VERS;
  join_msg.type = message_type;

  size_t join_header_len = sizeof(join_msg);
  std::string message((char *) &join_msg, join_header_len); 

  size_t num_join_peers = 
      p2p_table.getNumPeers() < MAX_RPEERS 
          ? p2p_table.getNumPeers() 
          : MAX_RPEERS;

  std::unordered_set<int> peering_fds = p2p_table.getPeeringFdSet();
  auto iter = peering_fds.begin();

  for (size_t i = 0; i < num_join_peers; ++i) {
    // Fail because we ran out of fds
    assert(iter != peering_fds.end());

    int fd = *iter++;
    const Connection& connection = p2p_table.fetchConnectionByFd(fd);

    autojoin_peer peer = 
        {connection.getRemoteIpv4(), (short) connection.getRemotePort(), 0};
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
        ntohs(connection.getRemotePort())
    ); 
    
    connection.close();
    return;
  } 
  
  // Notify user of new peer
  fprintf(
      stderr,
      "Connected from %s:%d\n",
      connection.getRemoteDomainName().c_str(),
      ntohs(connection.getRemotePort())
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
        ntohs(connection.getRemotePort())
    ); 

    connection.close();
    return;
  } 
  
  // Notify user of redirected peer
  fprintf(
      stderr,
      "Peer table full: %s:%d redirected\n",
      connection.getRemoteDomainName().c_str(),
      ntohs(connection.getRemotePort())
  ); 

  connection.close();
}

/**
 * handleJoinRequest()
 * - Accept peering request, if space available. Redirect, otherwise.
 * @param service : service listening for incomming connections
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
 * runP2pService()
 * - Activate p2p service.
 */
void runP2pService(const Service& service, P2pTable& p2p_table) {
  
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

    // Wait on peer nods for incomming traffic
    if (select(max_fd + 1, &rset, NULL, NULL, NULL) == -1) {
      throw SocketException("Failed while waiting on peer nodes");
    }

    // Handle incomming connection
    if (FD_ISSET(service.getFd(), &rset)) {
      handleJoinRequest(service, p2p_table);
    }

    // Handle messages from peer nodes
    for (int fd : peering_fds) {
      if (FD_ISSET(fd, &rset)) {
     // TODO this stuff 
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
    fprintf(stderr, "Invalid cli args: ./peer [-p <fqdn>:<port>] [-n <max peers>]"); 
    exit(1);
  }
 
  P2pTable peer_table(max_peers);
  u_short port = 0;

  // Connect to peer, if instructed by user. 
  if (!remote_fqdn.name.empty()) {
    Connection server = connectToPeer(remote_fqdn);
    port = server.getRemotePort();

    // Report server info
    fprintf(
        stderr,
        "Connected to peer %s:%d\n",
        server.getRemoteDomainName().c_str(),
        ntohs(port)
    );

    // Add user-specified peer to table (first one)
    peer_table.registerPendingPeer(server);
  }

  // Allow other peers to connect 
  Service service = spawnPeerListener(port);

  // Report service info
  fprintf(
      stderr,
      "This peer address is %s:%d/n",
      service.getDomainName().c_str(),
      ntohs(service.getPort())
  );

  runP2pService(service, peer_table);

  service.close();

  return 0;
}
