/*dir_test
* 1) getcwd
* 2) chdir
* 3) getcwd
*
*/


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

int main(){
    int rval;
	char newpath[PATH_MAX] = "emu0:/testbin";

	rval = chdir(newpath);
	if(rval == -1){
		printf("Error newpath: %s\n",strerror(errno));
		return -1;
	}
	
    printf("The current dir is %s\n",path2);
    printf("Test finished\n");

    return 0;
}
