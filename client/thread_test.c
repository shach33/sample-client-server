/****************** CLIENT CODE ****************/

#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<stdlib.h>
#include<unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <arpa/inet.h>
#define SO_TESTMARK 49
#define REQ_RATE 10
int clientSocket;
FILE *f;
typedef struct node{
	int req;
	struct node *next;
}node;

struct node *head = NULL;
struct node *tail = NULL;

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




pthread_t tid[2];



void* sendReq(void *arg)
{
	int i = 0, rps =0, cumlenSent = 0,reqId, ret, n = 0;
  	char addr[100], cl_addr[100], buffer1[1024];
  	int len_addr = 100;
	struct node *tmp, *tmp2;
	
 	while(1){
		rps = 0;
		//printf("\nn: %d\t",n);  	

		while(rps < REQ_RATE){
			//printf("rps: %d__",rps);  	
  			snprintf(buffer1,sizeof(buffer1), "Cl1: %d Hello Server",n);
			ret = 0;   
  			reqId = cumlenSent;
  			ret = send(clientSocket,buffer1,strlen(buffer1),0);
			if(ret){
				//printf("Ret: %d  ",ret);
				if(head==NULL){
					tmp =(struct node*)malloc(sizeof(struct node));
					tmp->req = reqId;
					tmp->next = NULL;
					head = tmp;
					tail = tmp;
					//printf("\tSent in head NULL %d\n", reqId);			
				}		
				else{
					tmp =(struct node*)malloc(sizeof(struct node));
					tmp->req = reqId;
					tmp->next = NULL;
					tail->next = tmp;
					tail = tmp;
					//printf("Sent in else %d\n", reqId);			

				}
				
  				cumlenSent+= ret;	
				peer_addr(clientSocket, addr, len_addr);
				my_addr(clientSocket, cl_addr, len_addr);
  				//printf("Sent after else");
				//printf("\nServer details: %s CLient %s reqID: %d",addr,cl_addr, reqId);
				//tmp2 = head;
				//while(tmp2!=NULL) { printf("%d\t",tmp2->req); tmp2=tmp2->next;}
				usleep(1000000/REQ_RATE);
				rps ++;
				n++;
			}
		}
		
	}
	return NULL;
}


void* rcvRep(void *arg)
{
	int i = 0, rps =0, cumlenRcvd = 0,repId, ret, n = 0, reqId;
  	char addr[100], cl_addr[100], buffer[1024];
  	int len_addr = 100;
	struct node *tmp;
	head = NULL;
	tail = NULL;
	while(1){
		peer_addr(clientSocket, addr, len_addr);

		my_addr(clientSocket, cl_addr, len_addr);
		ret= recv(clientSocket, buffer, 1024, 0);
	  	repId = cumlenRcvd;
		if(ret){
			if(head==NULL){
				printf("Error: No requests sent");			
			}		
			else{
				reqId = head ->req;
				tmp = head;
				head = head->next;
				free(tmp);
			}
			

	  		cumlenRcvd += ret;
		
			printf("\nReply %s Client: %s Server: %s RepID: %d ReqId %d\n", buffer, cl_addr,addr, repId,reqId);
		}
	}
	return NULL;
}

int main(void)
{
    	int i = 0;
    	int err;
		
	char buffer[1024];
	struct sockaddr_in serverAddr;
  	socklen_t addr_size;
  	int ret,opv,opl,setv,setl,numL=0;
  	int repId,cumlenRcvd=0;

  	f = fopen("mapCl.txt", "a"); 
  	int reqL[10], repL[10];
  	int n = 0;
  	
	if(f==NULL){
   		printf("cant open file");
  	}
 
	setv=5;
 
	clientSocket = socket(PF_INET, SOCK_STREAM, 0);
	//  ret = setsockopt(clientSocket,SOL_SOCKET, SO_MARK, &setv, sizeof setv);	
	//  printf("\n Returnded from mark : %d errno = %d",ret,errno);
	//ret = setsockopt(clientSocket,SOL_SOCKET,49,&setv,sizeof setv);
  	printf("\n returned from setopt: %d errno = %d sock#: %d",ret,errno,clientSocket); 
  	//fflush(stdin);
  	//ret = setsockopt(clientSocket,IPPROTO_TCP,TCP_NODELAY,&setv,sizeof setv);
  	printf("\n returned from tcp_nodelay setopt: %d",ret);
  	//getsockopt(clientSocket,SOL_SOCKET,SO_TESTMARK,&opv,&opl);
  	//printf("\nget sock val %d",opv);
  	getsockopt(clientSocket,IPPROTO_TCP,TCP_NODELAY,&opv,&opl);
  	printf("\n no delay : %d",opv);

  	/*---- Configure settings of the server address struct ----*/
  	/* Address family = Internet */
	serverAddr.sin_family = AF_INET;
  	/* Set port number, using htons function to use proper byte order */
  	serverAddr.sin_port = htons(7891);
  	/* Set IP address to localhost */
  	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  	/* Set all bits of the padding field to 0 */
  	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  
	
  	/*---- Connect the socket to the server using the address struct ----*/
  	addr_size = sizeof serverAddr;

  	ret = connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size);
  	printf("\n return from connect %d, e %d\n",ret,errno); 
   
	
        	err = pthread_create(&(tid[0]), NULL, &sendReq, NULL);
        	if (err != 0)
        	    printf("\ncan't create thread 0(Send):[%s]", strerror(err));
        	else
        	    printf("\n Thread 0 (Send) created successfully\n");

        	err = pthread_create(&(tid[1]), NULL, &rcvRep, NULL);
        	if (err != 0)
        	    printf("\ncan't create thread 1(Rcv):[%s]", strerror(err));
        	else
        	    printf("\n Thread 1(Rcv) created successfully\n");
	
        	//i++;
    	

    sleep(20);
    return 0;
}
