#include "rvm.h"

int base_rvm_id = 0;
int64_t base_trans_id = 0;

// prefix for all the files
#define DIR_PREFIX "/tmp"
#define INDEX_FILE ".index"

// delimiter
const char DELIMITER[9] = "\r\n\r\n\r\n\r\n";

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
  rvm_t rvm(directory);

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
    int temp = (size)%9;
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

      for(unsigned int i=0; i<(size/5); i++) {
        if(buffer[i*9+8] == ',') {
          int64_t temp = *((int64_t*)(buffer[i*9]));

          if(rvm.presentTx.count(temp) > 0) {
            rvm.presentTx[temp] = true;
          }

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

  return rvm;
}

/* map a segment from disk into memory. If the segment does not already exist,
   then create it and give it size size_to_create. If the segment exists but
   is shorter than size_to_create, then extend it until it is long enough.
   It is an error to try to map the same segment twice */
void *rvm_map(rvm_t rvm, const char *segname, int size_to_create) {
  char* segname_copy = new char[strlen(segname)+1];
  strcpy(segname_copy, segname);

  // should be unmapped
  ASSERT(rvm.segname_to_memory->count(segname_copy) > 0,
              "trying to map the same segment again");

  // allocate memory area
  char* memory_area = new char[size_to_create];
  bzero(memory_area, size_to_create);
  (*rvm.segname_to_memory)[segname_copy] = memory_area;
  (*rvm.memory_to_segname)[memory_area] = segname_copy;
  (*rvm.memory_to_size)[memory_area] = size_to_create;

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

    for(unsigned int i=0; i<size; i++) {
      unsigned int j;
      bool flag = true;
      for(j=i; j<size; j++) {
        if((buffer[j] == '\r') && (memcmp(buffer+j, DELIMITER, 8)==0)) {
          flag = false;
          break;
        }
      }

      if(flag) {
        truncate(lfilename, i);
        break;
      }

      // getting transaction id
      int64_t tid = *((int64_t*)(buffer[i]));
      i += 8;

      // check if transaction id is present in the index
      if(rvm.presentTx.count(tid) > 0 && rvm.presentTx[tid]) {
        // nothing
      } else {
        i = j + 8 - 1;
        continue;
      }

      // if present apply
      while(i < j) {
        int32_t offset = *((int32_t*)(buffer[i]));
        int32_t datasize = *((int32_t*)(buffer[i+4]));
        i += 8;

        memcpy(memory_area+offset, buffer+i, datasize);
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

  // remove the mapping
  char* segname = (*rvm.memory_to_segname)[segbase];
  rvm.segname_to_memory->erase(segname);
  rvm.memory_to_segname->erase(segbase);
  rvm.memory_to_size->erase(segbase);

  // free memory
  delete ((char*)segbase);
  delete ((char*)segname);
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
  trans->rvm = &rvm;
  for(int i=0; i<numsegs; i++) {
    trans->segbase_pointers->push_back(segbases[i]);
  }

  // add to rvm list
  LinkedListNode *node = new LinkedListNode();
  node->pointer = (void*) trans;
  node->next = rvm.transactions;
  rvm.transactions = node;

  // add transaction into the index file
  stringstream ss;
  ss<<DIR_PREFIX<<"/"<<rvm.directory<<"/"<<INDEX_FILE;
  append_to_file(ss.str().c_str(), (char*)(&trans->id), 8);

  return *trans;
}

/* declare that the library is about to modify a specified range of memory in the
   specified segment. The segment must be one of the segments specified in the call
   to rvm_begin_trans. Your library needs to ensure that the old memory has been
   saved, in case an abort is executed. It is legal call rvm_about_to_modify
   multiple times on the same memory area */
void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size) {
  // check if range lies in the mapped range
  if((*tid.rvm->memory_to_size)[segbase] <= offset+size) {
    return;
  }

  // Append new entry to undo log: Copy value of memory to undo log (see undo log structure)
  UndoLog *ul = new UndoLog();
  ul->base = segbase;
  ul->offset = offset;
  ul->size = size;

  // copying data;
  char *data = new char[size];
  memcpy(data, segbase, size);
  ul->data = data;

  LinkedListNode *node = new LinkedListNode();
  node->pointer = ul;
  node->next = tid.undo_logs;
  tid.undo_logs = node;
}

/* commit all changes that have been made within the specified transaction. When
   the call returns, then enough information should have been saved to disk so that,
   even if the program crashes, the changes will be seen by the program when it restarts */
void rvm_commit_trans(trans_t tid) {
  while(tid.undo_logs) {
    UndoLog *log = (UndoLog*)(tid.undo_logs->pointer);
    char* segname = (*tid.rvm->memory_to_segname)[log->base];
    stringstream ss;
    ss<<DIR_PREFIX<<"/"<<tid.rvm->directory<<"/"<<segname<<".log";

    ofstream file(ss.str().c_str(), ios::app);
    file.write((char*)(&(tid.id)), 8);
    file.write((char*)(&(log->offset)), 4);
    file.write((char*)(&(log->size)), 4);
    file.write(log->data, log->size);
    file.write(DELIMITER, 8);
    file.flush();
    file.close();

    LinkedListNode *node = tid.undo_logs;
    tid.undo_logs = node->next;
    delete[] ((char*)log->data);
    delete log;
    delete node;
  }

  stringstream ss;
  ss<<DIR_PREFIX<<"/"<<tid.rvm->directory<<"/"<<INDEX_FILE;
  ofstream file(ss.str().c_str(), ios::app);
  file.write((char*)(&(tid.id)), 8);
  file.close();
}

/* undo all changes that have happened within the specified transaction */
void rvm_abort_trans(trans_t tid) {
  // throw away the redo logs & apply undo logs
  LinkedListNode *cur = tid.undo_logs;
  while(cur) {
    UndoLog* ul = (UndoLog*)cur->pointer;
    memcpy(ul->base+ul->offset, ul->data, ul->size);
    delete[] ul->data;

    LinkedListNode *temp = cur;
    cur = cur->next;

    delete ul;
    delete temp;
  }

  // @todo remove transaction from the corresponding rvm
}

/* play through any committed or aborted items in the log file(s) and
   shrink the log file(s) as much as possible */
void rvm_truncate_log(rvm_t rvm) {
  // apply the redo logs on the backing file
}
