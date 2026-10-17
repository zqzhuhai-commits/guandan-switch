#pragma once
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>
namespace gd {
inline bool ensureDirectory(const std::string& directory){
 if(directory.empty()){errno=EINVAL;return false;}
 size_t start=directory.find(":/");start=start==std::string::npos?0:start+2;
 for(size_t end=start;end<=directory.size();end++){
  if(end<directory.size()&&directory[end]!='/')continue;
  std::string prefix=directory.substr(0,end);if(prefix.empty()||prefix.back()==':')continue;
  struct stat st{};if(stat(prefix.c_str(),&st)==0){if(!S_ISDIR(st.st_mode)){errno=ENOTDIR;return false;}continue;}
  if(errno!=ENOENT)return false;
  if(mkdir(prefix.c_str(),0777)!=0&&errno!=EEXIST)return false;
 }
 return true;
}
using RenameFile=int(*)(const char*,const char*);
using RemoveFile=int(*)(const char*);
// Horizon's rename API does not replace a destination. Never delete the current
// save before the completed temporary file and previous save are both protected.
inline bool commitTemporaryFile(const std::string& path,RenameFile move=std::rename,RemoveFile remove=std::remove){
 const auto temporary=path+".tmp",backup=path+".bak";struct stat st{};
 bool existing=stat(path.c_str(),&st)==0;
 if(!existing&&errno!=ENOENT)return false;
 if(existing){
  if(!S_ISREG(st.st_mode)){errno=EISDIR;return false;}
  if(remove(backup.c_str())!=0&&errno!=ENOENT)return false;
  if(move(path.c_str(),backup.c_str())!=0)return false;
 }
 if(move(temporary.c_str(),path.c_str())==0)return true;
 int failure=errno;
 if(existing)move(backup.c_str(),path.c_str());
 errno=failure;return false;
}
inline bool atomicTextFile(const std::string& path,const std::string& text){
 std::ofstream out(path+".tmp",std::ios::binary|std::ios::trunc);if(!out)return false;
 out.write(text.data(),std::streamsize(text.size()));out.flush();out.close();
 if(!out){if(!errno)errno=EIO;return false;}
 return commitTemporaryFile(path);
}
}
