#pragma once

#include <string>

#include <ServerSocket.h>

class ServerSocketBuilder {

 private:
   /**
    * True iff socket should be reused to allow peering. 
    */
  bool isReusable;
}
