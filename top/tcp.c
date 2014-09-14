//Copied from: http://www.cs.ucsb.edu/~almeroth/classes/W01.176B/hw2/examples/tcp-client.c

/* Sample TCP client */

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

int main(int argc, char**argv)
{
   int sockfd,n;
   struct sockaddr_in servaddr,cliaddr;
   char sendline[1000];
   char recvline[1000];

   if (argc != 3)
   {
      printf("usage: client <IP address> <brightness>\n");
      exit(1);
   }

   sockfd=socket(AF_INET,SOCK_STREAM,0);

   bzero(&servaddr,sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr=inet_addr(argv[1]);
   servaddr.sin_port=htons(55555);

   connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));


	sendline[0] = 255-atoi( argv[2] );
	sendto(sockfd,sendline,1,0,(struct sockaddr *)&servaddr,sizeof(servaddr));

}

