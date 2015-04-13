/* basic.c - test that basic persistency works */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

#define TEST_STRING "hello, world"
#define OFFSET2 1000


/* proc1 writes some data, commits it, then exits */
void proc1(int n_write)
{
   int i;
   char str[1000000];
   char j[15];
   strcpy(str,"0");//
   for (i=0;i<n_write;i++) {
     sprintf(j,"%d ",i);//conver i to  string
     strcat(str,j);//concate string(i) to str
   }
   FILE *f = fopen("file.txt", "w");
   if (f == NULL){printf("Error opening file!\n");
    exit(1);}
   fprintf(f, "%s",str);
   fclose(f);
   abort();
}

/* proc2 opens the segments and reads from them */
void proc2(int n_write)
{
     //read int from file
     FILE *o = fopen("file.txt", "rb");
     int n  = 0;
     while (!feof (o)){fscanf (o, "%d", &n);}
     fclose(o);
     if (n == (n_write-1)){
        printf("OK");
        fflush(stdout);
     }else{
        printf("%d",n);}
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
