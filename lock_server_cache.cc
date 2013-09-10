// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

static void *th_func(void *args){
    lock_server_cache *server = (lock_server_cache *) args;
    server->wake_up();  
    return NULL;
}

lock_server_cache::lock_server_cache():
  nacquire(0),where_is_lock(),who_waiting_lock()
{
  pthread_mutex_init(&serverlock_mutex,NULL);
  //pthread_t th;
  //pthread_create(&th, NULL, &th_func, (void *)this);
}

void lock_server_cache::wake_up(){
  while(1){
  tprintf("the server's background pthread used to wake up some thread\n");
  sleep(3);
  tprintf("wake_up waiting for the mutex\n");
  pthread_mutex_lock(&serverlock_mutex);
  std::map<lock_protocol::lockid_t,std::list<std::string> >::iterator iter;
  for(iter = who_waiting_lock.begin(); iter != who_waiting_lock.end(); ++iter){
    if(!iter->second.empty()){
        std::string owner = where_is_lock[iter->first];
        handle h(owner);
        rlock_protocol::status hret;
        rpcc *ser_cl;
        if((ser_cl = h.safebind()) != NULL){
          int r;
          hret = ser_cl->call(rlock_protocol::revoke, iter->first, r);
          tprintf("wake_up call the client %s for the lock\n",owner.c_str());
        }
        if(!ser_cl || hret != rlock_protocol::OK){
          tprintf("server wake_up unfortunately come here\n");
          pthread_mutex_unlock(&serverlock_mutex);
          //goto step_out;
        }
    }
  }
  pthread_mutex_unlock(&serverlock_mutex);
  }
step_out:
  pthread_exit(NULL);
}

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  tprintf("client %s : waiting in the acquire, for the mutex\n", id.c_str());
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&serverlock_mutex);
  tprintf("client %s : get the acquire mutex\n", id.c_str());
  if(where_is_lock.find(lid) == where_is_lock.end()){
    //no such lid, just create and grant it
    where_is_lock[lid] = id;
    tprintf("client %s, the server have never heard the lock, give it to you\n",id.c_str());
    pthread_mutex_unlock(&serverlock_mutex);
    return ret;
  }else{
    if(where_is_lock[lid] == ""){
      //this lid is now belong to the server
      tprintf("now no one own this lock, give it to client %s \n",id.c_str());
      where_is_lock[lid] = id;
      pthread_mutex_unlock(&serverlock_mutex);
      return ret;
    }else{
      //someone else own this lock now
      //add this id to the waiting list
      std::list<std::string>::iterator iter;
      bool isWaiting = false;
      bool need_revoke = false; // only when the waiting list equals empty that need revoke
      if(who_waiting_lock[lid].empty()){
        need_revoke = true;
      }
      for(iter = who_waiting_lock[lid].begin(); iter != who_waiting_lock[lid].end(); ++iter){
        if(iter->compare(id) == 0){
          isWaiting = true;
        }
      }
      if(!isWaiting){
        tprintf("client %s has been added to the waiting list\n",id.c_str());
        who_waiting_lock[lid].push_back(id);
      }
      //then revoke the owner
      pthread_mutex_unlock(&serverlock_mutex);      
      if(need_revoke){
        tprintf("client %s generate revoke\n",id.c_str());
        std::string owner = where_is_lock[lid];
        handle h(owner);
        rlock_protocol::status hret;
        rpcc *ser_cl;
        if((ser_cl = h.safebind()) != NULL){
          int r;
          hret = ser_cl->call(rlock_protocol::revoke, lid, r);
        }
        if(!ser_cl || hret != rlock_protocol::OK){
          tprintf("client %s unfortunately come here\n",id.c_str());
          ret = lock_protocol::RPCERR;
          pthread_mutex_unlock(&serverlock_mutex);
          return ret;
        }
        tprintf("client %s : successfully called the revoke rpc \n", id.c_str());
      }
      ret = lock_protocol::RETRY;
      //pthread_mutex_unlock(&serverlock_mutex);
      return ret;
    }
  }
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  tprintf("client %s, in the server release function\n",id.c_str());
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&serverlock_mutex);
  where_is_lock[lid] = "";
  tprintf("client %s, server release done\n",id.c_str());
  //pthread_mutex_unlock(&serverlock_mutex);
  if(!who_waiting_lock[lid].empty()){
    std::string first = who_waiting_lock[lid].front();
    who_waiting_lock[lid].pop_front();
    handle h(first);
    int r;
    rpcc *ser_cl = h.safebind();
    ser_cl->call(rlock_protocol::retry, lid, r);
    tprintf("client %s, in the server release, called retry\n",id.c_str());
  }
  pthread_mutex_unlock(&serverlock_mutex);
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}


