#pragma once

#include <string>
#include <assert.h>
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
     * Port of target server in host-byte-order.
     */
    uint16_t port_;

    /**
     * Ipv4 address of target in host-byte-order.
     */
    uint32_t ipv4Address_;
    
    /**
     * Specifies if IPv4 address is provided.
     */
    bool hasIpv4Address_;

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
    ServerBuilder& setDomainName(const std::string& domain_name);

    /**
     * setPort()
     * - Set target's port.
     * @param port : port value in host-byte-order  
     */
    ServerBuilder& setPort(uint16_t port);

    /**
     * setIpv4Address()
     * - Specify address of target in ipv4 format.
     * @param ipv4_addr : host's ipv4 address in host-byte-order
     */
    ServerBuilder& setIpv4Address(uint32_t ipv4_addr);

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
