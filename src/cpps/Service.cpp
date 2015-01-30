#include <Service.h>
#include <SocketException.h>

#include <string.h>        // memset(), memcmp(), strlen(), strcpy(), memcpy()
#include <unistd.h>        // getopt(), STDIN_FILENO, gethostname()
#include <signal.h>        // signal()
#include <netdb.h>         // gethostbyname(), gethostbyaddr()
#include <netinet/in.h>    // struct in_addr
#include <arpa/inet.h>     // htons(), inet_ntoa()
#include <sys/types.h>     // u_short
#include <sys/socket.h>    // socket API, setsockopt(), getsockname()
#include <sys/select.h>    // select(), FD_*

Service::Service(
    int file_descriptor,
    bool should_linger,
    unsigned int linger_duration
) : 
    fileDescriptor_(file_descriptor),
    shouldLinger_(should_linger),
    lingerDuration_(linger_duration) {
  
  // Initialize socket information for service
  struct sockaddr_in sin;
  socklen_t sin_len = sizeof(sin);
  if (getsockname(fileDescriptor_, (struct sockaddr *) &sin, &sin_len) == -1) {
    throw SocketException("Failed to fetch socket information in 'getsockname'");
  }

  port_ = sin.sin_port;
  
  char hostname_buff[MAXFQDN + 1];
  memset(hostname_buff, 0, MAXFQDN);
  if (gethostname(hostname_buff, MAXFQDN) == -1) {
    throw SocketException("Failed to fetch name of this host.");
  }

  domainName_ = std::string(hostname_buff);
}

int Service::getFileDescriptor() const {
  return fileDescriptor_;
}

u_short Service::getPort() const {
  return port_;
}

const std::string& Service::getDomainName() const {
  return domainName_;
}

const Client Service::accept() const {
  // Accept incoming connection
  struct sockaddr_in peer;
  socklen_t len = sizeof(sockaddr_in);

  int sd = accept(fileDescriptor_, (struct sockaddr *) &peer, &len);
  if (sd == -1) {
    throw SocketException("Failed to accept client connection.");
  }
 
  // Configure socket to linger
  if (shouldLinger_) {
    struct linger so_linger = {true, lingerDuration_}; 
    if (setsockopt(sd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)) == -1) {
      throw SocketException("Failed to configure socket for linger.");
    }
  }

  return Client(sd);
}

void Service::close() const {
  closesocket(fileDescriptor_); 
}
