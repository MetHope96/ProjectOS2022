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

int main(int argc, char **argv){
	
    pid_t pid;
    int rval, status;
    const char *prog = "testbin/helloworld";

    printf("Execution fork\n");

    pid = fork();

    if(pid < 0){ // Error
        printf("Error fork: %s\n",strerror(errno));
        return 1;
    }else if(pid == 0){ // Child
        printf("Child process execution of execv\n");
        
        rval = execv(prog, 0);
        if (rval == -1){
            printf("Error execv: %s\n",strerror(errno));
            return rval;
        }
    }else{ // Parent
        waitpid(pid, &status, 0);
        printf("This is printed the parent process. Status is %d, the child pid is %d.\n",status,pid);
    }

	return 0;
}