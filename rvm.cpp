#include "rvm.h"

int base_rvm_id = 0;
int base_trans_id = 0;

// max size of a log file 256KB
#define MAX_SIZE (1024*256)

// prefix for all the files
#define DIR_PREFIX "/tmp"
#define INDEX_FILE ".index"

// logs current position in file
#define LOG_POS() do {                                  \
  cout<<"line:"<<__LINE__<<" in file:"<<__FILE__<<endl; \
} while(0);

// throws error if condition is false
#define ASSERT(condition, message)          \
if(!(condition)) {                          \
  perror(message);                          \
  LOG_POS();                                \
  exit(EXIT_FAILURE);                       \
}

/* ------------------------ Abstract API for files ------------------------ */
bool exists_file(const char* filename) {
  ifstream file(filename);
  if(file) {
    file.close();
    return true;
  } else {
    return false;
  }
}

long get_file_size(const char* filename) {
  ifstream file(filename);
  ASSERT(file, "unable to get size of the file");

  long size = file.tellg();
  file.seekg(0, ios::end);
  ASSERT(file.good(), "unable to seek");
  size = file.tellg() - size;
  file.close();
  return size;
}

void create_file(const char* filename) {
  ofstream file(filename);
  ASSERT(file, "unable to create file!");
  file.close();
}

void write_to_file(const char* filename, const char* src, int size) {
  ofstream file(filename, ios::trunc|ios::binary);
  ASSERT(file, "unable to open file for writing");

  file.write(src, size);
  ASSERT(file.good(), "unable to write to file");
  file.flush();
  file.close();
}

void append_to_file(const char* filename, const char* src, int size) {
  ofstream file(filename, ios::app|ios::binary);
  ASSERT(file, "unable to open file for append-ing");

  file.write(src, size);
  ASSERT(file.good(), "unable to write to file");
  file.flush();
  file.close();
}

void read_from_file(const char* filename, char* dest, int size) {
  ifstream file(filename);
  ASSERT(file, "unable to open file for reading");

  file.read(dest, size);
  ASSERT(file.good(), "unable to read from file");
  file.close();
}
/*--------------------------------------------------------------------------*/

/* Initialize the library with the specified directory as backing store */
rvm_t rvm_init(const char *directory) {
  // initialize the log segment and return it
  stringstream ss;
  ss<<DIR_PREFIX<<"/"<<directory;
  mkdir(ss.str().c_str(), 0755);

  // reading the index file and setting the transaction numbers
  ss<<"/"<<INDEX_FILE;
  ifstream index(ss.str().c_str());
  if(index) {
    index.seekg(0, ios::end);
    size_t size = index.tellg();
    index.close();

    // if corrupt
    int temp = (size)%5;
    if(temp != 0) {
      truncate(ss.str().c_str(), size-temp);
    } else {
      index.close();
    }

    // finding last max transaction id
    ifstream indexfile(ss.str().c_str());
    if(size != 0) {
      char* buffer = new char[size];
      indexfile.read(buffer, size);
      indexfile.close();

      for(int i=0; i<(size/5); i++) {
        if(buffer[i*5+4] == ',') {
          int temp = *((int32_t*)(buffer[i*5]));
          if(temp < base_trans_id) {
            base_trans_id = temp;
          }
        }
      }

      delete[] buffer;
    }
  } else {
    create_file(ss.str().c_str());
  }

  // init rvm_t
  return rvm_t(directory);
}

/* map a segment from disk into memory. If the segment does not already exist,
   then create it and give it size size_to_create. If the segment exists but
   is shorter than size_to_create, then extend it until it is long enough.
   It is an error to try to map the same segment twice */
