// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include <pthread.h>
#include <ctime>
#include "extent_protocol.h"

class extent_server {

 private:
  //pthread_mutex_t extent_server_mutex;
  std::map<extent_protocol::extentid_t,std::string> content_map;
  std::map<extent_protocol::extentid_t,extent_protocol::attr> attr_map;
 public:
  extent_server();

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
};

#endif 







