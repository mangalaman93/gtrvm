#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
using namespace std;

/* we assign rvm ids incrementally and every new id
   is bigger than highest assigned id till now */
extern int base_rvm_id;

/* similar to rvm ids, we assign transaction ids incrementally
   and in a monotonically incremental way */
extern int base_trans_id;

typedef int trans_t;
typedef int rvm_t;

// struct for Undo Logs
typedef struct UndoLog {
  int32_t offset;
  int32_t size;
  char* data;

  UndoLog(int32_t o, int32_t s) : offset(o), size(s) {
    data = new char[size];
  }

  ~UndoLog() {
    delete[] data;
  }
} UndoLog;

/* struct to create a linked list of UndoLog */
typedef struct UndoLogNode {
  UndoLog* pointer;
  struct UndoLogNode* next;

  UndoLogNode(UndoLog *p) : pointer(p), next(NULL) {}
} UndoLogNode;

typedef struct segment_t {
  int size;
  int modify;
  char* segname;
  UndoLogNode* undo_logs;

  segment_t(int s, char* n)
    : size(s), modify(-1), segname(n), undo_logs(NULL) {}
} segment_t;

struct rvm_int_t;
typedef struct trans_int_t {
  // rvm pointer
  struct rvm_int_t* rvm;
  // vector of all used segbase pointers
  vector<char*>* segbase_pointers;

  trans_int_t(struct rvm_int_t* r) {
    rvm = r;
    segbase_pointers = new vector<char*>;
  }

  ~trans_int_t() {
    delete segbase_pointers;
  }
} trans_int_t;

// for map
struct CharCompare : public binary_function<const char*, const char*, bool> {
public:
  bool operator() (const char* str1, const char* str2) const {
   return strcmp(str1, str2) < 0;
 }
};

/* rvm struct */
typedef struct rvm_int_t {
  // corresponding directory name
  char* directory;
  // mapping from segname to memory area
  map<const char*, char*, CharCompare>* segname_to_memory;
  // mapping from memory area to segment
  map<char*, segment_t*>* memory_to_struct;

  rvm_int_t(const char* dir) {
    directory = new char[strlen(dir)+1];
    strcpy(directory, dir);
    segname_to_memory = new map<const char*, char*, CharCompare>();
    memory_to_struct = new map<char*, segment_t*>();
  }

  ~rvm_int_t() {
    delete[] directory;
    delete segname_to_memory;
    delete memory_to_struct;
  }
} rvm_int_t;

extern map<trans_t, trans_int_t*> transactions;
extern map<rvm_t, rvm_int_t*> rvms;
