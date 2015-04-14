/* crash.c - test that checks persistency on crash */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include "rvm.h"

static int *glob_var;

char * makeString(int max){
   int i;
   char *test_string = (char*) malloc( sizeof (char) * 1000000);
   char j[15];
   strcpy(test_string,"");
   for (i=0;i<max;i++) {
     sprintf(j,"%d ",i);
     strcat(test_string,j);
   }
   return test_string;
}

int lastInt(char *string){
  char* token=strtok(string, " ");
  char* last;
  while (token){last=token;
                token=strtok(NULL," ");}
  int n=atoi(last);
  return n;
}

/* proc1 writes some data, commits it, then exits */
void proc1(int n_write)
{

     rvm_t rvm;
     trans_t trans;
     char* segs[1];

     rvm = rvm_init("rvm_segments");
     rvm_destroy(rvm, "int_string");

     segs[0] = (char *) rvm_map(rvm, "int_string", 1000000);

     trans = rvm_begin_trans(rvm, 1, (void **) segs);

     rvm_about_to_modify(trans, segs[0], 0, 10);
     sprintf(segs[0], "0");
     rvm_commit_trans(trans);

     trans = rvm_begin_trans(rvm, 1, (void **) segs);
     rvm_about_to_modify(trans, segs[0], 0, 1000000);
     sprintf(segs[0], makeString(n_write));

     //make this atomic?
     rvm_commit_trans(trans);
     *glob_var=n_write-1;
     //make this atomic?
     abort();

}

/* proc2 opens the segments and reads from them */
void proc2(int n_write)
{
    char* segs[1];
    rvm_t rvm;
    rvm = rvm_init("rvm_segments");
    segs[0] = (char *) rvm_map(rvm, "int_string", 1000000);
    int n = lastInt(segs[0]);
    if (glob_var == n){
       printf("OK");
    }else{
       if (glob_var==1){
         //we crashed
         if (n==0){
           //we recovered from crash
           printf("crash: OK");
	 }else{
           //we actually crashed
           printf("error:%d",n);
         }
       }
    }
    exit(0);
}


int main(int argc, char **argv)
{
     if(argc!=2){printf( "usage: %s max_int\n", argv[0]);
         return -1;}
     int pid;
     int n = atoi(argv[1]);
     glob_var = mmap(NULL, sizeof *glob_var, PROT_READ | PROT_WRITE, 
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
     *glob_var=1;
     pid = fork();
     if(pid < 0) {perror("fork");
	          exit(2);}
     if(pid == 0){proc1(n);
	          exit(0);}
     //waitpid(pid,NULL,0);
     sleep(1);
     kill(pid,SIGKILL);

     proc2(n);

     return 0;
}
