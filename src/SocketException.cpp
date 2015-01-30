#include "SocketException.h"

SocketException::SocketException(const std::string& msg) 
  : std::runtime_error(msg) {}
