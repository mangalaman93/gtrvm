#include <stdio.h>
#include "rvm.h"

#define SEGNAME  "testseg"

int main()
{
     rvm_t rvm;
     trans_t trans;
     char* segs[1];

     rvm = rvm_init("rvm_segments");
     rvm_destroy(rvm, SEGNAME);
     segs[0] = (char *) rvm_map(rvm, SEGNAME, 10000);
     segs[0] = (char *) rvm_map(rvm, SEGNAME, 10000);

     if(segs[0] == NULL) {
		printf("Error in rvm_map.\n");
	}
	return 0;
}
