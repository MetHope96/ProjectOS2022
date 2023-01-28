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
    char *path_ret;
    int rval;
    char path1[PATH_MAX];
	char path2[PATH_MAX];
	char newpath[PATH_MAX] = "emu0:/testbin";

    path_ret = getcwd(path1, sizeof(path1));
	if(path_ret == NULL){
		printf("Error getcwd: %s\n",strerror(errno));
		return -1;
	}

    printf("The current dir is %s\n",path1);

	rval = chdir(newpath);
	if(rval == -1){
		printf("Error newpath: %s\n",strerror(errno));
		return -1;
	}

    path_ret = getcwd(path2, sizeof(path2));
	if(path_ret == NULL){
		printf("Error getcwd: %s\n",strerror(errno));
		return -1;
	}

    printf("The current dir is %s\n",path2);
    printf("Test finished\n");

    return 0;
}
