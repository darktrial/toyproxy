#define _GNU_SOURCE 
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <netdb.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wait.h>
#include <pthread.h>

#define OK   1
#define FAIL 0
#define BUFFER_MAX 1024

int sockfds[65535]={0};

int create_server_socket(int port) 
{
	int server_sock, optval = 1;
  struct sockaddr_in server_addr;

  if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		return FAIL;

	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) 
		return FAIL;

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) 
		return FAIL;

  if (listen(server_sock, 20) < 0) 
		return FAIL;

  return server_sock;
}

/* Create client connection */
int create_backend_connection(char *remote_host,int port) 
{
  struct sockaddr_in server_addr;
  struct hostent *server;
  int sock;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return FAIL;

  if ((server = gethostbyname(remote_host)) == NULL)
		return FAIL;

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  server_addr.sin_port = htons(port);

  if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
		return FAIL;

  return sock;
}


int main(int argc,char *argv[])
{
	
	char buf[BUFFER_MAX];
	int server_sock,backend_sock,frontend_sock,n,fdmax,i;
	struct sockaddr_in client_addr;
 	socklen_t addrlen = sizeof(client_addr);
	fd_set master_set,working_set;
	int pfd[2];
	char *backend_server_ip;
	int backend_server_port,local_proxy_port;
	
	if (argc<4)
		printf("usage: proxy local_proxy_port backend_server_ip backend_server_port\n");
	
	local_proxy_port=atoi(argv[1]);
	backend_server_ip=strdup(argv[2]);
	backend_server_port=atoi(argv[3]);
	
	if (pipe(pfd)!=0)
	{
		printf("initial pipe error\n");
		exit(0);
	}
	FD_ZERO(&master_set);
	server_sock=create_server_socket(local_proxy_port);
	if (server_sock==FAIL)
	{
		printf("socket failed\n");
		exit(0);
	}
	
	FD_SET(server_sock,&master_set);
	fdmax=server_sock;
	
	while (1)
	{
		working_set=master_set;
		if (select(fdmax+1,&working_set,NULL,NULL,NULL)==-1)
		{
			printf("error select\n");
			exit(0);
		}
		for(i=0; i<=fdmax;i++)
		{
    	if(FD_ISSET(i,&working_set))
    	{
    		if (i==server_sock)
    		{
    			frontend_sock = accept(server_sock,(struct sockaddr*)&client_addr,&addrlen);	
					backend_sock=create_backend_connection(backend_server_ip,backend_server_port);
					sockfds[frontend_sock]=backend_sock;
					sockfds[backend_sock]=frontend_sock;
					FD_SET(frontend_sock, &master_set);
					FD_SET(backend_sock, &master_set);
					if (frontend_sock>fdmax) fdmax=frontend_sock;
					if (backend_sock>fdmax) fdmax=backend_sock;
    		}
    		else
    		{
    			n=splice(i, NULL, pfd[1], NULL, 65535, SPLICE_F_MORE| SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    			if (n>0)
    				splice(pfd[0], NULL, sockfds[i], NULL, n, SPLICE_F_MORE| SPLICE_F_MOVE);
    			else
    			{
    				shutdown(i, SHUT_RDWR); 
  					close(i);
  					shutdown(sockfds[i], SHUT_RDWR); 
  					close(sockfds[i]);
  					FD_CLR(i, &master_set);
  					FD_CLR(sockfds[i], &master_set);
  					sockfds[i]=0;					
    			}
    			
    		}
    	}
		
		}
	}
}