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
