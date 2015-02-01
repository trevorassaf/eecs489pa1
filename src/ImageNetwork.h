#pragma once

#include "Connection.h"
#include "Service.h"
#include "P2pTable.h"

#include <assert.h>

class ImageNetwork {

  private:
    /**
     * Service for receiving image requests from netimg clients
     * and image transfers from peers.
     */
   const Service* imageService_;

   /**
    * Service for receiving peer/image traffic.
    */
   const Service* p2pService_;

   /**
    * Table of peer nodes.
    */
   P2pTable* p2pTable_;

   /**
    * Connection to img client (netimg, not peer).
    */
   const Connection* imageClient_;

  public:
   /**
    * ImageNetwork()
    * - Ctor for ImageNetwork.
    * @param img_service : service for img traffic
    * @param p2p_service : service for p2p traffic
    * @param p2p_table   : table of peers
    */
   ImageNetwork(
       const Service& img_service,
       const Service& p2p_service,
       P2pTable& p2p_table);

   /**
    * getImageService()
    * @return img service   
    */
   const Service& getImageService() const;
   
   /**
    * getP2pService()
    * @return p2p service   
    */
   const Service& getP2pService() const;
   
   /**
    * getP2pTable()
    * @return p2p table   
    */
   P2pTable& getP2pTable();

   /**
    * hasImageClient()
    * @return true iff a valid imgClient exists 
    */
   bool hasImageClient() const;

   /**
    * getImageClient()
    * @return image client  
    * @throws std::runtime_error if imgClient doesn't exist
    */
   const Connection& getImageClient() const;

   /**
    * setImageClient()
    * - Store image client.
    * @param connection to new imgClient  
    */
   void setImageClient(const Connection* img_client);

   /**
    * invalidateImageClient()
    * - Delete image client after sending back image. 
    */
   void invalidateImageClient();

};
