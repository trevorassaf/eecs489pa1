#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <assert.h>
#include <sys/select.h>    // select(), FD_*

#include "Connection.h"

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
     * Table of peers that are connected or have pending connections. The
     * bit is true iff the connection is pending.
     */
   std::unordered_map<int, ConnectionInfo> peerTable_;

   /**
    * Set of peers that have rejected requests by this node to join. 
    */
   std::unordered_map<int, const Connection> rejectedPeerTable_;

   /**
    * Maximum number of peers allowed in the peer table. 
    */
   size_t maxPeers_;

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
    * isRejectedPeer()
    * - Return true iff this node has been rejected previously by the
    *   specified peer.
    */
    bool isRejectedPeer(int peer_fd) const;

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
