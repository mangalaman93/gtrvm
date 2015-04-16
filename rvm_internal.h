#include <cstdio>
#include <cstdlib>
#include <cstring>
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
int base_rvm_id = 0;

/* similar to rvm ids, we assign transaction ids incrementally
   and in a monotonically incremental way */
int base_trans_id = 0;

// for map
struct CharCompare : public binary_function<const char*, const char*, bool> {
public:
  bool operator() (const char* str1, const char* str2) const {
   return std::strcmp(str1, str2) < 0;
 }
};

// to remove circular depedency
struct rvm_t;

// struct for Undo Logs
typedef struct UndoLog {
  int32_t offset;
  int32_t size;
  char* data;

  UndoLog() : offset(0), size(0), data(NULL) {}
} UndoLog;

/* struct to create a linked list of UndoLog */
typedef struct UndoLogNode {
  UndoLog* pointer;
  struct UndoLogNode* next;

  UndoLogNode() : pointer(NULL), next(NULL) {}
} UndoLogNode;

typedef struct segment_t {
  int size;
  bool modify;
  char* segname;
  UndoLogNode* undo_logs;

  segment_t(int s, bool m, char* n)
    : size(s), modify(m), segname(n), undo_logs(NULL) {}
} segment_t;

typedef struct trans_t {
  // rvm pointer
  struct rvm_t *rvm;
  // transaction id
  int64_t id;
  // vector of all used segbase pointers
  vector<void*> *segbase_pointers;

  // default constructor overload
  trans_t() : rvm(NULL), id(0), segbase_pointers(NULL) {
    base_trans_id += 1;
    id = base_trans_id;
    segbase_pointers = new vector<void*>();
  }

  // destructor
  ~trans_t() {
    delete segbase_pointers;
  }

  // cast to int
  operator int() const {
    return id;
  }
} trans_t;

/* rvm struct */
typedef struct rvm_t {
  // rvm id
  int id;
  // corresponding directory name
  char* directory;
  // mapping from segname to memory area
  map<const char*, char*, CharCompare>* segname_to_memory;
  // reverse mapping to memory area to segment name
  map<char*, segment_t*>* memory_to_struct;

  // default constructor overload
  rvm_t() : id(0), directory(NULL), segname_to_memory(NULL),
            memory_to_struct(NULL) {}

  // constructor
  rvm_t(const char* dir) : id(-1), directory(NULL), segname_to_memory(NULL),
                           memory_to_struct(NULL) {
    base_rvm_id += 1;
    id = base_rvm_id;
    directory = new char[strlen(dir)+1];
    strcpy(directory, dir);
    segname_to_memory = new map<const char*, char*, CharCompare>();
    memory_to_struct = new map<char*, segment_t*>();
  }

  ~rvm_t() {
    delete[] directory;
    delete segname_to_memory;

    // deleting data areas & corresponding segment string & undo logs
    for(map<char*, segment_t*>::iterator iter=memory_to_struct->begin();
        iter!=memory_to_struct->end(); ++iter) {
      UndoLogNode *undo_logs = iter->second->undo_logs;
      while(undo_logs) {
        delete[] undo_logs->pointer->data;

        UndoLogNode* node = undo_logs;
        undo_logs = node->next;
        delete node->pointer;
        delete node;
      }

      delete[] ((char*)iter->first);
      delete[] iter->second->segname;
      delete iter->second;
    }

    delete memory_to_struct;
  }

  // cast to int
  operator int() const {
    return id;
  }
} rvm_t;
