/*
* lseek_test.c
* 1) Create and write wr_test.txt
* 2) write the letter 'lol' at different position after lseek 
* 3) Read in RDONLY mode wr_test.txt
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(){
    char file_name[20] = "wr_test.txt";
    char w_string[50] = "The pen is on the table";
    char l_string[10] = "lol";
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

    rval = lseek(fd, -1, SEEK_END); // this moves the pointer 1 positions back from the end of the file
    printf("1)lseek, new position = %d\n ",rval);
    rval = write(fd, l_string, strlen(l_string));

    rval = lseek(fd,3,SEEK_SET); // this moves the pointer 3 positions ahead starting from the beginning of the file
    printf("2)lseek, new position = %d\n ",rval);
    rval = write(fd, l_string, strlen(l_string));

    rval = lseek(fd,5,SEEK_CUR); // this moves the pointer 5 positions ahead from the current position in the file
    printf("3)lseek, new position = %d\n ",rval);
    rval = write(fd, l_string, strlen(l_string));
    
    rval = lseek(fd,-1,SEEK_CUR); // this moves the pointer -1 positions back from the current position in the file
    printf("4)lseek, new position = %d\n ",rval);
    rval = write(fd, l_string, strlen(l_string));

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

	printf("The string read: %s\n",r_string);

	rval = close(fd);
	if (rval<0) {
		printf("Error close: %s\n",strerror(errno));
		return rval;
	}

	printf("Test finished\n");
	
    return 0;
}