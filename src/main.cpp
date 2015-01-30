#include <string>
#include <assert.h>        // assert()
#include <sys/types.h>     // u_short
#include <arpa/inet.h>     // htons(), inet_ntoa()
#include <iostream>

#include "Service.h"
#include "ServiceBuilder.h"
#include "ServerBuilder.h"
#include "Connection.h"
#include "SocketException.h"

#define PR_MAXPEERS 6
#define PR_MAXFQDN 256

#define PR_ADDRESS_FLAG 'p'
#define PR_PORT_DELIMITER ':'
#define PR_MAXPEERS_FLAG 'n'
#define PR_CLI_OPT_PREFIX '-'

#define PR_QLEN 10 

#define net_assert(err, errmsg) { if ((!err)) { perror(errmsg); assert((err)); } }

enum CliOption {P, N};

typedef struct {
  std::string name;     // host domain name 
  u_short port;    // port in network byte order
} fqdn;

/**
 *  parseMaxPeers()
 *  - Parse max-peers cli param.
 */
void parseMaxPeers(char* cli_input, unsigned int& max_peers) {
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
void setCliParam(CliOption opt, char* cli_param_str, unsigned int& max_peers, fqdn& remote) {
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
 * runP2pService()
 * - Activate p2p service.
 */
void runP2pService(const Service& service, P2pTable& p2p_table) {
  do {
    
  } while (true);
}

int main(int argc, char** argv) {
  
  // Parse command line arguments
  unsigned int max_peers = PR_MAXPEERS;
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

    // TODO add this to the table of peers
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
