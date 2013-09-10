// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  if(content_map_cache.find(eid) != content_map_cache.end()){
    //already in
    buf = content_map_cache[eid];
    printf("the content of %016llx is @%s@\n",eid,buf.c_str());
    return ret;
  }else{
    ret = cl->call(extent_protocol::get, eid, buf);
    if(ret == extent_protocol::OK){
      content_map_cache[eid] = buf;
    }
  }
  printf("the content of %016llx is @%s@\n",eid,buf.c_str());
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  if(attr_map_cache.find(eid) != attr_map_cache.end()){
    attr = attr_map_cache[eid];
  }else{
    ret = cl->call(extent_protocol::getattr, eid, attr);
    if(ret == extent_protocol::OK){
      attr_map_cache[eid] = attr;
    }
  }
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  //int r;
  //ret = cl->call(extent_protocol::put, eid, buf, r);
  content_map_cache[eid] = buf;
  dirty_eid_set.insert(eid);
  struct extent_protocol::attr new_attr;
  memset(&new_attr,0,sizeof(new_attr));
  time_t now = time(NULL);
  new_attr.atime = now;
  new_attr.ctime = now;
  new_attr.mtime = now;
  new_attr.size = buf.size();
  attr_map_cache[eid] = new_attr;
  printf("in the extent put,we set %016llx to be @%s@\n",eid,buf.c_str());
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  //int r;
  //ret = cl->call(extent_protocol::remove, eid, r);
  if(content_map_cache.find(eid) != content_map_cache.end()){
    content_map_cache.erase(eid);
  }
  if(attr_map_cache.find(eid) != attr_map_cache.end()){
    attr_map_cache.erase(eid);
  }
  return ret;
}

extent_protocol::status
extent_client::flush(extent_protocol::extentid_t eid){
  extent_protocol::status ret = extent_protocol::OK;
  if(dirty_eid_set.find(eid) != dirty_eid_set.end()){
    //already dirty now, sent it back
    int r;
    printf("in the flush, we write back %016llx to be @%s@\n",eid,content_map_cache[eid].c_str());
    ret = cl->call(extent_protocol::put, eid, content_map_cache[eid], r);
    dirty_eid_set.erase(eid);
  }
  remove(eid);
  return ret;
}
