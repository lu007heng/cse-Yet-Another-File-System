// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {
  //pthread_mutex_init(&extent_server_mutex,NULL);
  content_map.insert(std::map<extent_protocol::extentid_t,std::string>::value_type(0x1,""));
  struct extent_protocol::attr new_attr;
  memset(&new_attr,0,sizeof(new_attr));
  time_t now = time(NULL);
  new_attr.atime = now;
  new_attr.ctime = now;
  new_attr.mtime = now;
  new_attr.size = 0;
  attr_map.insert(std::map<extent_protocol::extentid_t,extent_protocol::attr>::value_type(0x1,new_attr));
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  // update the content_map
  std::map<extent_protocol::extentid_t,std::string>::iterator iter = content_map.find(id);
  if(iter == content_map.end()){
  // a new id
    content_map.insert(std::map<extent_protocol::extentid_t,std::string>::value_type(id,buf));
  }else{
    iter->second = buf;
  }
  // update the attr_map
  std::map<extent_protocol::extentid_t,extent_protocol::attr>::iterator iter1 = attr_map.find(id);
  if(iter1 == attr_map.end()){
    struct extent_protocol::attr new_attr;
    memset(&new_attr,0,sizeof(new_attr));
    time_t now = time(NULL);
    new_attr.atime = now;
    new_attr.ctime = now;
    new_attr.mtime = now;
    new_attr.size = buf.size();
    attr_map.insert(std::map<extent_protocol::extentid_t,extent_protocol::attr>::value_type(id,new_attr));
    printf("insert the attr of id %016llx is done, and the new buf size is %d\n",id,new_attr.size);
  }else{
    time_t now = time(NULL);
    iter1->second.ctime = now;
    iter1->second.mtime = now;
    iter1->second.size = buf.size();
  }
  //pthread_mutex_unlock(&extent_server_mutex);
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  printf("in extent_server::get function, try to find values of id: %016llx haha\n",id);
  std::map<extent_protocol::extentid_t,std::string>::iterator iter = content_map.find(id);
  std::map<extent_protocol::extentid_t,extent_protocol::attr>::iterator it = attr_map.find(id);
  if(iter != content_map.end() && it != attr_map.end()){
    printf("the values of id:%016llx is %s\n",id,iter->second.c_str());
    buf = iter->second;
    it->second.atime = time(NULL);
    return extent_protocol::OK;
  } 
  return extent_protocol::IOERR;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  std::map<extent_protocol::extentid_t,extent_protocol::attr>::iterator it = attr_map.find(id);
  if(it != attr_map.end()){
    a = it->second;
    return extent_protocol::OK;
  }
  return extent_protocol::IOERR;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  // both remove the id in content_map and attr_map
  std::map<extent_protocol::extentid_t,std::string>::iterator iter = content_map.find(id);
  std::map<extent_protocol::extentid_t,extent_protocol::attr>::iterator it = attr_map.find(id);
  if(iter != content_map.end() && it != attr_map.end()){
    //pthread_mutex_lock(&extent_server_mutex);
    content_map.erase(iter);
    attr_map.erase(it);
    //pthread_mutex_unlock(&extent_server_mutex);
    return extent_protocol::OK;
  }  
  return extent_protocol::IOERR;
}

/*int setattr(extent_protocol::extentid_t id, extent_protocol::attr attr, int &){
  pthread_mutex_lock(&extent_server_mutex);
  std::map<extent_protocol::extentid_t,extent_protocol::attr>::iterator iter = attr_map.find(id);
  if(iter != attr_map.end()){
    iter->second.ctime = attr.ctime;
    iter->second.mtime = attr.mtime;
  }else{
    pthread_mutex_unlock(&extent_server_mutex);
	return extent_protocol::NOENT
  }
  pthread_mutex_unlock(&extent_server_mutex);
  return extent_protocol::OK;
}*/
