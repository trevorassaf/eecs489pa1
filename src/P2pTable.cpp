#include "P2pTable.h"

P2pTable::P2pTable(
    size_t size
) : 
    peerTable_(size),
    rejectedPeerTable_(),
    maxPeers_(size)
{}

bool P2pTable::isConnectedPeer(int peer_fd) const {
  return peerTable_.count(peer_fd) && !peerTable_.at(peer_fd).is_pending;
}

bool P2pTable::isPendingPeer(int peer_fd) const {
  return peerTable_.count(peer_fd) && peerTable_.at(peer_fd).is_pending;
}

bool P2pTable::isRejectedPeer(int peer_fd) const {
  return rejectedPeerTable_.count(peer_fd);
}

void P2pTable::registerPendingPeer(const Connection& peer) {
  // Fail because 'peer' is already registered in this table.
  assert(!isConnectedPeer(peer.getFd()) && 
      !isPendingPeer(peer.getFd()) && 
      !isRejectedPeer(peer.getFd()));

  peerTable_.emplace(peer.getFd(), ConnectionInfo{peer, true});
}

void P2pTable::registerConnectedPeer(const Connection& peer) {
  // 'peer' must be in pending state before transitioning to connected state
  assert(!isRejectedPeer(peer.getFd()) &&
      !isConnectedPeer(peer.getFd()));

  peerTable_.emplace(peer.getFd(), ConnectionInfo{peer, false});
}

void P2pTable::registerConnectedPeer(int peer_fd) {
  // 'peer' must be in pending state before transitioning to connected state
  assert(isPendingPeer(peer_fd) &&
      !isRejectedPeer(peer_fd) &&
      !isConnectedPeer(peer_fd));

  peerTable_.at(peer_fd).is_pending = false;
}

void P2pTable::registerRejectedPeer(int peer_fd) {
  // 'peer' must be in pending state before transitioning to rejected state
  assert(isPendingPeer(peer_fd) &&
      !isRejectedPeer(peer_fd) &&
      !isConnectedPeer(peer_fd));

  rejectedPeerTable_.emplace(peer_fd, peerTable_.at(peer_fd).connection);
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
