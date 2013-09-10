#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include "lock_protocol.h"
#include "lock_client_cache.h"
#include <ctime>
#include <stdlib.h>

class yfs_client {
  extent_client *ec;
  lock_client_cache *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 public:
  static std::string filename(inum);
  static inum n2i(std::string);
  std::vector<std::string> split(const std::string &, std::string);
  bool isExist(inum , const char *);
  int add_entry(inum, const char *, inum);
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
  int create(inum, const char *, inum , extent_protocol::attr &);
  int mymkdir(inum, const char *, inum);
  int lookup(inum, const char *, inum &, extent_protocol::attr &, bool &);
  int readdir(inum, std::vector<std::string> &);
  int updatesize(inum, size_t);
  int read(inum, size_t, off_t, std::string &);
  int write(inum, const char *, size_t, off_t);
  int unlink(inum, const char *);
};

#endif 
