// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0), lock_inuse_map(), lock_cv_map()
{
  pthread_mutex_init(&lockmap_mutex,NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret;
  printf("acquire request from clt %d\n",clt);
  pthread_mutex_lock(&lockmap_mutex);
  std::map<lock_protocol::lockid_t,int>::iterator iter = lock_inuse_map.find(lid);
  if(iter == lock_inuse_map.end()){
    //still no such lock
    pthread_cond_init(&lock_cv_map[lid], NULL);
    lock_inuse_map[lid] = 1;
    ret = lock_protocol::OK;
    pthread_mutex_unlock(&lockmap_mutex);
    return ret;
  }
  iter->second += 1;
  if(iter->second == 1){
    ret = lock_protocol::OK;
    pthread_mutex_unlock(&lockmap_mutex);
    return ret;
  }
  pthread_cond_wait(&lock_cv_map[lid],&lockmap_mutex);
  ret = lock_protocol::OK;
  pthread_mutex_unlock(&lockmap_mutex);
  return ret; 
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r){
  lock_protocol::status ret;
  printf("release request from clt %d\n", clt);
  pthread_mutex_lock(&lockmap_mutex);
  std::map<lock_protocol::lockid_t, int>::iterator iter = lock_inuse_map.find(lid);
  pthread_cond_signal(&lock_cv_map[lid]);
  iter->second -= 1;
  ret = lock_protocol::OK;
  pthread_mutex_unlock(&lockmap_mutex);
  return ret;
}
