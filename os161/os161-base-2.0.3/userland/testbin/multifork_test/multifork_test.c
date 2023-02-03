/*
 * fork_test.c  : test for fork, getpid
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>


int main(void){
	
    pid_t pid, pid1, pid2;
    int status = 0;

    printf("Execution of first fork\n");
    
    pid = fork();
    if(pid < 0){ 
        printf("Error fork: %s\n",strerror(errno)); 
        return 1;
    }else if(pid == 0){ // Child
        pid1 = getpid();
        printf("This is the child process with pid = %d\n",pid1);
        _exit(0);
    }else{ // Parent
        printf("Execution of second fork\n");
        pid2 = fork();
        if(pid2 < 0){ 
        printf("Error fork: %s\n",strerror(errno)); 
        return 1;
        }else if(pid2 == 0){ // Child
            pid1 = getpid();
            printf("This is the child process with pid = %d\n",pid1);
            _exit(0);
        }else{
        waitpid(pid, &status, 0);
        waitpid(pid2, &status, 0);
        pid1 = getpid();
        printf("This the parent process with pid = %d\n",pid1,);
        return 0;
        }
    }
}
