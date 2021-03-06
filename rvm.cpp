#include "rvm.h"

// global variables
int base_rvm_id = 0;
int base_trans_id = 0;
map<trans_t, trans_int_t*> transactions;
map<rvm_t, rvm_int_t*> rvms;

// prefix for all the files
#define DIR_PREFIX ""

// delimiter
const char DELIMITER[9] = "\r\n\r\n\r\n\r\n";

// logs current position in file
#define LOG_POS() do {                                  \
  printf("line:%d in file:%s\n", __LINE__, __FILE__);   \
} while(0);

// throws error if condition is false
#define ASSERT(condition, message)          \
if(!(condition)) {                          \
  printf("%s\n", message);                  \
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

/* Initialize the library with the specified directory as backing store */
rvm_t rvm_init(const char *directory) {
  rvm_int_t* r = new rvm_int_t(directory);
  base_rvm_id = base_rvm_id + 1;
  rvms[base_rvm_id] = r;

  // initialize the log segment and return it
  stringstream ss;
  ss<<DIR_PREFIX<<directory;
  mkdir(ss.str().c_str(), 0755);

  return base_rvm_id;
}

/* map a segment from disk into memory. If the segment does not already exist,
   then create it and give it size size_to_create. If the segment exists but
   is shorter than size_to_create, then extend it until it is long enough.
   It is an error to try to map the same segment twice */
void* rvm_map(rvm_t rvm, const char *segname, int size_to_create) {
  char* segname_copy = new char[strlen(segname)+1];
  strcpy(segname_copy, segname);

  // getting pointer to internal rvm data structure
  rvm_int_t *r = rvms[rvm];

  // should be unmapped
  if(r->segname_to_memory->count(segname_copy) > 0) {
    delete[] segname_copy;
    return (*r->segname_to_memory)[segname];
  }

  // allocate memory area
  char* memory_area = new char[size_to_create];
  bzero(memory_area, size_to_create);
  (*r->segname_to_memory)[segname_copy] = memory_area;
  segment_t *seg = new segment_t(size_to_create, segname_copy);
  (*r->memory_to_struct)[memory_area] = seg;

  // backing file name
  stringstream ss;
  ss<<DIR_PREFIX<<r->directory<<"/"<<segname_copy<<".bak";
  string bs(ss.str());
  const char* bfilename = bs.c_str();

  // log file name
  stringstream st;
  st<<DIR_PREFIX<<r->directory<<"/"<<segname_copy<<".log";
  string ls(st.str());
  const char* lfilename = ls.c_str();

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

      // applying the log to memory area (not backing file)
      while(i < j) {
        int32_t offset = *((int32_t*)(&buffer[i]));
        int32_t datasize = *((int32_t*)(&buffer[i+4]));
        i += 8;

        memcpy(memory_area+offset, buffer+i, datasize);
        i += datasize;
      }

      i += 7;
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
  // getting pointer to internal rvm data structure
  rvm_int_t *r = rvms[rvm];

  // remove the mapping
  map<char*, segment_t*>::iterator it;
  it = r->memory_to_struct->find((char*)segbase);

  // mapping must exists and all transaction must have been commited
  ASSERT(it != r->memory_to_struct->end(), "unable to unmap");
  ASSERT(it->second->modify == -1, "unmapping a segnment which is involved in a transaction!");
  ASSERT(!it->second->undo_logs, "this is not possible!");

  // free memory
  r->memory_to_struct->erase(it);
  r->segname_to_memory->erase(it->second->segname);
  delete[] ((char*)segbase);
  delete[] ((char*)it->second->segname);
  delete it->second;
}

/* destroy a segment completely, erasing its backing store.
   This function should not be called on a segment that is currently mapped,
   must call unmap first before calling destroy */
void rvm_destroy(rvm_t rvm, const char *segname) {
  // getting pointer to internal rvm data structure
  rvm_int_t *r = rvms[rvm];

  if(r->segname_to_memory->count(segname) > 0) {
    return;
  }

  stringstream ss;
  ss<<"rm -f "<<DIR_PREFIX<<r->directory<<"/"<<segname<<".log "<<DIR_PREFIX<<r->directory<<"/"<<segname<<".bak";
  system(ss.str().c_str());
}

/* begin a transaction that will modify the segments listed in segbases.
   If any of the specified segments is already being modified by a transaction,
   then the call should fail and return (trans_t) -1. Note that trant_t needs
   to be able to be typecasted to an integer type */
trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases) {
  // getting pointer to internal rvm data structure
  rvm_int_t *r = rvms[rvm];

  for(int i=0; i<numsegs; i++) {
    map<char*, segment_t*>::iterator it;
    it = r->memory_to_struct->find((char*)segbases[i]);

    if(it->second->modify != -1) {
      return -1;
    } else {
      ASSERT(!it->second->undo_logs, "this is not possible!");
      it->second->modify = base_trans_id + 1;
    }
  }

  trans_int_t *trans = new trans_int_t(r);
  base_trans_id += 1;
  transactions[base_trans_id] = trans;
  for(int i=0; i<numsegs; i++) {
    trans->segbase_pointers->push_back((char*)segbases[i]);
  }

  return base_trans_id;
}

/* declare that the library is about to modify a specified range of memory in the
   specified segment. The segment must be one of the segments specified in the call
   to rvm_begin_trans. Your library needs to ensure that the old memory has been
   saved, in case an abort is executed. It is legal call rvm_about_to_modify
   multiple times on the same memory area */
