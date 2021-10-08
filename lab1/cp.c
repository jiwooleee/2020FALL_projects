#include <stdio.h>
#include <stdlib.h>
// open(), stat()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// read(), write(), stat()
#include <unistd.h>
#include <errno.h>

int main(int argc, char** argv) {
	if (argc != 3) {
		printf("usage: ./cp filename1 filename2\n");
		exit(1);
	}

	// get file names
	char* cfrom = argv[1];
	char* cto = argv[2];

	// open the files
	// open() returns nonnegative integers for file descriptors
	// if failed returns -1
	// O_RDONLY, O_WRONLY, O_CREAT are flags
	int input = open(cfrom, O_RDONLY);
	if (input == -1) {
		perror("input open");
		exit(1);
	}

	// get mode_t mode of the input file
	struct stat st;
	mode_t input_mode;
	// check if stat failed
	if (stat(cfrom, &st) == -1) {
		perror("stat");
		exit(1);
	}
	input_mode = st.st_mode;

  // O_WRONLY | O_CREAT: if the file exists, write on it.
	// 		       otherwise make one.
	// mode_t mode: initial 0 indicates octal notation.
	// 		777: the user, the group, and others can
	// 		     read, write, and execute.
	int output = open(cto, O_WRONLY | O_CREAT, st.st_mode);
	if (output == -1) {
		perror("output open");
		close(input);
		exit(1);
	}

	// read & write
	// read(2) puts the data into the buffer first
	int BUFFER_SIZE = 1000;
	char buffer[BUFFER_SIZE];
	ssize_t read_t, write_t;

	// read(2) returns 0 in the end of the file
	while((read_t = read(input, &buffer, BUFFER_SIZE)) > 0){
		write_t = write(output, &buffer, (ssize_t) read_t);
	// check if write(2) copied the right thing
		if (write_t != read_t) {
			perror("write");
			close(input);
			close(output);
			exit(1);
		}
	}

	close(input);
	close(output);

	return 0;
}
