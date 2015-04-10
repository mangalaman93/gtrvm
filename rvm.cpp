#include "rvm.h"
#include <stdlib.h>
#include <string>
#include <stdio.h>
using namespace std

rvm_t rvm_init(const char *directory) {
  string dir = string(directory);
  string cmd="mkdir "+dir;
  system(cmd);
  return directory;
}

void *rvm_map(rvm_t rvm, const char *segname, int size_to_create) {
}

void rvm_unmap(rvm_t rvm, void *segbase) {
}

void rvm_destroy(rvm_t rvm, const char *segname) {
  string filename = string(rvm)+"/"+string(segname);
  string touch= "touch "+filename;
  string cmd = "rm "+filename;
  system(touch);
  system(cmd);
  //If system cmd runs before system touch returns we might have a problem      
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases) {
  trans_t new;
  //If segments are not in use then create a trans_t 
  //this is not the best way
  new.rvm=rvm;
  new.numsegs=numsegs;
  new.segbases=segbases;
  //update list of segments
  return trans_t;
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size) {
}

void rvm_commit_trans(trans_t tid) {
}

void rvm_abort_trans(trans_t tid) {
}

void rvm_truncate_log(rvm_t rvm) {
}

