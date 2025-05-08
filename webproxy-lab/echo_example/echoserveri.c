#include "csapp.h"

void echo(int connfd);

/*Open listening socket, accept client connections, and echo received data*/
int main(int argc, char** argv)
{
    int listenfd, connfd;   // Listening & connected socket descriptor
    socklen_t clientlen;    // Size of client addr struct
    struct sockaddr_storage clientaddr;     // Generic client addr storage (IPv4, IPv6)
    char client_hostname[MAXLINE], client_port[MAXLINE];    // Buffers to store client name and port

    /*Check if port is provided*/
    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);  // Open listening socket on given port
    /*Infinite loop for connections*/
    while(1){
        clientlen = sizeof(struct sockaddr_storage);    // Set size of client addr struct
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);   // Accept new connection
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);   // Resolve and get client hostname and port
        printf("Connected to (%s, %s)\n", client_hostname, client_port);

        echo(connfd);   // Call echo function to handle client I/O
        Close(connfd);  // Close connection after handling
    }
}

/*Handle client I/O: read input and echo it back*/
void echo(int connfd)
{
    size_t n;           // Number of bytes read
    char buf[MAXLINE];  // Buffer to store client message
    rio_t rio;          // Robust I/O struct

    Rio_readinitb(&rio, connfd);    // Initialize robust I/O for connection

    /*Read line from client and echo back message*/
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0 ){
        printf("server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
}