#include "rvm.h"

rvm_t rvm_init(const char *directory) {
  // initialize the log segment and return it
}

void *rvm_map(rvm_t rvm, const char *segname, int size_to_create) {
  // map the data segment to region of virtual address space
}

void rvm_unmap(rvm_t rvm, void *segbase) {
  // unmap, the details should be available in rvm_t data structure
}

void rvm_destroy(rvm_t rvm, const char *segname) {
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases) {
  // create undo logs
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size) {
  // set the portion of segment which is to be modified until commit is called
}

void rvm_commit_trans(trans_t tid) {
  // commit the redo logs
  // throw away the undo logs
}

void rvm_abort_trans(trans_t tid) {
  // throw away the redo logs
}

void rvm_truncate_log(rvm_t rvm) {
  // apply the redo logs on the backing file
}
