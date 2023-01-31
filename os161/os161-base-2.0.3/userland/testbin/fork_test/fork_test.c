/*
 * fork_test.c  : test for fork, getpid
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>

int main ()
{    pid_t pid;
    int i;
    for (i=0; i < 3; i++)
    {
        pid= fork();      
        if(pid <0)
        {
            printf ("Error!\n");        
        }
        else if (pid == 0 )
        {
           printf ("I am the child with PID %d, I terminate here!\n", getpid());
            _exit (0);
        }
        else 
        {
            printf ("I am the father with PID %d, my child PID is %d\n", getpid(), pid); 
        }
    }
	return 0;
}