/*
 * fork_test.c  : test for fork, getpid
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>

#define SUBARGC_MAX 64
static char *subargv[SUBARGC_MAX];

int main(void){

    static char default_prog[] = "/testbin/helloworld";
    subargv[0] = default_prog;
	
    pid_t pid, pid1, pid2;
    int status = 0;
     int rval;

    printf("Execution of fork\n");
    
    pid = fork();
    if(pid < 0){ 
        printf("Error fork: %s\n",strerror(errno)); 
        return 1;
    }else if(pid == 0){ // Child
        pid1 = getpid();
        printf("This is the child process with pid = %d\n and execution of execv",pid1);
           
        
        rval = execv(subargv[0], subargv);
        if (rval == -1){
            printf("Error execv: %s\n",strerror(errno));
            return rval;
        }
    
    }else{ // Parent
        pid2 = getpid();
        waitpid(pid, &status, 0);
        printf("This the parent process with pid = %d that is waiting for its child with pid = %d\n",pid2,pid);
        printf("The child process is exited with exit status %d\n", status);
        return 0;
    }
}
