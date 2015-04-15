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
extern int base_rvm_id;

/* similar to rvm ids, we assign transaction ids incrementally
   and in a monotonically incremental way */
extern int64_t base_trans_id;

/* genertic struct to create a linked lits */
typedef struct LinkedListNode {
  void* pointer;
  struct LinkedListNode* next;

  LinkedListNode() : pointer(NULL), next(NULL) {}
} LinkedListNode;

struct CharCompare : public binary_function<const char*, const char*, bool> {
public:
  bool operator() (const char* str1, const char* str2) const {
   return std::strcmp(str1, str2) < 0;
 }
};

/* rvm struct */
typedef struct rvm_t {
  // rvm id
  int id;
  // corresponding directory name
  char* directory;
  // mapping from segname to memory area
  map<const char*, void*, CharCompare>* segname_to_memory;
  // reverse mapping to memory area to segment name
  map<void*, char*>* memory_to_segname;
  // memory to its size
  map<void*, int> memory_to_size;
  // linked list of ongoing transactions (trans_t*)
  LinkedListNode *transactions;

  // default constructor overload
  rvm_t() : id(0), directory(NULL), segname_to_memory(NULL),
            memory_to_segname(NULL), memory_to_size(NULL), transactions(NULL) {}

  // constructor
  rvm_t(const char* dir) : id(-1), directory(NULL), segname_to_memory(NULL),
                           memory_to_segname(NULL), memory_to_size(NULL), transactions(NULL) {
    base_rvm_id += 1;
    id = base_rvm_id;
    directory = new char[strlen(dir)+1];
    strcpy(directory, dir);
    segname_to_memory = new map<const char*, void*, CharCompare>();
    memory_to_segname = new map<void*, char*>();
    memory_to_size = new map<void*, int>();
    transactions = NULL;
  }

  ~rvm_t() {
    delete[] directory;

    // deleting data areas & corresponding segment string
    for(map<void*, char*>::iterator iter=memory_to_segname->begin();
        iter!=memory_to_segname->end(); ++iter) {
      delete[] iter->first;
      delete[] iter->second;
    }

    // deleting if any transaction is incomplete
    while(transactions) {
      delete transactions->pointer;

      LinkedListNode *node = transactions;
      transactions = node->next;
      delete node;
    }

    delete segname_to_memory;
    delete memory_to_segname;
    delete memory_to_size;
  }

  // cast to int
  operator int() const {
    return id;
  }
} rvm_t;

typedef struct UndoLog {
  void* base;
  int32_t offset;
  int32_t size;
  void* data;

  UndoLog() : base(NULL), offset(0), size(0), data(NULL) {}
} UndoLog;

typedef struct trans_t {
  // rvm pointer
  rvm_t *rvm;
  // transaction id
  int64_t id;
  // vector of all used segbase pointers
  vector<void*> *segbase_pointers;
  // linked list of all the undo logs (UndoLog*)
  LinkedListNode *undo_logs;

  // default constructor overload
  trans_t() : rvm(NULL), id(0), segbase_pointers(NULL), undo_logs(NULL) {
    base_trans_id += 1;
    id = base_trans_id;
    segbase_pointers = new vector<void*>();
  }

  // destructor
  ~trans_t() {
    delete segbase_pointers;

    while(undo_logs) {
      delete[] undo_logs->data;

      LinkedListNode node = undo_logs;
      undo_logs = node->next;
      delete node;
    }
  }

  // cast to int
  operator int() const {
    return id;
  }
} trans_t;
