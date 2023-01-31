/*
 * fork_test.c  : test for fork, getpid
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>

int main() {
   
    pid_t pid;
    pid= fork ();
    if (pid ==0)
    {
        printf("I am child, with the PID:%d and my parent ID is : %d\n", getpid());
     }
    else if (pid < 0)
    {
        printf("Child is not successful!\n");
    }
    else {
        printf("I am parent, with the PID:%d and my child ID is: %d\n", getpid(), pid);
    }



   printf("Done!\n");
   return 0;
}