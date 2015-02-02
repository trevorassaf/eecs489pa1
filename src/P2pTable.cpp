#include "P2pTable.h"

P2pTable::P2pTable(
    size_t size
) : 
    peerTable_(size),
    maxPeers_(size)
{}

bool P2pTable::isRejectedPeer(const Connection& connection) const {
  return isRejectedPeer(connection.getRemoteIpv4(), connection.getRemotePort());
}

const P2pTable::ConnectionInfo* P2pTable::fetchConnectionInfoByRemoteAddress(
  uint32_t ipv4,
  uint16_t port
) const {
 
  for (auto c_info : peerTable_) {
    if (c_info.second.connection.getRemoteIpv4() == ipv4 &&
        c_info.second.connection.getRemotePort() == port)
    {
      return &peerTable_.at(c_info.first);    
    }
  }

  return NULL;
}

bool P2pTable::isConnectedPeer(int peer_fd) const {
  return peerTable_.count(peer_fd) && !peerTable_.at(peer_fd).is_pending;
}

bool P2pTable::isPendingPeer(int peer_fd) const {
  return peerTable_.count(peer_fd) && peerTable_.at(peer_fd).is_pending;
}

bool P2pTable::hasRemote(uint32_t ipv4, uint16_t port) const {
  // Check table of connected/pending peers
  for (auto c_info : peerTable_) {
    if (c_info.second.connection.getRemoteIpv4() == ipv4 &&
        c_info.second.connection.getRemotePort() == port) 
    {
      return true;     
    }
  } 

  // Finally, check set of rejected peers
  return isRejectedPeer(ipv4, port);
}

bool P2pTable::isRejectedPeer(uint32_t ipv4, uint16_t port) const {
  for (auto remote_address : rejectedPeerList_) {
    if (remote_address.ipv4 == ipv4 &&
        remote_address.port == port)
    {
      return true;
    }
  }

  return false;
}

bool P2pTable::isConnectedPeer(uint32_t ipv4, uint16_t port) const {
  const ConnectionInfo* c_info = fetchConnectionInfoByRemoteAddress(ipv4, port);
  return c_info && !c_info->is_pending;
}

bool P2pTable::isPendingPeer(uint32_t ipv4, uint16_t port) const {
  const ConnectionInfo* c_info = fetchConnectionInfoByRemoteAddress(ipv4, port);
  return c_info && c_info->is_pending;
}

void P2pTable::registerPendingPeer(const Connection& peer) {
  // Fail because 'peer' is already registered in this table.
  assert(!isConnectedPeer(peer.getFd()) && 
      !isPendingPeer(peer.getFd()) && 
      !isRejectedPeer(peer));

  peerTable_.emplace(peer.getFd(), ConnectionInfo{peer, true});
}

void P2pTable::registerConnectedPeer(const Connection& peer) {
  // 'peer' must be in pending state before transitioning to connected state
  assert(!isRejectedPeer(peer) &&
      !isConnectedPeer(peer.getFd()));

  // Transfer peer, if already in map, else, make a new entry in map
  if (isPendingPeer(peer.getFd())) {
    registerConnectedPeer(peer.getFd());
  } else {
    peerTable_.emplace(peer.getFd(), ConnectionInfo{peer, false});
  }
}

void P2pTable::registerConnectedPeer(int peer_fd) {
  // 'peer' must be in pending state before transitioning to connected state
  assert(isPendingPeer(peer_fd));
 
  ConnectionInfo& c_info = peerTable_.at(peer_fd);

  // Fail because the peer is registered as rejected when it should
  // be pending
  assert(!isRejectedPeer(c_info.connection));

  c_info.is_pending = false;
}

void P2pTable::registerRejectedPeer(int peer_fd) {
  // 'peer' must be in pending state before transitioning to rejected state
  assert(isPendingPeer(peer_fd));

  ConnectionInfo& c_info = peerTable_.at(peer_fd);

  // Fail because the peer is registered as rejected when it should
  // be pending
  assert(!isRejectedPeer(c_info.connection));

  // Pop first element if list is full
  if (rejectedPeerList_.size() == MAX_REJECTED) {
    rejectedPeerList_.pop_front(); 
  }

  // Push new rejected peer
  rejectedPeerList_.push_back(
      RemoteAddress{
          c_info.connection.getRemoteIpv4(),
          c_info.connection.getRemotePort()
      }
  );

  peerTable_.erase(peer_fd);
}

size_t P2pTable::getMaxPeers() const {
  return maxPeers_;
}

size_t P2pTable::getNumPeers() const {
  return peerTable_.size();
}

bool P2pTable::isFull() const {
  return peerTable_.size() == maxPeers_;
}

const std::unordered_set<int> P2pTable::getPeeringFdSet() const {
  std::unordered_set<int> fd_set;
  for (auto connection_info : peerTable_) {
    // Fail due due to duplicate fd
    assert(!fd_set.count(connection_info.second.connection.getFd()));
    
    fd_set.emplace(connection_info.second.connection.getFd());
  }
  
  return fd_set; 
}

const Connection& P2pTable::fetchConnectionByFd(int fd) const {
  // Fail because the peer indicated by 'fd' is not in the
  // peer table
  assert(peerTable_.count(fd));

  return peerTable_.at(fd).connection;
}

const std::vector<const Connection*> P2pTable::fetchConnectedPeers() const {
  std::vector<const Connection*> connected_peers;
  
  for (const auto& peer_table_entry : peerTable_) {
    if (!peer_table_entry.second.is_pending) {
      connected_peers.push_back(&(peer_table_entry.second.connection));
    }
  }

  return connected_peers;
}
