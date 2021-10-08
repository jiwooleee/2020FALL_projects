/*
 * server.c - a chat server (and monitor) that uses pipes and sockets
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>

// constants for pipe FDs
#define WFD 1
#define RFD 0


void error_exit(const char* error) {
	perror(error);
	exit(-1);
}

/*
 * monitor - provides a local chat window
 * @param srfd - server read file descriptor
 * @param swfd - server write file descriptor
 */
void monitor(int srfd, int swfd) {

  	int rbyte, kbyte;
	char rbuf[1024], kbuf[1024];

	while (1) {
		// read from s2mFDs[RFD]
		rbyte = read(srfd, rbuf, sizeof(rbuf));
		// EOF
		if (rbyte == 0) {
			close(srfd);
			close(swfd);
			exit(0);
		}
		// error
		if (rbyte < 0)
			error_exit("s2m read");

		// write onto display
		if (write(STDOUT_FILENO, rbuf, rbyte) == -1)
			error_exit("display");
		if (write(STDOUT_FILENO, "> ", 3) == -1)
			error_exit("> write");

		// read from keyboard
		kbyte = read(STDIN_FILENO, kbuf, 1024);
		// EOF
		if (kbyte == 0) {
			close(srfd);
			close(swfd);
			exit(0);
		}
		// error
		if (kbyte < 0)
			error_exit("keyboard read");

		//write to m2sFDs[WFD]
		if (write(swfd, kbuf, kbyte) == -1)
			error_exit("m2s write");
	}
}



/*
 * server - relays chat messages
 * @param mrfd - monitor read file descriptor
 * @param mwfd - monitor write file descriptor
 * @param port - TCP port number to use for client connections
 */
void server(int mrfd, int mwfd, int port) {

	struct sockaddr_in serv_addr;
	int socketFD;

	// fills sizeof(serv_addr) bytes of serv_addr with 0
	memset(&serv_addr, 0, sizeof(serv_addr));

	// socket
	if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) == -1) error_exit("socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	// close() doesn't kill the kernel right away == TIME_WAIT
	// if bind() is called during TIME_WAIT, the kernel stays alive longer
	// SOL_SOCKET == socket code level +) there are IP & TCP levels
	// &val == address of the buffer that stores the result
	// returns 0 / -1
	int val = 1;
	setsockopt(socketFD, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));

	// bind(), listen(), accept()
	if(bind(socketFD, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1) error_exit("bind");

	if(listen(socketFD, 10) == -1) error_exit("listen");

	// acceptFD identifies the clients connecting to the server
	struct sockaddr_in client_addr;
	socklen_t addr_size = sizeof(client_addr);
	int acceptFD =  accept(socketFD, (struct sockaddr *)&client_addr, &addr_size);
	if (acceptFD == -1) error_exit("accept");

	int rbyte = 0;
	int cbyte = 0;
	char rbuf[1024], cbuf[1024];

	while (1) {
		// read from the server
		// recv(0) == flag not specified
		cbyte = recv(acceptFD, cbuf, sizeof(cbuf), 0);
		// EOF
		if (cbyte == 0) {
			close(mrfd);
			close(mwfd);
			close(socketFD);
			write(STDOUT_FILENO, "hanging up\n", 12);
			exit(0);
		}
		// error
		if (cbyte < 0)
			error_exit("server read");

		// write to s2mFDs[WFD]
		if (write(mwfd, cbuf, cbyte) == -1)
			error_exit("s2m write");

		// read from m2sFDs[RFD]
		rbyte = read(mrfd, rbuf, sizeof(rbuf));
		// EOF
		if (rbyte == 0) {
			close(mrfd);
			close(mwfd);
			close(socketFD);
			write(STDOUT_FILENO, "hanging up\n", 12);
			exit(0);
		}
		// error
		if (rbyte < 0)
			error_exit("server read");

		// write to the server
		if (send(acceptFD, rbuf, rbyte, 0) == -1)
			error_exit("send");
	}
}


// creates pipe and fork
// parent: server
// child: monitor
int main(int argc, char **argv) {

	// getopt()
	int opt, port;
	while ((opt = getopt(argc, argv, "hp:")) != -1) {
		switch(opt){
			case 'p':
				port = atoi(optarg);
				break;
			case 'h':
				printf("usage: ./server [-h] [-p port#]\n");
				printf("	-h - this help message\n");
				printf("	-p # - the port to use when connecting to the server\n");
				exit(0);
		}
	}

	// for server, s2mFDs[WFD] & m2sFDs[RFD]
	// for monitor, s2mFDs[RFD] & m2sFDs[WFD]
	int s2mFDs[2], m2sFDs[2];

	// pipe
	if ((pipe(s2mFDs) == -1) || (pipe(m2sFDs) == -1))
		error_exit("pipe");

	// fork
	pid_t pid = fork();
	if (pid < 0) error_exit("fork");
	else if (pid == 0) {
		// child
		close(s2mFDs[WFD]);
		close(m2sFDs[RFD]);
		monitor(s2mFDs[RFD], m2sFDs[WFD]);
		close(s2mFDs[RFD]);
		close(m2sFDs[WFD]);
		exit(0);
	}
	else {
		// parent
		close(m2sFDs[WFD]);
		close(s2mFDs[RFD]);
		server(m2sFDs[RFD], s2mFDs[WFD], port);
		close(m2sFDs[RFD]);
		close(s2mFDs[WFD]);
		wait(NULL);
	}

	return 0;
}
