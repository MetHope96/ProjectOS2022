/*
* rw_filetest.c
* 1) Create and write wr_test.txt
* 2) Read in RDONLY mode wr_test.txt
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(){
    char file_name[20] = "wr_test.txt";
    char w_string[50] = "The pen is on the table";
    char r_string[50];
    int fd, rval;
    	
    fd = open(file_name, O_CREAT|O_WRONLY);
	if (fd < 0) {
		printf("Error open: %s\n",strerror(errno));
		return -1;
	}

    rval = write(fd, w_string, strlen(w_string));
	if (rval<0) {
		printf("Error write: %s\n",strerror(errno));
		return rval;
	}
	printf("%s is written on file %s\n",w_string,file_name);

    rval = close(fd);
	if (rval<0) {
		printf("Error close: %s\n",strerror(errno));
		return rval;
	}

	fd = open(file_name, O_RDONLY);
	if (fd<0) {
		printf("Error open: %s\n",strerror(errno));
		return -1;
	}

	rval = read(fd, r_string, sizeof(r_string));
	if (rval<0) {
		printf("Error read: %s\n",strerror(errno));
		return rval;
	}

	rval = close(fd);
	if (rval<0) {
		printf("Error close: %s\n",strerror(errno));
		return rval;
	}


    return 0;
}