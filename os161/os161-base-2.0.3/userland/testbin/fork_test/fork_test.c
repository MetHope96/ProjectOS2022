/*
 * fork_test.c  : test for fork, getpid
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>

int main ()
{

    pid_t pid1, pid2, pid3;
    int i;

        
        pid1 = fork();
        
        if(pid1 <0)
        {
            printf ("Error!\n");        
        }
        else if (pid1 == 0 )
        {
           printf ("%d\n", getpid());
           _exit(0);
        }
        else 
        {
            pid2 = fork();
            if(pid2 <0)
            {
                printf ("Error!\n");        
            }
            else if (pid2 == 0 )
            {
            printf ("%d\n", getpid());
             _exit(0);
            }
            else{
                pid3 = fork();
                if(pid3 <0)
                {
                printf ("Error!\n");        
                }
                else if (pid3 == 0 )
                {
                printf ("%d\n", getpid());
                _exit(0);
                }
                else{
                printf ("%d\n", getpid());
                while (1);
                  
                }

            
            }
        }

    return 0;
}
