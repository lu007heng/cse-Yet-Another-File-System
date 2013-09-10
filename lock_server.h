// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <map>
#include <string>
#include <pthread.h>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

class lock_server {

 protected:
  int nacquire;
  std::map<lock_protocol::lockid_t,int> lock_inuse_map; // positive means in-use
  std::map<lock_protocol::lockid_t,pthread_cond_t> lock_cv_map;
  pthread_mutex_t lockmap_mutex;

 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt,lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt,lock_protocol::lockid_t lid, int &);
};

#endif 







