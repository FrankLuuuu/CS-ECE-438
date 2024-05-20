/*
** client.c -- a stream socket client demo
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
#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 1024 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{

	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	FILE *output; 
    char hostname[256];
    char path_to_file[1024];
	char message[4096];
	char port_string[5]; // string port
	int port = 80; // default port
	int step1 = 1; // first iteration into loop
	int step2 = 1; // second iteration into loop
	int offset = 4;

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	if(sscanf(argv[1], "http://%255[^/]/%1023s", hostname, path_to_file) != 2) { // get the components of the url
        fprintf(stderr, "Invalid URL\n");
        return 1;
    }

	sprintf(port_string, "%d", 80);
	char *port_location = strchr(hostname, ':'); // get the requested port
    if(port_location != NULL) { // store the port and remove the port from the hostname
		port_location++;
        port = atoi(port_location);
		strcpy(port_string, port_location);
		port_location--;
        *port_location = '\0'; 
    }

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(hostname, port_string, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);

	printf("Downloading /%s from %s:%d\n", path_to_file, hostname, port); // print info of request
	printf("http_client: connecting to %s\n", s);
	printf("http_client: sending GET\n");
	printf("Host: %s:%d\n", hostname, port);
	printf("http_client: reading response...\n");

	freeaddrinfo(servinfo); // all done with this structure

	snprintf(message, sizeof(message), "GET /%s HTTP/1.1\r\nUser-Agent:\thttp_client\r\nHost:\t%s:%d\r\nConnection:\tKeep-Alive\r\n\r\n", path_to_file, hostname, port);
	send(sockfd, message, strlen(message), 0); // send the request message to server

	if((output = fopen("output", "wb")) == NULL) { // open the output file
	    perror("Failed to open file");
	    exit(1);
	}			

	memset(buf, '\0', MAXDATASIZE);
	while (1) { 
	    numbytes = recv(sockfd, buf, MAXDATASIZE, 0); // receive the message from server
		if(step1) { // for the first loop, check if success
			step1 = 0;
			if(numbytes > 0) {
			    if (!strncmp(buf, "HTTP/1.1 200 OK", 15))
			        printf("Received a 200 OK response.\n");
			    else {
			        printf("Received a non-200 response: %s", buf);
			        return 1;
			    }
			} 
			else { // if the receive failed
			    perror("recv");
			    exit(1);
			}
		}
	    if(numbytes > 0) {
	        if(step2) { // write to the output file
	            step2 = 0;
	            char *start_buf = strstr(buf, "\r\n\r\n") + offset;
	            fwrite(start_buf, 1, strlen(start_buf), output);
	        } 
			else
	            fwrite(buf, 1, numbytes, output);
	        fflush(output);
	    } 
		else
			break;
		memset(buf, '\0', MAXDATASIZE);
	}

	fclose(output);
	close(sockfd);

	return 0;
}
