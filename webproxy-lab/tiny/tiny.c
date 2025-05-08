/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
		"Firefox/10.0.3\r\n";
/*HTTP header format*/
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestline_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";
/*HTTP header keys to be overridden by proxy*/
static const char *connection_key = "Connection";
static const char *user_agent_key = "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

/*Function declaration*/
void doit(int fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);

int main(int argc, char **argv)
{
	/*Variables for handling client conn and store client add info*/
	int listenfd, connfd;	// listening socket and connected socket descriptors
	char hostname[MAXLINE], port[MAXLINE];	// store in buffer
	socklen_t clientlen;	// size of client addr struct
	struct sockaddr_storage clientaddr;		// generic client addr structure (IPv4, IPv6)

	/*Check command-line args*/
	if (argc != 2){
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}

	listenfd = Open_listenfd(argv[1]);	// Create listening socket on given port
	/*Infinite loop to accept & handle client connections*/
	while(1){
		clientlen = sizeof(clientaddr);		// set client addr length
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 	// accept new connection from client
		Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);	// get client hostname and port for logging 
		printf("Accepted connection from (%s, %s)\n", hostname, port);
		doit(connfd);	// handle HTTP request from client
		Close(connfd);	// close conn after request complete
	}
	
	return 0; 	// program ends (technically never reached)
}

/*Handle a single HTTP request: parse, validate, and serve the response*/
void doit(int fd)
{
	int is_static;		// Flag : static (1), dynamic (0)
	struct stat sbuf;	// Store file metadata
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];		// Buffers for request line
	char filename[MAXLINE], cgiargs[MAXLINE];	// Parsed file path and CGI arg
	rio_t rio; 		// Robust I/O buffer structure

	Rio_readinitb(&rio, fd);	// Init RIO buffer with client file descriptor
	Rio_readlineb(&rio, buf, MAXLINE);		// Read request line from client
	printf("Request headers:\n");
	printf("%s", buf);		// Print request line to server terminal
	sscanf(buf, "%s %s %s", method, uri, version);		// Parse request line to perform logic

	if (strcasecmp(method, "GET")){		// Only support GET method
		clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");	// 501 error if not GET
		return;
	}
	read_requesthdrs(&rio);

	/*Parse URI from GET request*/
	is_static = parse_uri(uri, filename, cgiargs);

	/*Check if requested file exists and retrieve metadata*/
	// stat func : if exists ret 0, else -1
	if (stat(filename, &sbuf) < 0){
		clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
		return;
	}

	if (is_static){  /*Serve static content*/
		// check if file is regular file & have read access (읽기 권한)
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
			clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read file");
			return;
		}
		// if requirements met : Serve the file with appropriate HTTP headers over to client socket
		serve_static(fd, filename, sbuf.st_size);
	}else{		/*Serve dynamic content*/
		// Check if file is regular file & have access to execute file
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
			clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run CGI program");
			return;
		}
		// if req met : Run the CGI program; it generates the HTTP response and sends it to the client
		serve_dynamic(fd, filename, cgiargs);
	}
}

/*<Util> Build and send HTTP error message (header+body) to client when something goes wrong*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
	char buf[MAXLINE], body[MAXLINE];	// Buffers for HTTP response

	/*Build HTTP response body*/
	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body, "%s<body bgcolor=\"ffffff\"\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

	/*Print HTTP response*/
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n", (int)strlen(body));
	Rio_writen(fd, buf, strlen(buf));
	Rio_writen(fd, body, strlen(body));
}

/*<Util> Read and discard the remaining HTTP request headers*/
void read_requesthdrs(rio_t *rp)
{
	char buf[MAXLINE];	// Buffer to store each header line

	Rio_readlineb(rp, buf, MAXLINE);	// Read 1st header line
	/*Loop until empty line*/
	while (strcmp(buf, "\r\n")){
		Rio_readlineb(rp, buf, MAXLINE);	// Read next header line
		printf("%s", buf);		// Print header line to server terminal
	}
	return;		// All headers read and ignored
}

