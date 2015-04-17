/* resize.c - test if it's resizing the segments*/

#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define SEGNAME0  "testseg1"
#define SEGNAME1  "testseg2"

#define OFFSET0  10
#define OFFSET1  100

#define STRING0 "hello, world"
#define STRING1 "black agagadrof!"


void proc1()
{
     rvm_t rvm;
     char* segs[2];
     trans_t trans;

     rvm = rvm_init("rvm_segments");

     rvm_destroy(rvm, SEGNAME0);

     segs[0] = (char*) rvm_map(rvm, SEGNAME0, 1000);

     trans = rvm_begin_trans(rvm, 2, (void **)segs);

     rvm_about_to_modify(trans, segs[0], OFFSET0, 100);
     strcpy(segs[0]+OFFSET0, STRING0);
     rvm_commit_trans(trans);
     rvm_truncate_log(rvm);

     abort();
}


void proc2()
{

     printf("Before Resizing:\n");
     system("ls -l rvm_segments");
     rvm = rvm_init("rvm_segments");

     segs[0] = (char*) rvm_map(rvm, SEGNAME0, 10000);

     printf("\nAfter Resizing:\n");
     system("ls -l rvm_segments");
}


int main(int argc, char **argv)
{
     int pid;

     pid = fork();
     if(pid < 0) {
	  perror("fork");
	  exit(2);
     }
     if(pid == 0) {
	  proc1();
	  exit(0);
     }

     waitpid(pid, NULL, 0);

     proc2();

     return 0;
}