void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size) {
  // getting pointer to internal trans_t data structure
  trans_int_t *t = transactions[tid];

  // getting pointer to internal rvm data structure
  rvm_int_t *r = t->rvm;

  // check if transaction has permission to modify the segment
  if((*r->memory_to_struct)[(char*)segbase]->modify != tid) {
    printf("Trying to modify a segment without asking permission!\n");
    return;
  }

  // check if range lies in the mapped range
  map<char*, segment_t*>::iterator it;
  it = r->memory_to_struct->find((char*)segbase);
  if(it->second->size < offset+size) {
    return;
  }

  // Append new entry to undo log: Copy value of memory to undo log (see undo log structure)
  UndoLog *ul = new UndoLog(offset, size);
  memcpy(ul->data, segbase+offset, size);

  UndoLogNode *node = new UndoLogNode(ul);
  node->next = it->second->undo_logs;
  it->second->undo_logs = node;
}

/* commit all changes that have been made within the specified transaction. When
   the call returns, then enough information should have been saved to disk so that,
   even if the program crashes, the changes will be seen by the program when it restarts */
void rvm_commit_trans(trans_t tid) {
  // getting pointer to internal trans_t data structure
  trans_int_t *t = transactions[tid];

  // getting pointer to internal rvm data structure
  rvm_int_t *r = t->rvm;

  for(vector<char*>::iterator iter=t->segbase_pointers->begin();
      iter!=t->segbase_pointers->end(); ++iter) {
    segment_t *seg = (*r->memory_to_struct)[*iter];

    stringstream ss;
    ss<<DIR_PREFIX<<r->directory<<"/"<<seg->segname<<".log";
    ofstream file(ss.str().c_str(), ios::app);
    while(seg->undo_logs) {
      UndoLog *log = (seg->undo_logs->pointer);

      file.write((char*)(&(log->offset)), 4);
      file.write((char*)(&(log->size)), 4);
      file.write((*iter) + log->offset, log->size);

      UndoLogNode *node = seg->undo_logs;
      seg->undo_logs = node->next;
      delete log;
      delete node;
    }

    file.write(DELIMITER, 8);
    file.flush();
    file.close();
    seg->modify = -1;
  }
}

/* undo all changes that have happened within the specified transaction */
void rvm_abort_trans(trans_t tid) {
  // getting pointer to internal trans_t data structure
  trans_int_t *t = transactions[tid];

  // getting pointer to internal rvm data structure
  rvm_int_t *r = t->rvm;

  for(vector<char*>::iterator iter=t->segbase_pointers->begin();
      iter!=t->segbase_pointers->end(); ++iter) {
    segment_t *seg = (*r->memory_to_struct)[*iter];

    while(seg->undo_logs) {
      UndoLog *log = (seg->undo_logs->pointer);
      memcpy((*iter) + log->offset, log->data, log->size);

      UndoLogNode *node = seg->undo_logs;
      seg->undo_logs = node->next;
      delete log;
      delete node;
    }

    seg->modify = -1;
  }
}

/* play through any committed or aborted items in the log file(s) and
   shrink the log file(s) as much as possible */
void rvm_truncate_log(rvm_t rvm) {
  // getting pointer to internal rvm data structure
  rvm_int_t *r = rvms[rvm];

  DIR *dir;
  struct dirent *ent;
  if((dir = opendir(r->directory)) != NULL) {
    while((ent = readdir(dir)) != NULL) {
      string filename(ent->d_name);
      if(filename.find(".log") == string::npos) {
        continue;
      }

      // log file name
      string ls = string(r->directory) + string("/") + filename;
      const char* lfilename = ls.c_str();

      // backing file name
      string bs = string(r->directory) + string("/") + filename;
      int length = bs.length();
      bs[length-3] = 'b';
      bs[length-2] = 'a';
      bs[length-1] = 'k';
      const char* bfilename = bs.c_str();

      // reading the log file
      ifstream lfile(lfilename);
      lfile.seekg(0, ios::end);
      size_t lsize = lfile.tellg();
      lfile.seekg(0, ios::beg);
      char* buffer = new char[lsize];
      lfile.read(buffer, lsize);
      lfile.close();

      // reading the backing file
      ifstream bfile(bfilename);
      bfile.seekg(0, ios::end);
      size_t bsize = bfile.tellg();
      bfile.seekg(0, ios::beg);
      char* memory_area = new char[bsize];
      bfile.read(memory_area, bsize);
      bfile.close();

      for(unsigned int i=0; i<lsize; i++) {
        unsigned int j;
        bool flag = true;
        for(j=i; j<lsize; j++) {
          if((buffer[j] == '\r') && (memcmp(buffer+j, DELIMITER, 8)==0)) {
            flag = false;
            break;
          }
        }

        if(flag) {
          truncate(lfilename, i);
          break;
        }

        // applying the log to memory area (not backing file)
        while(i < j) {
          int32_t offset = *((int32_t*)(&buffer[i]));
          int32_t datasize = *((int32_t*)(&buffer[i+4]));
          i += 8;

          memcpy(memory_area+offset, buffer+i, datasize);
          i += datasize;
        }

        i += 7;
      }

      write_to_file(bfilename, memory_area, bsize);
      truncate(lfilename, 0);
      delete[] memory_area;
      delete[] buffer;
    }

    closedir(dir);
  }
}
