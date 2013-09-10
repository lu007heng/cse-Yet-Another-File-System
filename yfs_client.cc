// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client_cache.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  //lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  lock_release_action *lra = new lock_release_action(ec);
  lc = new lock_client_cache(lock_dst, lra);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock
  // does this mean that I only need to add a lock here?
  lc->acquire(inum);
  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    //printf("@@@@@@@@@@@@@@@@@@@@@@@@@@get here!!!@@@@@@");
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:
  lc->release(inum);
  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock
  lc->acquire(inum);
  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  lc->release(inum);
  return r;
}

std::vector<std::string> 
yfs_client::split(const std::string& src, std::string separate_character)
{
	std::vector<std::string> strs;
	int separate_characterLen = separate_character.size();//分割字符串长度，这样就可以支持多字 符串的分隔符
	int lastPosition = 0;
	int index = -1;
	while (-1 != (index = src.find(separate_character, lastPosition)))
	{
		strs.push_back(src.substr(lastPosition, index - lastPosition));
		lastPosition = index + separate_characterLen;
	}
	std::string lastString = src.substr(lastPosition);
	if (!lastString.empty())
		strs.push_back(lastString);
	return strs;
}

bool
yfs_client::isExist(inum parent, const char *name){
//if name already exist, return true
    std::string entries;
	if(ec->get(parent, entries) != extent_protocol::OK){
	  return false;
	}
	std::string str_name(name);
	std::string sep = " ";
	std::vector<std::string> key_values = split(entries,sep);
	for(unsigned int i = 1; i < key_values.size() ; i += 2){
	    if(str_name == key_values[i]){
		  return true;
		}
	}
	return false;	
}

int
yfs_client::add_entry(inum parent, const char *name, inum new_inum){
//add a <name,new_inum> entry into the @parent
// return 0 means success
    std::string entries;
	if(ec->get(parent, entries) != extent_protocol::OK){
	  return 1;
	}
	std::string str_name(name);
	std::string sep = " ";
        if(entries != ""){
	  entries += sep;
        }
	entries += filename(new_inum);
	entries += sep;
	entries += str_name;
	if(ec->put(parent, entries) != extent_protocol::OK){
	  return 1;
	}
	return 0;
}

int 
yfs_client::create(inum parent, const char *name, inum new_inum, extent_protocol::attr &result_attr)
{
  lc->acquire(parent);
  lc->acquire(new_inum);
  int ret = yfs_client::OK;
  std::string new_extent = "";
  printf("in yfs function call , we get the inum is %016llx, and the name is %s\n",new_inum,name);
  if(isExist(parent,name)){
    ret = yfs_client::EXIST;
    goto release;
  }
  //std::string new_extent = "";
  ec->put(new_inum,new_extent);
  printf("when we try to send the inum in the client, the inum is %016llx \n",new_inum);
  if( 0 != add_entry(parent, name, new_inum)){
    ret = yfs_client::IOERR;
    goto release;
  }
  //Change the parent's mtime and ctime to the current time/date
  //this may be done while we add_entry
  /*
  string temp;
  ec->get(parent,temp);
  ec->put(parent,temp);*/
  //result_inum = new_inum;
  if(ec->getattr(new_inum,result_attr) != extent_protocol::OK){
    ret = yfs_client::IOERR;
    goto release;
  }
  printf("the file %s is created\n",name);
 
release:
  lc->release(new_inum);
  lc->release(parent);
  return ret;
  
}

int 
yfs_client::lookup(inum parent, const char *name, inum &result_inum, extent_protocol::attr &result_attr, bool &found){
        int ret = yfs_client::OK;
        lc->acquire(parent);
        std::string entries;
        std::string str_name(name);
        std::string sep = " ";
        std::vector<std::string> key_values;
	if(ec->get(parent, entries) != extent_protocol::OK){
	  ret = yfs_client::IOERR;
          goto release;
	}
	//std::string str_name(name);
	//std::string sep = " ";
	/*std::vector<std::string>*/ key_values = split(entries,sep);
	for(unsigned int i = 1; i < key_values.size() ; i += 2){
	    if(str_name == key_values[i]){
		  found = true;
		  result_inum = n2i(key_values[i-1]);
                  lc->acquire(result_inum);
		  if(ec->getattr(n2i(key_values[i-1]), result_attr) != extent_protocol::OK){
		    ret = yfs_client::IOERR;
                    lc->release(result_inum);
                    goto release;
		  }
                  lc->release(result_inum);
		  ret = yfs_client::OK;
                  goto release;
		}
	}
  release:
        lc->release(parent);
	return ret;
}

