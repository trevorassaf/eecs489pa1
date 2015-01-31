#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <assert.h>
#include <sys/select.h>    // select(), FD_*

#include "Connection.h"

#define MAX_REJECTED 6

class P2pTable {

  private:
   
   /**
    * Bookkeeping data for Connection
    */
   struct ConnectionInfo {
     const Connection connection;
     bool is_pending;
   };

   /**
    * Address of rejected remote peers.
    */
   struct RemoteAddress {
    uint32_t ipv4;
    uint16_t port;
   };

    /**
     * Table of peers that are connected or have pending connections. The
     * bit is true iff the connection is pending.
     */
   std::unordered_map<int, ConnectionInfo> peerTable_;

   /**
    * Set of peers that have rejected requests by this node to join. 
    * The maximum length of this list is MAX_REJECTED. Operates as a
    * queue, in which the first element is popped out when the list
    * saturates and a new element is added.
    */
   std::deque<RemoteAddress> rejectedPeerList_;

   /**
    * Maximum number of peers allowed in the peer table. 
    */
   size_t maxPeers_;

   /**
    * isRejectedPeer()
    * - Return true iff this node has been rejected previously by the
    *   specified peer.
    * @param connection : connection to search for   
    */
    bool isRejectedPeer(const Connection& connection) const;

    /**
     * fetchConnectionInfoByRemoteAddress()
     * - Return connection-info of the specified remote address.
     * @param ipv4 : ipv4 address of remote (host byte order)  
     * @param port : port of remote (host byte order)  
     */
    const ConnectionInfo* fetchConnectionInfoByRemoteAddress(uint32_t ipv4, uint16_t port) const;

  public:
    /**
     * P2pTable()
     * - Create P2pTable with max size 'size'
     * @param size : max size of peerTable_
     */
    explicit P2pTable(size_t size);

   /**
    * isConnectedPeer()
    * - Return true iff this node has connected to the specified peer.
    */
   bool isConnectedPeer(int peer_fd) const;

   /**
    * isPendingPeer()
    * - Return true iff this node has a pending connection with the
    *   specified peer.
    */
   bool isPendingPeer(int peer_fd) const;

   /**
    * hasRemote()
    * - Returns true iff the table constins the specified remote address.
    * @param ipv4 : ipv4 address of remote (host byte order)  
    * @param port : port of remote (host byte order)  
    */
   bool hasRemote(uint32_t ipv4, uint16_t port) const;

   /**
    * isRejectedPeer()
    * - Return true iff this node has been rejected previously by the
    *   specified peer.
    * @param ipv4 : ipv4 address of remote (host byte order)  
    * @param port : port of remote (host byte order)  
    */
    bool isRejectedPeer(uint32_t ipv4, uint16_t port) const;
   
    /**
     * isConnectedPeer()
     * - Return true iff the local node is connected to the
     *   specified remote.
     * @param ipv4 : ipv4 address of remote (host byte order)  
     * @param port : port of remote (host byte order)  
     */
    bool isConnectedPeer(uint32_t ipv4, uint16_t port) const;
    
    /**
     * isPendingPeer()
     * - Return true iff the local node has a pending connection
     *   with the specified remote.
     * @param ipv4 : ipv4 address of remote (host byte order)  
     * @param port : port of remote (host byte order)  
     */
    bool isPendingPeer(uint32_t ipv4, uint16_t port) const;

    /**
     * registerPendingPeer()
     * - Add peer to peer table in pending state.
     */
    void registerPendingPeer(const Connection& peer);
    
    /**
     * registerConnectedPeer()
     * - Add peer to peer table in connected state.
     */
    void registerConnectedPeer(const Connection& peer);

    /**
     * registerConnectedPeer()
     * - Transfer peer from 'pending' to 'connected'
     */
    void registerConnectedPeer(int peer_fd);

    /**
     * registerRejectedPeer()
     * - Transfer peer from 'pending' to 'rejected'
     */
    void registerRejectedPeer(int peer_fd);

    /**
     * getMaxPeers()
     * - Return the maximum number of connected/pending peers
     *   allowed by this table.
     */
    size_t getMaxPeers() const;

    /**
     * getNumPeers()
     * - Return the number of connected/pending peers in
     *   this table.
     */
    size_t getNumPeers() const;

    /**
     * isFull()
     * - Return true iff peer table is saturated.
     */
    bool isFull() const;
      
    /**
     * waitForPeers()
     * - Wait for incomming network traffic from known peers.
     * @return list of Connections with traffic
     */
    const std::unordered_set<int> getPeeringFdSet() const; 

    /**
     * fetchConnectionByFd()
     * - Return ref to Connection linked to 'fd'
     */
    const Connection& fetchConnectionByFd(int fd) const;

};
