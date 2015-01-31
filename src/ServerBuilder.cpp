#include "ServerBuilder.h"

ServerBuilder::ServerBuilder()
    : port_(0), ipv4Address_(0), hasIpv4Address_(false), shouldReuseAddress_(false)  {}

ServerBuilder& ServerBuilder::setDomainName(const std::string& domain_name) {
  domainName_ = domain_name;
  return *this;
}

ServerBuilder& ServerBuilder::setPort(uint16_t port) {
  port_ = port;
  return *this;
}

ServerBuilder& ServerBuilder::setIpv4Address(uint32_t ipv4_addr) {
  ipv4Address_ = ipv4_addr;
  hasIpv4Address_ = true;
  return *this;
}

ServerBuilder& ServerBuilder::enableAddressReuse() {
  shouldReuseAddress_ = true;
  return *this;
}

ServerBuilder& ServerBuilder::disableAddressReuse() {
  shouldReuseAddress_ = false;
  return *this;
}

Connection ServerBuilder::build() const {
  
  // Initialize socket
  int sd = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sd == -1) {
    throw SocketException("Failed to start server socket.");
  }

  // Configure socket for address reuse
  if (shouldReuseAddress_) {
    int reuseaddr_optval = 1;
    // Configure address for reuse
    if (::setsockopt(
          sd,
          SOL_SOCKET,
          SO_REUSEADDR,
          &reuseaddr_optval,
          sizeof(int)) == -1
    ) {
      throw SocketException("Failed to configure socket for address reuse.");
    }
    
    // Configure port for reuse
    if (::setsockopt(
          sd,
          SOL_SOCKET,
          SO_REUSEPORT,
          &reuseaddr_optval,
          sizeof(int)) == -1
    ) {
      throw SocketException("Failed to configure socket for port reuse.");
    }
  }

  // Lookup peer server address
  struct sockaddr_in server;
  size_t size_server = sizeof(server);

  memset(&server, 0, size_server);
  server.sin_family = AF_INET;
  server.sin_port = htons(port_);

  // Identify target server by ipv4 address or domain name
  if (hasIpv4Address_) {
    server.sin_addr.s_addr = htonl(ipv4Address_);  
  } else {
    // Must specify 'domain name' if not using ip
    assert(!domainName_.empty());

    struct hostent *sp = ::gethostbyname(domainName_.c_str());
    memcpy(&server.sin_addr, sp->h_addr, sp->h_length);
  }

  // Connect to peer server
  if (::connect(sd, (struct sockaddr *) &server, size_server) == -1) {
    throw SocketException("Failed to connect to peer server: " + domainName_);
  }

  return Connection(sd);
}
