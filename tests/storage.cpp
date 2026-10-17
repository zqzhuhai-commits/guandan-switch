#include "../source/engine.hpp"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
static int rejectOverwrite(const char*from,const char*to){struct stat s{};if(stat(to,&s)==0){errno=EEXIST;return -1;}return std::rename(from,to);}
static int rejectInstall(const char*from,const char*to){std::string f=from;if(f.size()>4&&f.substr(f.size()-4)==".tmp"){errno=ENOSPC;return -1;}return rejectOverwrite(from,to);}
static std::string read(const std::string&p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
 char dir[]="/tmp/gd-storage-XXXXXX";assert(mkdtemp(dir));std::string root=std::string(dir)+"/switch/GuandanSwitch/";
 assert(gd::ensureDirectory(root));assert(gd::ensureDirectory(root));
 std::string p=root+"sample";assert(gd::atomicTextFile(p,"first"));
 {std::ofstream f(p+".tmp");f<<"second";}assert(gd::commitTemporaryFile(p,rejectOverwrite));assert(read(p)=="second"&&read(p+".bak")=="first");
 {std::ofstream f(p+".tmp");f<<"third";}assert(!gd::commitTemporaryFile(p,rejectInstall));assert(errno==ENOSPC);assert(read(p)=="second"&&read(p+".tmp")=="third");
 assert(gd::commitTemporaryFile(p,rejectOverwrite));assert(read(p)=="third"&&read(p+".bak")=="second");
 gd::Game game;game.newMatch(0,123);assert(game.save(root+"session.dat"));for(int i=0;i<16;i++){game.autoStep();assert(game.save(root+"session.dat"));gd::Game copy;assert(copy.load(root+"session.dat"));assert(copy.history.size()==game.history.size());}
 assert(!gd::ensureDirectory(p+"/not-a-directory"));assert(errno==ENOTDIR);
 for(auto name:{"sample","sample.bak","sample.tmp","session.dat","session.dat.bak"})std::remove((root+name).c_str());rmdir(root.c_str());rmdir((std::string(dir)+"/switch").c_str());rmdir(dir);
 std::cout<<"PASS: recursive first-run paths, non-overwriting Switch rename, repeated save/load and failed-commit rollback\n";
}
