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
extern long int base_trans_id;

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
  // linked list of ongoing transactions (trans_t*)
  LinkedListNode *transactions;

  // default constructor overload
  rvm_t() : id(-1), directory(NULL), segname_to_memory(NULL),
            memory_to_segname(NULL), transactions(NULL) {}

  // constructor
  rvm_t(const char* dir) : id(-1), directory(NULL), segname_to_memory(NULL),
                           memory_to_segname(NULL), transactions(NULL) {
    base_rvm_id += 1;
    id = base_rvm_id;
    directory = (char*)malloc(strlen(dir)+1);
    strcpy(directory, dir);
    segname_to_memory = new map<const char*, void*, CharCompare>();
    memory_to_segname = new map<void*, char*>();
    transactions = NULL;
  }

  ~rvm_t() {
    free(directory);

    // destructor @todo delete inside objects including transactions linked list
    delete segname_to_memory;
    delete memory_to_segname;
  }

  // cast to int
  operator int() const {
    return id;
  }
} rvm_t;

typedef struct UndoLog {
  void* base;
  int32_t offset;
  size_t size;
  void* data;
} UndoLog;

typedef struct trans_t {
  // transaction id
  long int id;
  // vector of all used segbase pointers
  vector<void*> *segbase_pointers;
  // linked list of all the undo logs (UndoLog*)
  LinkedListNode *undo_logs;

  // default constructor overload
  trans_t() : id(-1), segbase_pointers(NULL), undo_logs(NULL) {
    base_trans_id += 1;
    id = base_trans_id;
    segbase_pointers = new vector<void*>();
  }

  // destructor @todo
  ~trans_t() {}

  // cast to int
  operator int() const {
    return id;
  }
} trans_t;
