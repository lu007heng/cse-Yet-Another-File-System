// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"
#include <unistd.h>

static void *th_function(void *args){
  lock_client_cache *lcc = (lock_client_cache *) args;
  lcc->give_back();
  return NULL;
}

void lock_client_cache::give_back(){
  //check the lock currently owned by me, and give it back if it is free for a long time
  while(1){
    sleep(1);
    //pthread_mutex_lock(&lockmap_mutex);
    tprintf("client %s begin to give back\n",id.c_str());
    std::map<lock_protocol::lockid_t,int>::iterator iter = lockstate_map.begin();
    for(; iter != lockstate_map.end(); ++iter){
      if(iter->second == client_lock_state::FREE){
        int r;
        tprintf("client %s, do a give back\n",id.c_str());
        pthread_mutex_lock(&lockmap_mutex);
        lockstate_map[iter->first] = client_lock_state::NONE;//++
        pthread_mutex_unlock(&lockmap_mutex);//++
        tprintf("get here, before lu->dorelease\n");
        lu->dorelease(iter->first);
        tprintf("get here, after lu->dorelease\n");
        lock_protocol::status ret1 = cl->call(lock_protocol::release, iter->first, id, r);
        if(ret1 != lock_protocol::OK){
          pthread_mutex_unlock(&lockmap_mutex);
          //goto step_out;
        }
        //lockstate_map[iter->first] = client_lock_state::NONE;
        pthread_cond_broadcast(&lock_cv_map[iter->first]);
        tprintf("in the give back, client %s, successfully give the lock %016llx back\n",id.c_str(),iter->first);
        break;// give back one at a time, maybe the bug is here
        //pthread_mutex_unlock(&lockmap_mutex);
      }
    }
    tprintf("client %s, end to give back\n", id.c_str());
    //pthread_mutex_unlock(&lockmap_mutex);
  }
step_out:
  tprintf("thread exit\n");
  pthread_exit(NULL); 
}

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();

  //initialize the mutex
  pthread_mutex_init(&lockmap_mutex,NULL);
  pthread_t th;
  pthread_create(&th, NULL, &th_function, (void *)this);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  //here insert the function of cache acquire
  //first, check the lid is already in this client
  pthread_mutex_lock(&lockmap_mutex);
  tprintf("client %s get the mutex, and do the acquire function\n",id.c_str());
  if(lockstate_map.find(lid) == lockstate_map.end()){
    lockstate_map[lid] = client_lock_state::NONE;
    pthread_cond_init(&lock_cv_map[lid], NULL);
  }

  while(1){
  if(lockstate_map[lid] == client_lock_state::FREE){
    //handle the state that the lid is free
    tprintf("client %s, haha, the lock is now in my local space\n",id.c_str());
    lockstate_map[lid] = client_lock_state::LOCKED;
    pthread_mutex_unlock(&lockmap_mutex);
    return ret;
  }else if(lockstate_map[lid] == client_lock_state::LOCKED){
    //handle the state that the lid is locked by another thread on this client
    pthread_cond_wait(&lock_cv_map[lid],&lockmap_mutex);
  }else if(lockstate_map[lid] == client_lock_state::ACQUIRING){
    //do nothing but wait the server to revoke
    pthread_cond_wait(&lock_cv_map[lid],&lockmap_mutex);   
  }else if(lockstate_map[lid] == client_lock_state::RELEASING){
    //although the lock is currently in this client, but it will return it back
    pthread_cond_wait(&lock_cv_map[lid],&lockmap_mutex);
  }else if(lockstate_map[lid] == client_lock_state::NONE){
    tprintf("client %s get into NONE, call the rpc, for lock %016llx \n",id.c_str(),lid);
    int r;
    lock_protocol::status ret1 = cl->call(lock_protocol::acquire, lid, id, r);//cl is inherited from the parent class
    tprintf("client %s, rpc call down\n",id.c_str());
    if(ret1 == lock_protocol::OK){
      lockstate_map[lid] = client_lock_state::LOCKED;
      pthread_mutex_unlock(&lockmap_mutex);
      tprintf("client %s get the lock %016llx\n",id.c_str(),lid);
      return ret;
    }else if(ret1 == lock_protocol::RETRY){
      tprintf("client %s , you just need to retry\n",id.c_str());
      lockstate_map[lid] = client_lock_state::ACQUIRING;
      pthread_cond_wait(&lock_cv_map[lid],&lockmap_mutex); 
    }else{
      tprintf("client %s, unrecognized reply\n",id.c_str());
    }
  }
  }
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  tprintf("client %s in the release, waiting for the lock\n",id.c_str());
  pthread_mutex_lock(&lockmap_mutex);
  tprintf("client %s begin to release\n",id.c_str());
  if(lockstate_map.find(lid) == lockstate_map.end()){
    ret = lock_protocol::NOENT;
    pthread_mutex_unlock(&lockmap_mutex);
    return ret;
  }
  if(lockstate_map[lid] == client_lock_state::RELEASING){
    //revoke has been called
    tprintf("client %s, you need to give back the lock\n",id.c_str());
    int r;
    lockstate_map[lid] = client_lock_state::NONE;//++
    pthread_cond_broadcast(&lock_cv_map[lid]);//++
    pthread_mutex_unlock(&lockmap_mutex);//++
    lu->dorelease(lid);
    lock_protocol::status ret1 = cl->call(lock_protocol::release, lid, id, r);
    if(ret1 != lock_protocol::OK){
      ret = ret1;
      pthread_mutex_unlock(&lockmap_mutex);
      return ret;
    }
    //lockstate_map[lid] = client_lock_state::NONE;
    //pthread_cond_broadcast(&lock_cv_map[lid]);
    tprintf("client %s, successfully give the lock back\n",id.c_str());
    //pthread_mutex_unlock(&lockmap_mutex);
    return ret;
  }else{
    tprintf("client %s, give back the lock to the local\n",id.c_str());
    lockstate_map[lid] = client_lock_state::FREE;
    pthread_cond_broadcast(&lock_cv_map[lid]);
    pthread_mutex_unlock(&lockmap_mutex);
    return ret;
  }
  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  int ret = rlock_protocol::OK;
  tprintf("client %s, the server called my revoke function\n",id.c_str());
  pthread_mutex_lock(&lockmap_mutex);
  tprintf("client %s, get the mutex and begin to revoke\n",id.c_str());
  if(lockstate_map[lid] == client_lock_state::FREE){
    tprintf("client %s, oh, the lock server want is free now in me\n",id.c_str());
    /*int r;
    lock_protocol::status ret1 = cl->call(lock_protocol::release, lid, id, r);
    if(ret1 != lock_protocol::OK){
      ret = ret1;
      printf("client %s, release failed\n",id.c_str());
      pthread_mutex_unlock(&lockmap_mutex);
      return ret;
    }
    lockstate_map[lid] = client_lock_state::NONE;*/
    pthread_mutex_unlock(&lockmap_mutex);
    return ret;
  }else if(lockstate_map[lid] == client_lock_state::LOCKED){
    tprintf("client %s, still locked, just wait\n",id.c_str()); 
    lockstate_map[lid] = client_lock_state::RELEASING;
    pthread_mutex_unlock(&lockmap_mutex);
    return ret;
  }else{
    //this means revoke comes ahead of the acquire ok
    //we need to do some thing
    pthread_mutex_unlock(&lockmap_mutex);
    tprintf("[DEBUG] this situation does exists, the situation is %d\n", lockstate_map[lid]);    
  }
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  tprintf("the server want me to retry\n");
  int ret = rlock_protocol::OK;
  pthread_mutex_lock(&lockmap_mutex);
  tprintf("get the mutex and begin to retry\n");
  if(lock_cv_map.find(lid) == lock_cv_map.end()){
    ret = rlock_protocol::RPCERR;
    tprintf("333333333333333333\n");
    pthread_mutex_unlock(&lockmap_mutex);
    return ret;
  }
  lockstate_map[lid] = client_lock_state::NONE;
  pthread_cond_broadcast(&lock_cv_map[lid]);
  pthread_mutex_unlock(&lockmap_mutex); 
  return ret;
}



