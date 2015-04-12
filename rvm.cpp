#include "rvm.h"

int base_rvm_id = 0;
int base_trans_id = 0;

// prefix for all the files
#define DIR_PREFIX "/tmp"

// logs current position in file
#define LOG_POS() do {\
  cout<<"line:"<<__LINE__<<" in file:"<<__FILE__<<endl;\
} while(0);

// throws error if condition is false
#define ASSERT(condition, message)     \
if(!condition) {                             \
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
  file.close();
}

void append_to_file(const char* filename, const char* src, int size) {
  ofstream file(filename, ios::app|ios::binary);
  ASSERT(file, "unable to open file for append-ing");

  file.write(src, size);
  ASSERT(file.good(), "unable to write to file");
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
  ss<<"make -p "<<DIR_PREFIX<<"/"<<directory;
  system(ss.str().c_str());

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
  } else {
    ASSERT(!exists_file(lfilename), "log file should not exist!");
    write_to_file(bfilename, memory_area, size_to_create);
    create_file(lfilename);
  }

  return (void*)memory_area;
}

/* unmap a segment from memory */
void rvm_unmap(rvm_t rvm, void *segbase) {
  // make sure that no transaction is uncommited
  // unmap, the details should be available in rvm_t data structure

  // free segment_copy memory_area
}

/* destroy a segment completely, erasing its backing store.
   This function should not be called on a segment that is currently mapped */
void rvm_destroy(rvm_t rvm, const char *segname) {
}

/* begin a transaction that will modify the segments listed in segbases.
   If any of the specified segments is already being modified by a transaction,
   then the call should fail and return (trans_t) -1. Note that trant_t needs
   to be able to be typecasted to an integer type */
trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases) {
}

/* declare that the library is about to modify a specified range of memory in the
   specified segment. The segment must be one of the segments specified in the call
   to rvm_begin_trans. Your library needs to ensure that the old memory has been
   saved, in case an abort is executed. It is legal call rvm_about_to_modify
   multiple times on the same memory area */
void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size) {
  // check range if it lies in the mapped range
  // create undo logs
  // set the portion of segment which is to be modified until commit is called
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
