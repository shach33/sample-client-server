/****************** SERVER CODE ****************/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#define SO_CUMLENSENT 50
#define SO_CUMLENRCVD 51
#include <arpa/inet.h>

char* peer_addr(int s, char *buf, int buf_size){
	int z;
	struct sockaddr_in adr_inet;
	int len_inet = sizeof(adr_inet);

	z = getpeername(s, (struct sockaddr*)&adr_inet, &len_inet);
	if(z==-1)
		return NULL;
	z = snprintf(buf, buf_size, "%s:%u", inet_ntoa(adr_inet.sin_addr) ,(unsigned)ntohs(adr_inet.sin_port));	
	if(z==-1)
		return NULL;
	else
		return buf;
}

char* my_addr(int s, char *buf, int buf_size){
	int z;
	struct sockaddr_in adr_inet;
	int len_inet = sizeof(adr_inet);

	z = getsockname(s, (struct sockaddr*)&adr_inet, &len_inet);
	if(z==-1)
		return NULL;
	z = snprintf(buf, buf_size, "%s:%u", inet_ntoa(adr_inet.sin_addr) ,(unsigned)ntohs(adr_inet.sin_port));	
	if(z==-1)
		return NULL;
	else
		return buf;
}
int main(){
  int welcomeSocket, newSocket,wel2;
  char buffer[1024],buffer1[1024],buffer2[1024];
  struct sockaddr_in serverAddr;
  struct sockaddr_storage serverStorage;
  socklen_t addr_size;
  int ret,setv=5, n = 0;
  int reqId, repId,opl;
  int cumlenSent=0, cumlenRcvd=0;
  char sr_addr[100], cl_addr[100];
  FILE *f = fopen("mapSr.txt","a");
  if(f == NULL){
 	printf("Cant open file");
  }
  /*---- Create the socket. The three arguments are: ----*/
  /* 1) Internet domain 2) Stream socket 3) Default protocol (TCP in this case) */
  welcomeSocket = socket(PF_INET, SOCK_STREAM, 0);
  //ret = setsockopt(welcomeSocket,SOL_SOCKET,SO_TESTMARK,&setv, sizeof setv);
  //printf("\n return from socket : %d sock# %d",ret,welcomeSocket);
  setsockopt(welcomeSocket, SOL_SOCKET, SO_REUSEADDR, &setv, sizeof setv);
  /*---- Configure settings of the server address struct ----*/
  /* Address family = Internet */
  serverAddr.sin_family = AF_INET;
  /* Set port number, using htons function to use proper byte order */
  serverAddr.sin_port = htons(7891);
  /* Set IP address to localhost */
  serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  /* Set all bits of the padding field to 0 */
  memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  

  /*---- Bind the address struct to the socket ----*/
  bind(welcomeSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));

  /*---- Listen on the socket, with 5 max connection requests queued ----*/
//  while(1){
	int clientfd;
//	struct sockaddr_in c_addr;
	
  //bind(welcomeSocket,(struct sockaddr *) &serverAddr,sizeof(serverAddr));
  if(listen(welcomeSocket,5)==0)
    printf("Listening\n");
  else
    printf("Error\n");

  /*---- Accept call creates a new socket for the incoming connection ----*/
  addr_size = sizeof serverStorage;
  clientfd = accept(welcomeSocket, (struct sockaddr *) &serverStorage, &addr_size);
  while(1){
  	reqId = cumlenRcvd;
  	ret = recv(clientfd, buffer1, 1024, 0);
  	cumlenRcvd += ret;

  	repId= cumlenSent;
  	snprintf(buffer,sizeof(buffer),"Hello Client %d", n);
  	ret = send(clientfd,buffer,13,0);
  	//printf("\n After send \n");
  	cumlenSent+=ret;
  	//getsockopt(clientfd,SOL_SOCKET, SO_CUMLENSENT, &sentL,&opl);

  	//recv(clientfd, buffer2, 1024, 0);
  	//  printf("S2: rcvd %s\n",buffer2);
  
  	/*---- Send message to the socket of the incoming connection ----*/
  	peer_addr(clientfd,cl_addr, 100);
  
 
  	my_addr(clientfd,sr_addr, 100);

  	//printf("\nCLient details: %s",cl_addr);
  	printf("Rqst ID: %d,Reply ID: %d\n",reqId,repId);
  	//fprintf(f, "Client: %s Server: %s ReqID: %d RepID: %d\n",cl_addr, sr_addr, reqId, repId);
  	n++;
 }
  close(clientfd);
  fclose(f);
  return 0;

}
