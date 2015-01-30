#pragma once

#include <string>
#include <unistd.h>        // getopt(), STDIN_FILENO, gethostname()
#include <signal.h>        // signal()
#include <netdb.h>         // gethostbyname(), gethostbyaddr()
#include <netinet/in.h>    // struct in_addr
#include <arpa/inet.h>     // htons(), inet_ntoa()
#include <sys/types.h>     // u_short
#include <sys/socket.h>    // socket API, setsockopt(), getsockname()
#include <sys/select.h>    // select(), FD_*

#include "Connection.h"

class ServerBuilder {

  private:
    /**
     * Domain name of target.
     */
    std::string domainName_;

    /**
     * Port of target server in network-byte-order.
     */
    u_short port_;

    /**
     * Indicates whether the socket's address may be reused or not.
     */
    bool shouldReuseAddress_;

  public:
    /**
     * ServerBuilder()
     * - Ctor for ServerBuilder.
     */
    ServerBuilder();

    /**
     * setDomainName()
     * - Establish domain name.
     */
    ServerBuilder& setDomainName(const std::string domain_name);

    /**
     * setPort()
     * - Establish port in network-byte-order. 
     */
    ServerBuilder& setPort(u_short port);

    /**
     * enableAddressReuse()
     * - Configure socket for address reuse.
     */
    ServerBuilder& enableAddressReuse();

    /**
     * disableAddressReuse()
     * - Configure socket to prevent address reuse.
     */
    ServerBuilder& disableAddressReuse();

    /**
     * build()
     * - Construct Connection from ServerBuilder.
     */
    Connection build() const;
};
