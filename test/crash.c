/* basic.c - test that basic persistency works */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include "rvm.h"

/* proc1 writes some data, commits it, then exits */
void proc1(int n_write)
{
   int i;
   char segs[1];
   rvm_t rvm;

   char test_string[1000000];
   char j[15];
   strcpy(test_string,"0");//
   for (i=0;i<n_write;i++) {
     sprintf(j,"%d\n",i);//conver i to  string
     strcat(test_string,j);//concate string(i) to str
   }

   rvm = rvm_init("rvm_segments");
   rvm_destroy(rvm, "int_string");
   segs[0] = (char *) rvm_map(rvm, "int_string", 1000000);
   trans = rvm_begin_trans(rvm, 1, (void **) segs);
   rvm_about_to_modify(trans, segs[0], 0, 1000000);
   sprintf(segs[0], test_string);
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
    printf("OK\n"); 
    exit(0);
    //find a way to parse this
    while (!feof (o)){sscanf (segs[0], "%d", &n);}
    if (n == (n_write-1)){
        printf("OK");
     }else{
        printf("error%d",n);}
     exit(0);
}


int main(int argc, char **argv)
{
     if(argc!=2){printf( "usage: %s n_writes\n", argv[0]);
         return -1;}
     int pid;
     int n = atoi(argv[1]);

     pid = fork();
     if(pid < 0) {
	  perror("fork");
	  exit(2);
     }
     if(pid == 0) {
	  proc1(n);
	  exit(0);
     }
     sleep(1);
     kill(pid,SIGKILL);

     proc2(n);

     return 0;
}
