#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
// strcmp()
#include <string.h>
// socket()
#include <sys/socket.h>
// AF_INET, sockaddr_in
#include <netinet/in.h>
// gethostbyaddr, gethostbyname
#include <netdb.h>
// htons()
#include <arpa/inet.h>
// getopt()
#include <unistd.h>

void error_exit(const char* error) {
	perror(error);
	exit(-1);
}

int main(int argc, char** argv) {

	// getopt()
	int opt, port;
	char* hostname;
	while ((opt = getopt(argc, argv, "h:p:")) != -1) {
		switch(opt){
			case 'p':
				port = atoi(optarg);
				break;
			case 'h':
				hostname = optarg;
				break;
		}
	}

	struct sockaddr_in serv_addr;
	int socketFD;

	memset(&serv_addr, 0, sizeof(serv_addr));

	struct hostent* host = gethostbyname(hostname);
	if (host == 0)
		error_exit("gethostbyname");
	serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr, host->h_addr, host->h_length);
	serv_addr.sin_port = htons(port);

	if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) == -1) error_exit("socket");

	// connect
	if(connect(socketFD, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		close(socketFD);
		error_exit("connect");
	}

	write(STDOUT_FILENO, "connected to server...\n\n", sizeof("connected to server...\n\n"));

	// chat

	fd_set readFDs;
	int fdmax = socketFD;
	int rbyte, kbyte;
	char rbuf[1024], kbuf[1024];
	while(1) {

		FD_ZERO(&readFDs);
		FD_SET(STDIN_FILENO, &readFDs);
		FD_SET(socketFD, &readFDs);
		struct timeval tv;
		tv.tv_sec = 0;
		// timeout == 0.1 second
		tv.tv_usec = 100000;
		if (select(fdmax + 1, &readFDs, 0, 0, &tv) == -1)
			error_exit("select client");

		for (int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &readFDs)) {
				// STDIN_FILENO is set
				if (i == STDIN_FILENO) {
					// read from the keyboard
					if ((kbyte = read(STDIN_FILENO, kbuf, sizeof(kbuf))) == -1)
						error_exit("keyboard error");
					// EOF
					if (kbyte == 0) {
						close(socketFD);
						write(STDOUT_FILENO, "hanging up\n", 12);
						exit(0);
					}
					else {
						// write to the server
						if (send(socketFD, kbuf, kbyte, 0) == -1)
							error_exit("server write");
					}
				}

				// socketFD is set
				if (i == socketFD) {
					// read from the server
					rbyte = recv(socketFD, rbuf, sizeof(rbuf), 0);
					// EOF
					if (rbyte == 0) {
						close(socketFD);
						write(STDOUT_FILENO, "hanging up\n", 12);
						exit(0);
					}
					// error
					if (rbyte < 0)
						error_exit("server read");

					// write onto display
					if (write(STDOUT_FILENO, rbuf, rbyte) == -1)
						error_exit("display write");
				}
			}
		}
	}

	close(socketFD);
	return 0;
}
