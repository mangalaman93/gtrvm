/*struct rvm_t {
};
Why should rvm_t be a struct?
*/
typedef const char* rvm_t;

struct trans_t {
  //int tid;
  rvm_t rvm;
  int numsegs;
  void** segbase;
};
