/*
server.c -- a stream socket server demo
CIS3210 A1 F20 
Adrian Clinansmith 1028785
October 2020

This is an edited version of code from:
Beej’s Guide to Network Programming Using Internet Sockets (pp 30-33)
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h> 
#include <fcntl.h>
#include <semaphore.h>

#define PORT "28785" // the port users will be connecting to
#define BACKLOG 10	 // how many pending connections queue will hold
#define BUFSIZE 4096 // max number of bytes we can get at once 

void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa); 		// get sockaddr, IPv4 or IPv6
int recvall(int their_sockfd, size_t bufsize);	// receive all bytes from the connector

//****************************************
// Main 
//****************************************

int main(int argc, char *argv[])
{
	int my_sockfd, their_sockfd;  			// listen on my_sockfd, new connection on their_sockfd (file descriptors)
	struct addrinfo hints, *servinfo, *p;	// address info: IP family, socket type, address to bind to 
	struct sockaddr_storage their_addr; 	// connector's address information
	struct sigaction sa;					// used to reap zombie processes as the forked children exit.
	char their_addr_str[INET6_ADDRSTRLEN];	// connector's IP address as a string, made big enough to hold IPv6

	size_t bufsize = BUFSIZE;

	if (argc == 2 && strlen(argv[1]) < 6) {
		int arg = atoi(argv[1]);
		bufsize = arg > 9 ? arg : BUFSIZE;
	}
	printf("server: buffer %zu (min 10, max 99999, default %d)\n", bufsize, BUFSIZE);

	
	memset(&hints, 0, sizeof hints);	// load up hints with address criteria
	hints.ai_family = AF_UNSPEC;		// accept IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM;	// use TCP rather than UDP
	hints.ai_flags = AI_PASSIVE;		// listen on localhost (127.0.0.1)

	// get a list of addresses that match the criteria in hints
	int rv; 
	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		// get an unbound socket (an address without a port) file descriptor
		if ((my_sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		// allow reuse of the same socket so that closing and rerunning the application
		// won't result in an "address already in use" error
		int yes = 1;
 		if (setsockopt(my_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		// bind the socket to a port (socket = address + port)
		if (bind(my_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(my_sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		freeaddrinfo(servinfo);
		exit(1);
	}

	// convert the IP to a string and print it
	char addrstr[INET6_ADDRSTRLEN];
	struct sockaddr* addr = get_in_addr((struct sockaddr *)p->ai_addr);
	if (inet_ntop(p->ai_family, addr, addrstr, INET6_ADDRSTRLEN)) {
		printf("server: listening on %s\n", addrstr);
	}
	
	freeaddrinfo(servinfo); // all done with this structure

	// listen for incoming connections & hold up to BACKLOG connections in a queue
	if (listen(my_sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	// create a semaphore name so it can be referenced by child processes
	srand((unsigned) time(NULL));
	int r = rand();
	char semname[20];
	sprintf(semname, "/sem%d", r % 100000000);
	printf("server: using semaphore named %s\n", semname);

	// the semaphore will allow incoming files to be be displayed one at a time
	sem_t* sem = sem_open(semname, O_CREAT, O_RDWR, 1);
	if (sem == SEM_FAILED) {
		fprintf(stderr, "server: failed to create named semaphore, %s may be in use.\n", semname);
		return 1;
	}
	sem_unlink(semname);	// remove the semaphore after all processes finish

	printf("server: waiting for connections...\n");

	// main accept() loop
	while(1) {  
		// accept an incoming connection & get connector socket's file descriptor
		socklen_t their_size = sizeof their_addr;
		their_sockfd = accept(my_sockfd, (struct sockaddr *)&their_addr, &their_size);
		if (their_sockfd == -1) {
			perror("accept");
			continue;
		}

		// network to presentation: print the connector's address as a readable string
		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), their_addr_str, sizeof their_addr_str);
		printf("server: got connection from %s\n", their_addr_str);

		if (!fork()) { 			// this is the child process
			close(my_sockfd); 	// child doesn't need the listener
			sem_wait(sem); 		// wait for previous children to finish sending
			int ret = recvall(their_sockfd, bufsize);	// receive all the text from the client
			sem_post(sem); 	// give access to the next waiting child
			exit(ret);		// the child exits
		}

		close(their_sockfd);  // parent doesn't need this anymore
	}

	close(my_sockfd);
	return 0;
}

//****************************************
// Helper Functions 
//****************************************

void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int recvall(int their_sockfd, size_t bufsize) {
	int numbytes;	// number of bytes received on each iteration
	char* buf = malloc(bufsize);

	if (!buf) {
		perror("malloc");
		close(their_sockfd);
		return 1;
	}

	printf("server: received...\n\n");

	// receive bytes until the connection closes, i.e. recv() returns 0 or -1 on error
	while ((numbytes = recv(their_sockfd, buf, bufsize, 0)) > 0) {
		fwrite(buf, sizeof(char), numbytes, stdout);
	}
	printf("\n\n");

	if (numbytes == -1) {
		perror("recv");
	}
	close(their_sockfd);
	free(buf);
	return numbytes;	
}