void *rvm_map(rvm_t rvm, const char *segname, int size_to_create) {
  char* segname_copy = (char*)malloc(strlen(segname)+1);
  strcpy(segname_copy, segname);

  // should be unmapped
  ASSERT(rvm.segname_to_memory->count(segname_copy) > 0,
              "trying to map the same segment again");

  // allocate memory area
  char* memory_area = (char*)malloc(size_to_create);
  bzero(memory_area, size_to_create);
  (*rvm.segname_to_memory)[segname_copy] = memory_area;
  (*rvm.memory_to_segname)[memory_area] = segname_copy;

  // backing file name
  stringstream ss;
  ss<<DIR_PREFIX<<"/"<<rvm.directory<<"/"<<segname_copy<<".bak";
  const char* bfilename = ss.str().c_str();

  // log file name
  stringstream st;
  st<<DIR_PREFIX<<"/"<<rvm.directory<<"/"<<segname_copy<<".log";
  const char* lfilename = st.str().c_str();

  // read the file in memory if it already exists
  if(exists_file(bfilename)) {
    long fsize = get_file_size(bfilename);

    if(fsize < size_to_create) {
      read_from_file(bfilename, memory_area, fsize);
      append_to_file(bfilename, memory_area+fsize, size_to_create-fsize);
    } else if(fsize > size_to_create) {
      read_from_file(bfilename, memory_area, size_to_create);
      write_to_file(bfilename, memory_area, size_to_create);
    } else {
      read_from_file(bfilename, memory_area, size_to_create);
    }

    ASSERT(exists_file(lfilename), "log file should exist!");
    // read the log file and apply it to memory area & cleanup logs in case
    ifstream lfile(lfilename);
    lfile.seekg(0, ios::end);
    size_t size = lfile.tellg();
    lfile.seekg(0, ios::beg);

    char* buffer = new char[size];
    lfile.read(buffer, size);
    lfile.close();

    const char DELIMITER[8] = "\r\n\r\n\r\n\r\n";

    for(int i=0; i<size; i++) {
      int j;
      bool flag = true;
      for(j=i; j<size; j++) {
        if((buffer[j] == '\r') && (memcmp(buffer+j, DELIMITER, 8)==0)) {
          flag = false;
          break;
        }
      }

      if(flag) {
        truncate(lfile.str().c_str(), i);
        break;
      }

      // getting transaction id
      int64_t tid = *((int64_t*)(buffer[i]));
      i += 8;

      // check if transaction id is present in the index

      // if present apply
      while(i < j) {
        int32_t offset = *((int32_t*)(buffer[i]));
        int32_t datasize = *((int32_t*)(buffer[i+4]));
        i += 8;

        memcpy(memory_area+offset, buffer[i], datasize);
        i += datasize;
      }
    }

    delete[] buffer;
  } else {
    ASSERT(!exists_file(lfilename), "log file should not exist!");
    write_to_file(bfilename, memory_area, size_to_create);
    create_file(lfilename);
  }

  return (void*)memory_area;
}

/* unmap a segment from memory */
void rvm_unmap(rvm_t rvm, void *segbase) {
  ASSERT(rvm.memory_to_segname->count(segbase) > 0, "unable to unmap");

  // @todo abort all the corresponding transactions

  // truncate all logs
  rvm_truncate_log(rvm);

  // remove the mapping
  char* segname = (*rvm.memory_to_segname)[segbase];
  rvm.segname_to_memory->erase(segname);
  rvm.memory_to_segname->erase(segbase);

  // free memory
  free(segname);
  free(segbase);
}

/* destroy a segment completely, erasing its backing store.
   This function should not be called on a segment that is currently mapped,
   must call unmap first before calling destroy */
void rvm_destroy(rvm_t rvm, const char *segname) {
  ASSERT(rvm.segname_to_memory->count(segname) > 0, "cannot destroy mapped segment");

  stringstream ss;
  ss<<"rm -f "<<rvm.directory<<"/"<<segname<<".log "<<rvm.directory<<"/"<<segname<<".bak";
  system(ss.str().c_str());
}

/* begin a transaction that will modify the segments listed in segbases.
   If any of the specified segments is already being modified by a transaction,
   then the call should fail and return (trans_t) -1. Note that trant_t needs
   to be able to be typecasted to an integer type */
trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases) {
  for(int i=0; i<numsegs; i++) {
    LinkedListNode* cur_trans = rvm.transactions;
    while(cur_trans) {
      for(std::vector<void*>::iterator iter = ((trans_t*)(cur_trans->pointer))->segbase_pointers->begin();
          iter != ((trans_t*)(cur_trans->pointer))->segbase_pointers->end(); ++iter) {
        if(*iter == segbases[i]) {
          return *(trans_t*)(-1);
        }
      }

      cur_trans = cur_trans->next;
    }
  }

  trans_t *trans = new trans_t();
  for(int i=0; i<numsegs; i++) {
    trans->segbase_pointers->push_back(segbases[i]);
  }

  // add to rvm list
  LinkedListNode *node = new LinkedListNode();
  node->pointer = (void*) trans;
  node->next = rvm.transactions;
  rvm.transactions = node;

  return *trans;
}

/* declare that the library is about to modify a specified range of memory in the
   specified segment. The segment must be one of the segments specified in the call
   to rvm_begin_trans. Your library needs to ensure that the old memory has been
   saved, in case an abort is executed. It is legal call rvm_about_to_modify
   multiple times on the same memory area */
void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size) {
  // check if range lies in the mapped range
  // If offset+size > than len(segment) return -1
  // Append new entry to undo log: Copy value of memory to undo log (see undo log structure)
  // segbase, offset, size, original data
}

/* commit all changes that have been made within the specified transaction. When
   the call returns, then enough information should have been saved to disk so that,
   even if the program crashes, the changes will be seen by the program when it restarts */
void rvm_commit_trans(trans_t tid) {
  // commit the redo logs to persistent memory
  // throw away the undo logs
}

/* undo all changes that have happened within the specified transaction */
void rvm_abort_trans(trans_t tid) {
  // throw away the redo logs
}

/* play through any committed or aborted items in the log file(s) and
   shrink the log file(s) as much as possible */
void rvm_truncate_log(rvm_t rvm) {
  // apply the redo logs on the backing file
}
