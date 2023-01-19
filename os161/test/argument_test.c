/*argument_test - print the passed arguments*/

#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv){

    printf("Passed arguments are:\n");
    for(int i=0; i<argc; i++){
        printf("\t %s\n",argv[i]);
    }
    printf("Test finished\n");
	
    return 0;
}