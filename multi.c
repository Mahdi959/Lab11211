/*
** multi.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define STDIN 0 // file descriptor for standard input

#define PORT 9034 // port we're listening on

int main(void)
{
	fd_set master, // master file descriptor list
		read_fds; // temp file descriptor list for select()
	struct sockaddr_in myaddr, // server address
		remoteaddr; // client address
	int fdmax, // maximum file descriptor number
		listener, // listening socket descriptor
		newfd, // newly accept()ed socket descriptor
		nbytes,
		yes = 1, // for setsockopt() SO_REUSEADDR, below
		i, j;

	char buf[256]; // buffer for client data
	socklen_t addrlen;

	FD_ZERO(&master); // clear the master and temp sets
	FD_ZERO(&read_fds);

	// get the listener
	if ((listener = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	// lose the pesky "address already in use" error message
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	// bind
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = INADDR_ANY;
	myaddr.sin_port = htons(PORT);
	memset(&(myaddr.sin_zero), '\0', 8);
	if (bind(listener, (struct sockaddr *)&myaddr, sizeof(myaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	// listen
	if (listen(listener, 10) == -1) {
		perror("listen");
		exit(1);
	}

	// add the listener to the master set
	FD_SET(listener, &master);

	// keep track of the biggest file descriptor
	fdmax = listener; // so far, it's this one

	// main loop
	for (;;) {
		read_fds = master; // copy it
		if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(1);
		}

		struct timeval tv;
		fd_set readfds;
		tv.tv_sec = 10;
		tv.tv_usec = 500000;
		FD_ZERO(&readfds);
		FD_SET(STDIN, &readfds);

		// don't care about writefds and exceptfds:
		select(STDIN + 1, &readfds, NULL, NULL, &tv); // on return from time out tv.tv_sec = 0,                     
		            //tv.tv_usec =0
		if (FD_ISSET(STDIN, &readfds))
			printf("A key was pressed!\n");
		else
			//printf("Timed out.\n");
			printf("10 seconds have passed.  Still waiting\n");
		//return 0;



		// run through the existing connections looking for data to read
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // we got one!!
				if (i == listener) {

					// handle new connections
					addrlen = sizeof(remoteaddr);
					if ((newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen)) == -1) {
						perror("accept");
					}
					else {
						FD_SET(newfd, &master); // add to master set
						if (newfd > fdmax) { // keep track of the maximum
							fdmax = newfd;
						}
						printf("selectserver: new connection from %s on "
							"socket %d\n", inet_ntoa(remoteaddr.sin_addr), newfd);
					}
				}
				else {
					// handle data from a client
					if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
						// got error or connection closed by client
						if (nbytes == 0) {
							// connection closed
							printf("selectserver: socket %d hung up\n", i);
						}
						else {
							perror("recv");
						}
						close(i); // bye!
						FD_CLR(i, &master); // remove from master set
					}
					else {
						// we got some data from a client
						for (j = 0; j <= fdmax; j++) {
							// send to everyone!
							if (FD_ISSET(j, &master)) {
								// except the listener and ourselves
								if (j != listener && j != i) {
									if (send(j, buf, nbytes, 0) == -1) {
										perror("send");
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return 0;
}
