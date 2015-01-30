#pragma once

#include <string>
#include <stdexcept>

class SocketException : public std::runtime_error {

 public:
   /**
    * SocketException()
    * - Ctor for SocketException.
    */
   SocketException(const std::string& msg);
};
