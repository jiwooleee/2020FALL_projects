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

// maximum number of clients
#define MAX_CLIENT 10

// exit on error
void error_exit(const char* error);

// FOR SERVER()
// sends message to all clients
void broadcast(int receiver, int sender, int socketFD, int recv_bytes, const char* recv_buf, fd_set* OGFDs);

// sends & receives message, remove client from readFDs on EOF
void send_recv(int client, int mrfd, int mwfd, fd_set* OGFDs, int clientFDs[], int socketFD, int* fdmax, int* clientNum, int clientInd);

// wait for connection
void connect_wait(int* socketFD, struct sockaddr_in* serv_addr, int port);

// accept connections, add new fd to fd_set readFDs, add new client to client list
void connect_accept(fd_set *OGFDs, int clientFDs[], int* clientNum, int* fdmax, int socketFD);

/*
 * monitor - provides a local chat window
 * @param srfd - server read file descriptor, s2mFDs[RFD]
 * @param swfd - server write file descriptor, m2sFDs[WFD]
 */
void monitor(int srfd, int swfd) {

	fd_set readFDs;
  	int rbyte, kbyte;
	char rbuf[1024], kbuf[1024];
	// STDIN_FILENO == 0
	int fdmax = srfd;

	do {

		// select()
		FD_ZERO(&readFDs);
		FD_SET(STDIN_FILENO, &readFDs);
		FD_SET(srfd, &readFDs);
		if (select(fdmax + 1, &readFDs, 0, 0, 0) == -1)
			error_exit("select monitor");

		for (int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &readFDs)) {
				// s2mFDs[RFD] is set
				if (i == srfd) {
					// read error
					if ((rbyte = read(srfd, rbuf, 1024)) < 0)
						error_exit("s2m read");
					else {
					// write to display
						if (write(STDOUT_FILENO, rbuf, rbyte) == -1)
							error_exit("display monitor");
					}
				}

				// STDIN_FILENO is set
				if (i == STDIN_FILENO) {
					// read from keyboard
					kbyte = read(STDIN_FILENO, kbuf, sizeof(kbuf));
					// EOF
					if (kbyte == 0) {
						exit(0);
					}
					else if (kbyte < 0) {
						error_exit("keyboard");
					}

					//write to m2sFDs[WFD]
					if (write(swfd, kbuf, kbyte) == -1)
						error_exit("m2s write");

				}

			}
		}
	} while(1);
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
	memset(&serv_addr, 0, sizeof(serv_addr));
	connect_wait(&socketFD, &serv_addr, port);

	int rbyte, cbyte;
	char rbuf[1024], cbuf[1024];

	int clientFDs[MAX_CLIENT] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
	int clientNum = 0;

	fd_set OGFDs, readFDs;
	FD_ZERO(&OGFDs);
	int fdmax = (mrfd > socketFD) ? mwfd : socketFD;

	// do until the monitor sends EOF
	do {
		// reset readFDs
		FD_ZERO(&readFDs);
		readFDs = OGFDs;
		FD_SET(mrfd, &readFDs);
		FD_SET(socketFD, &readFDs);

		struct timeval tv;
		tv.tv_sec = 0;
		// 0.1 second
		tv.tv_usec = 100000;
		if (select(fdmax + 1, &readFDs, 0, 0, &tv) == -1)
			error_exit("select server");

		for (int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &readFDs)) {
				// socketFD is set
				if (i == socketFD) {
					if (clientNum < MAX_CLIENT)
						connect_accept(&OGFDs, clientFDs, &clientNum, &fdmax, socketFD);
				}
				// m2sFDs[RFD] is set
				if (i == mrfd) {
					if ((rbyte = read(mrfd, rbuf, 1024)) <= 0) {
						// error
						if (rbyte < 0)
							error_exit("read from monitor server");
						// rbyte == 0, EOF
						else {
							close(socketFD);
							write(STDOUT_FILENO, "hanging up\n", 12);
							exit(0);
						}
					}
					// rbyte > 0
					else {
						for (int i = 0; i < MAX_CLIENT; i++)
							broadcast(clientFDs[i], socketFD, socketFD, rbyte, rbuf, &OGFDs);
					}

				}

				// is it one of the clients?
				for (int j = 0; j <MAX_CLIENT; j++) {
						if (i == clientFDs[j])
							send_recv(i, mrfd, mwfd, &OGFDs, clientFDs, socketFD, &fdmax, &clientNum, j);
				}

			}
		}

	} while(1);

}

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
		wait(0);
	}

	return 0;
}


void error_exit(const char* error) {
	perror(error);
	exit(-1);
}

/*
 * connect_wait - calls listen() and bind()
 * @param socketFD - socketFD made in server()
 * @param serv_addr - holds IP + port
 * @param port - port number
 */
