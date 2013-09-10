// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <set>
#include <string>
#include <ctime>
#include "extent_protocol.h"
#include "rpc.h"

class extent_client {
 private:
  rpcc *cl;
  std::map<extent_protocol::extentid_t,std::string> content_map_cache;
  std::map<extent_protocol::extentid_t,extent_protocol::attr> attr_map_cache;
  std::set<extent_protocol::extentid_t> dirty_eid_set;
 public:
  extent_client(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  extent_protocol::status flush(extent_protocol::extentid_t eid);
};

#endif 

