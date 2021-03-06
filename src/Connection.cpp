#include "Connection.h"

Connection::Connection(
    int file_descriptor
) : 
    fileDescriptor_(file_descriptor)
{
  char hostname[BUFFER_SIZE+1];
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  
  // Fetch local host domain name     
  memset(hostname, 0, BUFFER_SIZE+1);
  if (::gethostname(hostname, BUFFER_SIZE) == -1) {
    throw SocketException("Failed to determine domain name of localhost");
  }

  localDomainName_ = std::string(hostname);
  
  // Fetch local host port number
  if (::getsockname(fileDescriptor_, (struct sockaddr *) &addr, &addr_len) == -1) {
    throw SocketException("Failed to determine local host port");
  }

  localPort_ = ntohs(addr.sin_port);
  localIpv4_ = ntohl(addr.sin_addr.s_addr);

  // Fetch remote host port 
  if (::getpeername(fileDescriptor_, (struct sockaddr *) &addr, &addr_len) == -1) {
    throw SocketException("Failed to lookup remote address.");
  }

  remotePort_ = ntohs(addr.sin_port);
  remoteIpv4_ = ntohl(addr.sin_addr.s_addr);

  // Determine remote host domain name
  memset(hostname, 0, BUFFER_SIZE); 
  if (::getnameinfo(
        (struct sockaddr *) &addr,
        addr_len,
        hostname,
        BUFFER_SIZE,
        NULL,
        0,
        0) == -1) {
    throw SocketException("Failed to fetch remote host domain name");
  } 

  remoteDomainName_ = std::string(hostname);
}

int Connection::getFd() const {
  return fileDescriptor_;
}

const std::string Connection::read() const {
  char buffer[BUFFER_SIZE];
  int bytes_read = ::recv(fileDescriptor_, buffer, BUFFER_SIZE, 0);
  if (bytes_read == -1) {
    throw SocketException("Failed to read from socket.");
  }

  return std::string(buffer, bytes_read);
}

size_t Connection::write(const std::string data) const {
  int bytes_sent = ::send(fileDescriptor_, data.c_str(), data.size(), 0);
  if (bytes_sent == -1) {
    throw SocketException("Bad write to socket");
  }

  return data.size() - bytes_sent;
}

uint16_t Connection::getLocalPort() const {
  return localPort_;
}

uint16_t Connection::getRemotePort() const {
  return remotePort_;
}

const std::string& Connection::getLocalDomainName() const {
  return localDomainName_;
}

const std::string& Connection::getRemoteDomainName() const {
  return remoteDomainName_;
}

void Connection::close() const {
  if (::close(fileDescriptor_) == -1) {
    throw SocketException("Failed to close socket");
  }
}

bool Connection::operator==(const Connection& other) const {
  return remotePort_ == other.remotePort_ &&
    localPort_ == other.localPort_ &&
    remoteDomainName_ == other.remoteDomainName_ &&
    localDomainName_ == other.localDomainName_ &&
    localIpv4_ == other.localIpv4_ &&
    remoteIpv4_ == other.remoteIpv4_;
}

bool Connection::operator!=(const Connection& other) const {
  return !(*this == other); 
}

uint32_t Connection::getLocalIpv4() const {
  return localIpv4_;
}

uint32_t Connection::getRemoteIpv4() const {
  return remoteIpv4_;
}