void connect_wait(int *socketFD, struct sockaddr_in* serv_addr, int port) {

	if ((*socketFD = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		error_exit("socket");

	serv_addr->sin_family = AF_INET;
	serv_addr->sin_addr.s_addr = INADDR_ANY;
	serv_addr->sin_port = htons(port);

	int val = 1;
	setsockopt(*socketFD, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));

	if(bind(*socketFD, (struct sockaddr*) serv_addr, sizeof(struct sockaddr)) == -1)
		error_exit("bind");

	if(listen(*socketFD, 10) == -1)
		error_exit("listen");

}

/*
 * connect_accept - accept clients when FD_ISSET(socketFD)
 * 		  - increments clientNum
 * 		  - adds clientFD to OGFDs and clientFDs
 * 		  - update fdmax
 *
 * @param OGFDs - stores file descriptors currently available to read from
 * 		- only used to update readFDs, call select() on readFDs
 * @param clientFDs - list of clients currently in the chat
 * @param clientNum - number of clients currently in the chat
 * @param fdmax - maximum fd value in OGFDs
 * @param socketFD - socket made in server()
 */
void connect_accept(fd_set *OGFDs, int clientFDs[], int *clientNum, int *fdmax, int socketFD)
{
	struct sockaddr_in client_addr;
	socklen_t addr_size = sizeof(client_addr);
	int acceptFD;

	if ((acceptFD = accept(socketFD, (struct sockaddr *)&client_addr, &addr_size)) == -1)
		error_exit("accept");

	//connect message
	//printf("Client connected from %s...\n", inet_ntoa(client_addr.sin_addr));
	// add it to the client FD list
	FD_SET(acceptFD, OGFDs);
	// increment clientNum
	(*clientNum)++;
	// update fdmax
	if (acceptFD > *fdmax)
		*fdmax = acceptFD;

	// add it to the clientFDs array
	for (int i = 0; i < MAX_CLIENT; i++) {
		if (clientFDs[i] == -1) {
			clientFDs[i] = acceptFD;
			return;
		}
	}
}

/*
 * send_recv - recv() from the client whose fd is ready to be read from
 *	     - on receiving 0 < bytes, error_exit
 *	     - on receiving 0 byte, remove client from OGFDs & clientFDs, update fdmax
 *	     - send() to other clients in the chat by broadcast()
 * @param client - client FD
 * @param mwfd - s2mFDs[WFD]
 * @param OGFDs - stores FDs currently available to read from
 * @param clientFDs - list of clients currently in the chat
 * @param socketFD - socket fd made in server()
 * @param fdmax - maximum value of fd in OGFDs
 */
void send_recv(int client, int mrfd, int mwfd, fd_set* OGFDs, int clientFDs[], int socketFD, int* fdmax, int* clientNum, int clientInd) {
	int recv_bytes;
	char recv_buf[1024];
	if ((recv_bytes = recv(client, recv_buf, 1024, 0)) <= 0) {
		if (recv_bytes < 0) {
			error_exit("receive send_recv");
		}
		else {
			// client hung up
			close(client);
			clientFDs[clientInd] = -1;
			FD_CLR(client, OGFDs);
			(*clientNum)--;
			//printf("The client has disconnected");
			// update fdmax
			if (client == *fdmax) {
				if (*clientNum == 0)
					*fdmax  = mrfd;
				else {
					for (int i = 0; i < MAX_CLIENT; i++) {
						if (clientFDs[i] > *fdmax)
							*fdmax = clientFDs[i];
					}
				}
			}
		}
	}
	// there's something to read
	else {
		for (int i = 0; i < MAX_CLIENT; i++) {
			broadcast(clientFDs[i], client, socketFD, recv_bytes, recv_buf, OGFDs);
		}

	}
		if (write(mwfd, recv_buf, recv_bytes) == -1)
		       error_exit("write swfd send_recv");
}


/*
 * broadcast - sends the received message to every client except monitor and the sender
 * @param receiver - a client in clientFds
 * @param sender - the client who sent the message
 * @param socketFD - the monitor socket fd
 * @param recv_bytes - # of bytes of message received
 * @param recv_buf - received message from the sender
 * @param OGFDs - stores fds currently available to read
 */
void broadcast(int receiver, int sender, int socketFD, int recv_bytes, const char* recv_buf, fd_set* OGFDs){

	// is the receiver still in the chat
	// catch clientFDs[i] == -1
	if (FD_ISSET(receiver, OGFDs)) {
		// catch if the receiver is the monitor OR the sender
		if ((receiver != socketFD) && (receiver != sender)) {
			if (send(receiver, recv_buf, recv_bytes, 0) == -1)
				error_exit("send broadcast");
		}
	}
}
