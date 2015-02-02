#include "ImageNetwork.h"

ImageNetwork::ImageNetwork(
  const Service& img_service,
  const Service& p2p_service,
  P2pTable& p2p_table
) :
  imageService_(&img_service),
  p2pService_(&p2p_service),
  p2pTable_(&p2p_table),
  imageClient_(NULL)
{}

const Service& ImageNetwork::getImageService() const {
  return *imageService_;
}

const Service& ImageNetwork::getP2pService() const {
  return *p2pService_;
}

P2pTable& ImageNetwork::getP2pTable() {
  return *p2pTable_; 
}

bool ImageNetwork::hasImageClient() const {
  return imageClient_;
}

const Connection& ImageNetwork::getImageClient() const {
  // Fail because img-client is invalid
  assert(hasImageClient());

  return *imageClient_;
}

void ImageNetwork::setImageClient(const Connection* img_client) {
  // Failed because a new image client was added before the
  // previous one was invalidated
  assert(!hasImageClient());

  imageClient_ = img_client;
}

void ImageNetwork::invalidateImageClient() {
  delete imageClient_;
  imageClient_ = NULL;
}

void ImageNetwork::addImageQuery(const p2p_image_query_t& query_packet) {
  // Pop front if history is overflowing
  if (packetHistory_.size() == PR_MAXPEERS) {
    packetHistory_.pop_front(); 
  }

  packetHistory_.push_back(p2p_image_query_t(query_packet));
}

bool ImageNetwork::hasSeenP2pImageQuery(const p2p_image_query_t& query_packet) const {
  for (const auto& seen_packet : packetHistory_) {
    // Fail because vers and type must be validated before this
    assert(seen_packet.header.vers == query_packet.header.vers &&
        seen_packet.header.type == query_packet.header.type);
    if (seen_packet.search_id == query_packet.search_id &&
        seen_packet.orig_peer.ipv4 == query_packet.orig_peer.ipv4 &&
        seen_packet.orig_peer.port == query_packet.orig_peer.port)
    {
      return true; 
    }
  }

  return false;
}

unsigned short ImageNetwork::genSearchId() {
  return ++currMaxSearchId_;
}
