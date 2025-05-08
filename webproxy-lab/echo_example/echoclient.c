/*
* An echo server reads data sent by a client and sends the exact same data back.
* It is commonly used to test and debug network communication, socket handling, and robust I/O routines.
*/

#include "csapp.h"

int main(int argc, char ** argv)
{
    int clientfd;   // File descriptor for client socket
    char *host, *port, buf[MAXLINE];    // Hostname, port number, I/O buffer
    rio_t rio;      // Robust I/O buffer struct

    /*Check command-line args*/
    if (argc != 3){
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1];     // Hostname of server
    port = argv[2];     // Port# of server

    clientfd = Open_clientfd(host, port);   // Open connection to server
    Rio_readinitb(&rio, clientfd);          // Initialize robust I/O buffer

    /*Loop : read input from user, send to server, print server's reply*/
    while(Fgets(buf, MAXLINE, stdin) != NULL ){
        Rio_writen(clientfd, buf, strlen(buf));     // Send input line to server
        Rio_readlineb(&rio, buf, MAXLINE);      // Read server's echoed
        Fputs(buf, stdout);     // Print response to stdout
    }

    Close(clientfd);
    exit(0);
}