#include "Service.h"

Service::Service(
    int file_descriptor
) : 
    fileDescriptor_(file_descriptor),
    shouldLinger_(false),
    lingerDuration_(0)
{ 
  initService(); 
}

Service::Service(
    int file_descriptor, 
    unsigned int linger_duration
) :
    fileDescriptor_(file_descriptor),
    shouldLinger_(true),
    lingerDuration_(linger_duration)
{
  initService();      
}

void Service::initService() {
  // Initialize socket information for service
  struct sockaddr_in sin;
  socklen_t sin_len = sizeof(sin);
  if (::getsockname(fileDescriptor_, (struct sockaddr *) &sin, &sin_len) == -1) {
    throw SocketException("Failed to fetch socket information in 'getsockname'");
  }

  port_ = ntohs(sin.sin_port);
  
  char hostname_buff[MAXFQDN + 1];
  memset(hostname_buff, 0, MAXFQDN);
  if (::gethostname(hostname_buff, MAXFQDN) == -1) {
    throw SocketException("Failed to fetch name of this host.");
  }

  domainName_ = std::string(hostname_buff);
}

int Service::initSocket() const {
  // Accept incoming connection
  struct sockaddr_in peer;
  socklen_t len = sizeof(sockaddr_in);

  int sd = ::accept(fileDescriptor_, (struct sockaddr *) &peer, &len);
  if (sd == -1) {
    throw SocketException("Failed to accept client connection.");
  }
 
  // Configure socket to linger
  if (shouldLinger_) {
    struct linger so_linger = {true, (int) lingerDuration_}; 
    if (::setsockopt(
          sd,
          SOL_SOCKET,
          SO_LINGER,
          &so_linger,
          sizeof(so_linger)) == -1
    ) {
      throw SocketException("Failed to configure socket for linger.");
    }
  }

  return sd;
}

int Service::getFd() const {
  return fileDescriptor_;
}

u_short Service::getPort() const {
  return port_;
}

const std::string& Service::getDomainName() const {
  return domainName_;
}

const Connection Service::accept() const {
  int fd = initSocket();
  return Connection(fd);
}

const Connection* Service::acceptNew() const {
  int fd = initSocket();
  return new Connection(fd);
}

void Service::close() const {
  if (::close(fileDescriptor_) == -1) {
    throw SocketException("Failed to clost socket");
  }
}
