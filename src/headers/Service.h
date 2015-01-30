#pragma once

#include <string>
#include <sys/types.h>

#define MAXFQDN 256

class Service {

  private:
    /**
     * Socket file-descriptor.
     */
    int fileDescriptor_;

    /**
     * Port number for this service in network-byte-order.
     */
    u_short port_;

    /**
     * Domain name of service host.
     */
    std::string domainName_;

    /**
     * Specifies if socket should linger after closure.
     */
    bool shouldLinger_;

    /**
     * Specifies duration of socket linger.
     */
    unsigned int lingerDuration_;

  public:
    /**
     * ServerSocket()
     * - Ctor for ServerSocket.
     * @param file_descriptor : fd for socket.
     */
    explicit Service(
        int file_descriptor,
        bool should_linger,
        unsigned int linger_duration);
    
    /**
     * getFileDescriptor()
     * - Return file descriptor.
     */
    int getFileDescriptor() ;

    /**
     * getPort()
     * - Return port that this service is running on in network-byte-order.
     */
    u_short getPort() const;

    /**
     * getDomainName()
     * - Return the domain name of this host.
     */
    const std::string& getDomainName() const;

    /**
     * accept()
     * - Spawn connection for client.
     */
    const Client accept() const;

    /**
     * close()
     * - Closes socket.
     * @throws SocketException
     */
    void close() const;
}
