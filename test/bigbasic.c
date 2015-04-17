/* crash.c - test that checks persistency on crash */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include "rvm.h"

char * makeString(int max){
   int i;
   char *test_string = (char*) malloc( sizeof (char) * 1000000);
   char j[15];
   strcpy(test_string,"");
   for (i=0;i<max;i++) {
     sprintf(j,"%d ",i);
     strcat(test_string,j);
   }
   //printf("%s\n",test_string);
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

     rvm_about_to_modify(trans, segs[0], 0, 1000000-1);
     char* str;
     str=makeString(n_write);
     sprintf(segs[0], str);
     ///printf("%s\n",segs[0]);
     rvm_commit_trans(trans);

     abort();

}

/* proc2 opens the segments and reads from them */
void proc2(int n_write)
{
    char* segs[1];
    rvm_t rvm;
    rvm = rvm_init("rvm_segments");
    segs[0] = (char *) rvm_map(rvm, "int_string", 1000000);
    //printf("%s\n",segs[0]);
    int n = lastInt(segs[0]);
    if (n == (n_write-1)){
        printf("OK\n");
    }else{
        printf("error:%d",n);}
     exit(0);
}


int main(int argc, char **argv)
{
     if(argc!=2){printf( "usage: %s n_writes\n", argv[0]);
         return -1;}
     int pid;
     int n = atoi(argv[1]);

     pid = fork();
     if(pid < 0) {perror("fork");
	          exit(2);}
     if(pid == 0){proc1(n);
	          exit(0);}
     waitpid(pid,NULL,0);//sleep(2);
     //kill(pid,SIGKILL);

     proc2(n);

     return 0;
}