int 
yfs_client::readdir(inum parent, std::vector<std::string> &result){
	std::string entries;
        int ret = yfs_client::OK;
        lc->acquire(parent);
        std::string sep = " ";
	if(ec->get(parent, entries) != extent_protocol::OK){
	  ret = yfs_client::IOERR;
          goto release;
	}
        //printf("here the entries equals = *%s*\n",entries);
	//std::string sep = " ";
	result = split(entries,sep);
        /*for(int i = 0; i< result.size();i++){
          printf("result[%d] = %s\n",result[i]);
        }*/
  release:
        lc->release(parent);
	return ret;
}

int
yfs_client::updatesize(inum tochange_inum, size_t new_size){
  extent_protocol::attr old_attr;
  int ret = yfs_client::OK;
  size_t old_size;
  std::string old_buf;
  lc->acquire(tochange_inum);
  if(ec->getattr(tochange_inum,old_attr) != extent_protocol::OK){
    ret = yfs_client::IOERR;
    goto release;
  }
  
  old_size = old_attr.size;
  old_buf;
  if(new_size > old_size){
    if(ec->get(tochange_inum,old_buf) != extent_protocol::OK){
      ret = yfs_client::IOERR;
      goto release;
    }
    old_buf.append(new_size-old_size,'\0');
    //int i = 0;
    /*while(i < new_size-old_size){
        old_buf.append("\0");
        ++i;
    }*/
    if(ec->put(tochange_inum,old_buf) != extent_protocol::OK){
      ret = yfs_client::IOERR;
      goto release;
    }
  }else{
    if(ec->get(tochange_inum,old_buf) != extent_protocol::OK){
      ret = yfs_client::IOERR;
      goto release;
    }
    if(ec->put(tochange_inum,old_buf.substr(0,new_size)) != extent_protocol::OK){
      ret = yfs_client::IOERR;
      goto release;
    } 
  }
 release:
  lc->release(tochange_inum);
  return ret;
}

int 
yfs_client::read(inum to_read, size_t size, off_t off, std::string &buf){
  std::string content;
  int ret = yfs_client::OK;
  lc->acquire(to_read);
  printf("here entered the yfs_client::read\n");
  if(ec->get(to_read, content) != extent_protocol::OK){
    ret = yfs_client::IOERR;
    goto release;
  }
  printf("we get here in yfs_client::read, and the content is *%s*\n",content.c_str());
  //printf("the off:%d the size:%d \n",off,size);
  //printf("what the fucking size is %ld \n",size);
  if(off < content.size()){
    //printf("%d,%d,%d,%d\n",size<0,size==0,size>0,size==NULL);
    if(size < 0){
      printf("the condition works, is null \n");
      buf = content.substr(off);
      printf("the buf is %s\n",buf.c_str());
    }else{
      printf("unfortunately, it has not work\n");
      buf = content.substr(off,size);
      printf("can we get here?\n");
      printf("the buf is %s\n",buf.c_str());
    }
  }else{
    buf = "";
  }
  printf("what we readout is *%s* haha\n",buf.c_str());
 release:
  lc->release(to_read);
  return ret;
}

