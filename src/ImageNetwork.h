#pragma once

#include "Connection.h"
#include "Service.h"
#include "P2pTable.h"
#include "packets.h"

#include <assert.h>
#include <deque>

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

   /**
    * History of packet seen by this node.
    */
   std::deque<p2p_image_query_t> packetHistory_;

   /**
    * Current greatest search id assigned to p2p image-queries.
    */
   unsigned short currMaxSearchId_;

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

   /**
    * addImageQuery()
    * - Track this image query packet by adding to the "seen"
    *   history for this node.
    * - Simulates a curcular array with max-size MAX_PEERS
    * @param query_packet : packet to track  
    */
   void addImageQuery(const p2p_image_query_t& query_packet);

   /**
    * hasSeenP2pImageQuery()
    * @param query_packet : packet to check
    * @return true iff 'query-packet' exists in 'packetHistory'
    */
   bool hasSeenP2pImageQuery(const p2p_image_query_t& query_packet) const;

   /**
    * genSearchId()
    * - Return a uuid for p2p image-queries originating from this node.
    * @return unsigned short : uuid for p2p image search packets.
    */
   unsigned short genSearchId();

};
