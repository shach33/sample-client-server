/****************** CLIENT CODE ****************/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#define SO_TESTMARK 49

char* peer_addr(int s, char *buf, int buf_size){
	int z;
	struct sockaddr_in adr_inet;
	int len_inet;
	len_inet = sizeof(adr_inet);

	z = getpeername(s,(struct sockaddr*)&adr_inet,&len_inet);
	if(z==-1){
		return NULL;
	}
	z = snprintf(buf, buf_size,"%s:%u", inet_ntoa(adr_inet.sin_addr), (unsigned)ntohs(adr_inet.sin_port));
	if(z==-1){
		return NULL;
	}
	return buf;
}

char* my_addr(int s, char *buf, int buf_size){
	int z;
	struct sockaddr_in adr_inet;
	int len_inet;
	len_inet = sizeof(adr_inet);

	z = getsockname(s,(struct sockaddr*)&adr_inet,&len_inet);
	if(z==-1){
		return NULL;
	}
	z = snprintf(buf, buf_size,"%s:%u", inet_ntoa(adr_inet.sin_addr), (unsigned)ntohs(adr_inet.sin_port));
	if(z==-1){
		return NULL;
	}
	return buf;
}

int main(){
  int clientSocket;
  char buffer[1024],buffer1[1024];
  struct sockaddr_in serverAddr;
  socklen_t addr_size;
  int ret,opv,opl,setv,setl,numL=0;
  int reqId, repId,cumlenSent=0,cumlenRcvd=0;
  char addr[100], cl_addr[100];
  int len_addr = 100;
  FILE *f = fopen("mapCl.txt", "a"); 
  int reqL[10], repL[10];
 setv=5;
  int n = 0;
  if(f==NULL){
   printf("cant open file");
  }
 
  
  /*---- Create the socket. The three arguments are: ----*/
  /* 1) Internet domain 2) Stream socket 3) Default protocol (TCP in this case) */
//  while(!numL){
  clientSocket = socket(PF_INET, SOCK_STREAM, 0);
//  ret = setsockopt(clientSocket,SOL_SOCKET, SO_MARK, &setv, sizeof setv);
//  printf("\n Returnded from mark : %d errno = %d",ret,errno);
  ret = setsockopt(clientSocket,SOL_SOCKET,49,&setv,sizeof setv);
  printf("\n returned from setopt: %d errno = %d sock#: %d",ret,errno,clientSocket); 
  //fflush(stdin);
  ret = setsockopt(clientSocket,IPPROTO_TCP,TCP_NODELAY,&setv,sizeof setv);
  printf("\n returned from tcp_nodelay setopt: %d",ret);
  /*---- Configure settings of the server address struct ----*/
  /* Address family = Internet */
  getsockopt(clientSocket,SOL_SOCKET,SO_TESTMARK,&opv,&opl);
  printf("\nget sock val %d",opv);
  //getsockopt(clientSocket,IPPROTO_TCP,TCP_NODELAY,&opv,&opl);
  //printf("\n no delay : %d",opv);
  serverAddr.sin_family = AF_INET;
  /* Set port number, using htons function to use proper byte order */
  serverAddr.sin_port = htons(7891);
  /* Set IP address to localhost */
  serverAddr.sin_addr.s_addr = inet_addr("10.22.17.172");
  /* Set all bits of the padding field to 0 */
  memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  

  /*---- Connect the socket to the server using the address struct ----*/
  addr_size = sizeof serverAddr;
 //while(numL==0){
//  printf("into while ");
  ret = connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size);
  printf("\n return from connect %d, e %d\n",ret,errno); 
		/* if(ret == -1)
 		 printf("\nreturn errno by connect %d ",errno);  */
   
  while(n<10){
  
  strcpy(buffer1,"Cl1: Hello Server\n");
   
  reqId = cumlenSent;
  ret = send(clientSocket,buffer1,strlen(buffer1),0);
  cumlenSent+= ret;
   //getsockopt(clientSocket,SOL_SOCKET, SO_CUMLENRCVD,&rpl Id,&opl);
  peer_addr(clientSocket, addr, len_addr);

  my_addr(clientSocket, cl_addr, len_addr);
  printf("\nServer details: %s CLient %s\n",addr,cl_addr);
  printf("\n Sent data size %d",ret);
   
  ret= recv(clientSocket, buffer, 1024, 0);
  repId = cumlenRcvd;
  cumlenRcvd += ret;
//  shutdown(clientSocket,SHUT_WR);

  fprintf(f,"Client: %s Server: %s ReqID: %d RepID: %d\n", cl_addr,addr, reqId, repId);

  /*strcpy(buffer1,"Cl2: Hello again!\n");
  ret = send(clientSocket,buffer1,20,0);
  printf("\n 2 sent data size %d",ret);
  cumlenSent +=ret;
*/  /*---- Read the message from the server into the buffer ----*/
  
/*  ret= recv(clientSocket, buffer, 1024, 0);
  repId = cumlenRcvd;
  cumlenRcvd += ret;
  fprintf(f,"Client: %s Server: %s ReqID: %d RepID: %d\n", cl_addr,addr, reqId, repId);
  
*/
//  fclose(f);
  /*---- Print the received message ----*/
 // printf("Data received: %s #: %d",buffer,ret);   
//  printf("Enter 0 to continue");
//  scanf("%d",&numL);
  n++;
  }
  fclose(f);
  close(clientSocket);
// }
  return 0;
}
