/*
 * fork_test.c  : test for fork, getpid
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

int main(void){
	
    pid_t pid, pid1, pid2;
    int status;

    printf("Execution of fork\n");
    
    pid = fork();
    if(pid < 0){ 
        printf("Error fork: %s\n",strerror(errno)); 
        return 1;
    }else if(pid == 0){ // Child
        pid1 = getpid();
        printf("This is the child process with pid = %d\n",pid1);
        _exit(0);
    
    }else{ // Parent
        pid2 = getpid();
        waitpid(pid, &status, 0);
        printf("This the parent process with pid = %d that is waiting for its child with pid = %d\n",pid2,pid);
        printf("The child process is exited with exit status %d\n", status);
        return 0;
    }
}
