#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include <queue>
#include <map>
#include <pthread.h>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
  int nacquire;
  std::map<lock_protocol::lockid_t,std::string> where_is_lock;
  std::map<lock_protocol::lockid_t,std::list<std::string> > who_waiting_lock;
  pthread_mutex_t serverlock_mutex;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
  void wake_up();
};

#endif
