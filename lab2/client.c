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

	struct sockaddr_in serv_addr;
	int socketFD;

	memset(&serv_addr, 0, sizeof(serv_addr));

	struct hostent* host = gethostbyname("senna");
	serv_addr.sin_family = AF_INET;
	// char ** -> in_addr_t
	// inet_addr() takes char* IP address from dotted-decimal notation to
	// 		to Big Edian, network byte order 32 bits
	// inet_ntoa() takes in in_addr(hence casting struct in_addr*)
	// 		returns char* IP address
	// sin_addr.s_addr == struct in_addr of in_addr_t type IPv4 addresses
	// h_addr_list == char**, list of addresses
	serv_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list));
	serv_addr.sin_port = htons(port);

	if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) == -1) error_exit("socket");

	// connect
	if(connect(socketFD, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		close(socketFD);
		error_exit("connect");
	}

	write(STDOUT_FILENO, "connected to server...\n\n", sizeof("connected to server...\n\n"));

	// chat
	int rbyte, kbyte;
	char rbuf[1024], kbuf[1024];
	while(1) {
		if (write(STDOUT_FILENO, "> ", 3) == -1)
			error_exit("> write");

		// read from the keyboard
		kbyte = read(STDIN_FILENO, kbuf, sizeof(kbuf));
		// EOF
		if (kbyte == 0) {
			close(socketFD);
			write(STDOUT_FILENO, "hanging up\n", 12);
			exit(0);
		}
		// error
		if (kbyte < 0)
			error_exit("keyboard error");

		// write to the server
		if (send(socketFD, kbuf, kbyte, 0) == -1)
			error_exit("server write");

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

	close(socketFD);
	return 0;
}
