#include "ServerBuilder.h"

ServerBuilder::ServerBuilder()
    : port_(0), shouldReuseAddress_(false) {}

ServerBuilder& ServerBuilder::setDomainName(const std::string domain_name) {
  domainName_ = domain_name;
  return *this;
}

ServerBuilder& ServerBuilder::setPort(u_short port) {
  port_ = port;
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
  int sd = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sd == -1) {
    throw SocketException("Failed to start server socket.");
  }

  // Configure socket for address reuse
  if (shouldReuseAddress_) {
    int reuseaddr_optval = 1;
    if (::setsockopt(
          sd, SOL_SOCKET, SO_REUSEADDR,
          &reuseaddr_optval,
          sizeof(int)) == -1) {
      throw SocketException("Failed to configure socket for lingering.");
    }
  }

  // Lookup peer server address
  struct sockaddr_in server;
  size_t size_server = sizeof(server);

  memset(&server, 0, size_server);
  server.sin_family = AF_INET;
  server.sin_port = port_;

  struct hostent *sp = ::gethostbyname(domainName_.c_str());
  memcpy(&server.sin_addr, sp->h_addr, sp->h_length);

  // Connect to peer server
  if (::connect(sd, (struct sockaddr *) &server, size_server) == -1) {
    throw SocketException("Failed to connect to peer server: " + domainName_);
  }

  return Connection(sd);
}
