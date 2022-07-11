/*
client.c -- a stream socket client demo
CIS3210 A1 F20 
Adrian Clinansmith 1028785
October 2020

This is an edited version of code from:
Beej’s Guide to Network Programming Using Internet Sockets (pp 33-35)
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/mman.h>      

#include <arpa/inet.h>

#define PORT "5000" // the port client will be connecting to 

void *get_in_addr(struct sockaddr *sa);         // get sockaddr as IPv4 or IPv6
int sendall(int sockfd, char *str, int *len);   // send len characters from str to sockfd

//****************************************
// Main Program 
//****************************************

int main(int argc, char *argv[])
{
	int sockfd;                             // socket file descriptor
	struct addrinfo hints, *servinfo, *p;
	int rv;                                 // return value of getaddrinfo(), used to print an error if need be
	char s[INET6_ADDRSTRLEN];               // string to present the server's IP 

	if (argc != 3) {
	    fprintf(stderr,"usage: client hostname filename\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;        // IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM;    // TCP stream sockets

	if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("client: connect");
			close(sockfd);
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
        freeaddrinfo(servinfo);
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

    // open file to send
    FILE* fileToSend = fopen(argv[2], "r");
    if (fileToSend == NULL) {
        close(sockfd);
        fprintf(stderr,"client: could not open %s\n", argv[2]);
	    exit(1);
    }

    const int BUFSIZE = 1000;
    char buf[BUFSIZE];
    int numread;

    while ((numread = fread(buf, sizeof(char), BUFSIZE, fileToSend))) {

        if (ferror(fileToSend)) {
            fprintf(stderr,"client: an error occured while reading %s\n", argv[2]);
            break;
        }

        if (sendall(sockfd, buf, &numread) == -1) { // send the whole file in chunks
            perror("send");
            break;
        }
    }

    fclose(fileToSend);
    close(sockfd);
	return 0;
}

//****************************************
// Helper Functions 
//****************************************

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int sendall(int sockfd, char *str, int *len) {  // Source: page 53
    int total = 0;          // how many bytes we've sent
    int bytesleft = *len;   // how many we have left to send
    int n;                  // how many were sent on each iteration
    

    while(total < *len) {
        n = send(sockfd, str+total, bytesleft, 0);
        if(n==-1) { 
            break; 
        }
        total += n;
        bytesleft -= n;
    }
    *len = total;                   // return number actually sent here 
    return n==-1?-1:0;              // return -1 on failure, 0 on success
}