int
yfs_client::write(inum to_write, const char *buf, size_t size, off_t off){
  //printf("what the test write is *%s* haha \n",buf);
  //printf("in yfs_client::write:buf:*%s*,size:%ld,off:%ld\n",buf,size,off);
  int ret = yfs_client::OK;
  lc->acquire(to_write);
  std::string buf_str(buf);
  printf("when convert to string,the string is: *%s* the size is : %d\n",buf_str.c_str() , buf_str.size());
  std::string content;
  extent_protocol::attr the_attr;
  std::string new_content;
  if(ec->get(to_write, content) != extent_protocol::OK){
    ret = yfs_client::IOERR;
    goto release;
  }
  //if this is an overwrite
  if(ec->getattr(to_write,the_attr) != extent_protocol::OK){
    ret = yfs_client::IOERR;
    goto release;
  }
  if(the_attr.size == 0){
    printf("this is a overwrite");
    if(size > buf_str.size()){
      buf_str.append(size-buf_str.size(),'\0');
    }
    if(ec->put(to_write,buf_str.substr(0,size)) != extent_protocol::OK){
      ret = yfs_client::IOERR;
      goto release;
    }
    ret = yfs_client::OK;
    goto release;
  }
  printf("the original content is *%s*\n",content.c_str());
  //how about if the size is originally bigger than then length of buf?
  if(size > buf_str.size()){
    printf("unfortunately this happened~");
    buf_str.append(size-buf_str.size(),'\0');
  }
  if(off > content.size()){
    new_content = content.append(off-content.size(),'\0');
    new_content += buf_str.substr(0,size);
  }else{
    //new_content = content.substr(0,off) + buf_str.substr(0,size);
    if((off+size > content.size()) || (off+size == content.size())){
      if(off == 0)
        new_content = buf_str.substr(0,size);
      else
        new_content = content.substr(0,off)+buf_str.substr(0,size);
    }else{
      if(off == 0)
        new_content = buf_str.substr(0,size)+content.substr(off+size);
      else
        new_content = content.substr(0,off)+buf_str.substr(0,size)+content.substr(off+size);
    }
  }
  printf("after write, the new content is: *%s*\n",new_content.c_str());
  if(ec->put(to_write,new_content) != extent_protocol::OK){
    ret = yfs_client::IOERR;
    goto release;
  }
 release:
  lc->release(to_write);
  return ret;
}

int 
yfs_client::mymkdir(inum parent,const char *name,inum dir_inum){
  //if the name is still exist
  int ret = yfs_client::OK;
  std::string new_extent;
  lc->acquire(parent);
  lc->acquire(dir_inum);
  if(isExist(parent,name)){
    ret = yfs_client::EXIST;
    goto release;
  }
  new_extent = "";
  if(ec->put(dir_inum,new_extent) != extent_protocol::OK){
    ret = yfs_client::IOERR;
    goto release;
  }
  if( 0 != add_entry(parent, name, dir_inum)){
    ret = yfs_client::IOERR;
    goto release;
  }
 release:
  lc->release(dir_inum);
  lc->release(parent);
  return ret;
}

int
yfs_client::unlink(inum parent, const char *name){
  int ret = yfs_client::OK;
  std::string entries;
  std::string str_name(name);
  std::string sep = " ";
  inum unlink_inum;
  std::string str_unlink_inum;
  std::vector<std::string> key_values;
  std::string new_extent = "";
  std::string new_entries = "";
  lc->acquire(parent);
  if(isExist(parent,name) == 0){
    ret = yfs_client::NOENT;
    goto release;
  }
  //check the name is not a dir, then remove the entry from its parent
  //std::string entries;
  if( ec->get(parent,entries) != extent_protocol::OK){
    ret = yfs_client::IOERR;
    goto release;
  }
  printf("the entries for %s is *%s*\n",filename(parent).c_str(),entries.c_str());
  //std::string str_name(name);
  //std::string sep = " ";
  //inum unlink_inum;
  //std::string str_unlink_inum;
  key_values = split(entries,sep);
  for(unsigned int i = 1; i < key_values.size(); i += 2){
    if(str_name==key_values[i]){
      str_unlink_inum = key_values[i-1];
      unlink_inum = n2i(key_values[i-1]);
      break;
    }
  }
  if(isdir(unlink_inum)){
    ret = yfs_client::IOERR;
    goto release;
  }
  //std::string new_entries = "";
  for(unsigned int i=0; i<key_values.size(); i++){
    if(key_values[i] != str_unlink_inum && key_values[i] != str_name){
      new_entries += key_values[i];
      new_entries += sep;
    }
  }
  printf("the new entries for %s is *%s*\n",filename(parent).c_str(),new_entries.c_str());
  if(ec->put(parent,new_entries.substr(0,new_entries.size()-1)) != extent_protocol::OK){
    ret = yfs_client::IOERR;
    goto release;
  }
  //free its extent
  lc->acquire(unlink_inum);
  //std::string new_extent = "";
  if(ec->put(unlink_inum,new_extent) != extent_protocol::OK){
    ret = yfs_client::IOERR;
    lc->release(unlink_inum);
    goto release;
  }
 release:
  lc->release(parent);
  if(ret == yfs_client::OK)
    printf("the unlink of %s is successful\n",name);
  return ret;
}