/*<Util> Parse URI into local filename and add CGI args if dynamic*/
int parse_uri(char *uri, char *filename, char *cgiargs)
{
	char *ptr;

	/*If static : uri not contain "cgi-bin"*/
	if(!strstr(uri, "cgi-bin")){
		strcpy(cgiargs, "");		// Clear CGI arg
		strcpy(filename, ".");		// Start filename with curr directory
		strcat(filename, uri);		// Append URI to filename to form relative path
		// If uri ends with '/', append default file
		if(uri[strlen(uri)-1] == '/')
			strcat(filename, "home.html");
		return 1;		// Indicate static content
	}
	/*If dynamic : uri contain "cgi-bin"*/
	else{
		ptr = index(uri, '?');		// Look for CGI arg delimiter '?'
		if(ptr){
			strcpy(cgiargs, ptr+1);	// Copy argument after '?'
			*ptr = '\0';			// Terminate URI at '?'
		}
		else{
			strcpy(cgiargs, "");	// No argument present
		}
		strcpy(filename, ".");		// Start filename with current directory
		strcat(filename, uri);		// Append URI to filename
		return 0;					// Indicate dynamic content
	}
}

/*Send a requested static file to the client via HTTP, using memory-mapped I/O*/
void serve_static(int fd, char *filename, int filesize)
{
	int srcfd;
	char *srcp, filetype[MAXLINE], buf[MAXBUF];

	/*Send response headers to client*/
	get_filetype(filename, filetype);	// Determine file type based on filename extension
	sprintf(buf, "HTTP/1.0 200 OK\r\n");	// Build status line
	sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
	sprintf(buf, "%sConnection: close\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
	Rio_writen(fd, buf, strlen(buf));	// Send headers to client

	printf("Response headers:\n");
	printf("%s", buf);

	/*Send response body to client*/
	srcfd = Open(filename, O_RDONLY, 0);
	srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);		// Map file to memory (Memory-Mapped I/O)
	Close(srcfd);	// File descriptor no longer needed after mmap
	Rio_writen(fd, srcp, filesize);		// Send file contents to client
	Munmap(srcp, filesize); 	// Unmap file from memory
}

/* <Util> Derive MIME type string from file extension */
void get_filetype(char *filename, char *filetype)
{
	if (strstr(filename, ".html"))
		strcpy(filetype, "text/html");
	else if (strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
	else if (strstr(filename, ".png"))
		strcpy(filetype, "image/png");
	else if (strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mpg"))
        strcpy(filetype, "video/mpeg");
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4");
	else
		strcpy(filetype, "text/plain");
}

/*Fork a child process to run a CGI program, redirect its stdout to the client, and wait for completion*/
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
	// Buffer for headers and emtpy argv list for execve
	char buf[MAXLINE], *emptylist[] = { NULL };

	/*Return first part of HTTP response*/
	sprintf(buf, "HTTP/1.0 200 0K\r\n");	// Write status line
	Rio_writen(fd, buf, strlen(buf));		// Send to client

	sprintf(buf, "Server: Tiny Web Server\r\n");	// Write new header line (Server info)
	Rio_writen(fd, buf, strlen(buf));		// Send to client

	/*Child process : real server would set all CGI vars here*/
	// Fork == 0 means func only executed in child process
	// fork은 사람을 복사하고 execve는 사람의 뇌까지 완전 갈아 끼운다.
	if (Fork() == 0){
		setenv("QUERY_STRING", cgiargs, 1);		// Set CGI arguments as environment variable
		Dup2(fd, STDOUT_FILENO);	// Redirect stdout to client socket
		Execve(filename, emptylist, environ);	// Child process created by fork becomes CGI program and is executed; isolate execution from the main server
	}

	Wait(NULL);		// Parent waits for child to finish. Prevent child being zombie process.
}