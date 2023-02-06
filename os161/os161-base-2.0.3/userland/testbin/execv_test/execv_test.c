/*
 * execv_test.c 
 *      
 *    
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

#define SUBARGC_MAX 64
static char *subargv[SUBARGC_MAX];

int main(){
		
	
    static char default_prog[] = "/testbin/argtest";
    static char arg1_prog[] = "arg1";
    static char arg2_prog[] = "arg2";
    subargv[0] = default_prog;
    subargv[1] = arg1_prog;
    subargv[2] = arg2_prog;

    int rval;
        
        rval = execv(subargv[0], subargv);
        if (rval == -1){
            printf("Error execv: %s\n",strerror(errno));
            return rval;
        }
	return 0;
}
