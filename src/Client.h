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

class Connection {

  private:
    /**
     * File descriptor for this socket.
     */
   int fileDescriptor;
    
    /**
     * Port that client is connecting on.
     */
    u_short port;

    /**
     * Address of connection.
     */
    struct in_addr address;

    /**
     * close()
     * - Closes socket.
     * @throws SocketException
     */
    void close() const;

  public:  
    /**
     * Connection()
     * - Ctor for Connection.
     */
    Connection(int file_descriptor, u_short port, struct in_addr address);

    /**
     * ~Connection()
     * - Dtor for Connection. Closes socket connection.
     */
    ~Connection();

    /**
     * read()
     * - Read from the socket, return data as string.
     * - Returns empty string if there is no more data to read. 
     * @return string : data read from the socket
     * @throws SocketException
     */
    const std::string read() const;

    /**
     * write()
     * - Write data to socket.
     * @param data : string of data
     * @return int : number of unsent bytes 
     * @throws SocketException
     */
    int write(const std::string data) const; 
    
    /**
     * getPort()
     * - Return port that client connected on.
     */
    u_short getPort() const;
   
    /**
     * getAddress()
     * - Return read-only reference to address.
     */
    const in_addr& getAddress() const;

    /**
     * getFileDescriptor()
     * - Returns the file descriptor for this connected socket.
     */
    int getFileDescriptor() const;
}
