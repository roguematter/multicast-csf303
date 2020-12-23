#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define shm_key_path "temp0"
#define shm_key_id 1
#define max_shm_len 1024
#define l1_shm_key_path "temp1"
#define l1_shm_key_id 1
#define l1_shm_key_path_dec "temp2"
#define l1_shm_key_id_dec 1
#define l1_max_shm_len 3072
#define max_hname_len 64
#define max_buddy 10
#define max_buddy_digit 2
#define max_thread_num 128
pthread_mutex_t mutex;
typedef struct _buf_params buf_params;
typedef struct _buf_params * ptr_buf_params;
struct _buf_params{
	int num_bufs;
	int buf_size;
};
typedef struct _fwd_arg fwd_arg;
typedef struct _fwd_arg * ptr_fwd_arg;
struct _fwd_arg{
	char *ptrBuf;
	int buf_size;
	char transAddr[max_hname_len];
};

int status;
int async;
int tl_size;
int hl_size;
char tl[max_buddy-1][max_hname_len];
char hl[max_buddy][max_hname_len];
char self[max_hname_len];
void *shm_address;

void reciever(void *ptrBufParams);
void transmitter();
void forwarder(void *fwdArg);

void main(int argc, char *argv[]){
	key_t shm_key=ftok(shm_key_path,shm_key_id);
	int shm_id=shmget(shm_key,max_shm_len,0666|IPC_CREAT);
	void *shm_addr=shmat(shm_id,NULL,0);
	bzero((char *)shm_addr,max_shm_len);
	struct shmid_ds shm_struct;
	
	int pos=0;
	int i;

	while(strcmp((char *)shm_addr,"1")!=0);
	sscanf((char *)shm_addr+2,"%d",&tl_size);
	pos=2+max_buddy_digit+1;
	for(i=0;i<tl_size;i++){
		sscanf((char *)shm_addr+pos,"%s",tl[i]);
		pos+=strlen(tl[i])+1;
		//printf("%s\n",tl[i]);
	}
	strcpy((char *)shm_addr,"0");
	while(strcmp((char *)shm_addr,"1")!=0);
	sscanf((char *)shm_addr+2,"%d",&hl_size);
	pos=2+max_buddy_digit+1;
	for(i=0;i<hl_size;i++){
		sscanf((char *)shm_addr+pos,"%s",hl[i]);
		pos+=strlen(hl[i])+1;
		//printf("%s\n",hl[i]);
	}
	shmdt(shm_addr);
	shmctl(shm_id,IPC_RMID,&shm_struct);
	
	async=atoi(argv[1]);
	strcpy(self,argv[2]);
	
	buf_params bufParams;
	
	/*
	read these parameters from shm interface with buf_manager
	*/
	
	bufParams.num_bufs=tl_size*4;
	bufParams.buf_size=l1_max_shm_len+max_hname_len;
	
	pthread_t tid[2];
	pthread_create(&tid[0],(const pthread_attr_t *)NULL,(void *)reciever,(void *)&bufParams);
	pthread_create(&tid[1],(const pthread_attr_t *)NULL,(void *)transmitter,(void *)NULL);
	pthread_join(tid[1],NULL);
}

void reciever(void *ptrBufParams){
	key_t shm_key=ftok(l1_shm_key_path_dec,l1_shm_key_id_dec);
	int shm_id=shmget(shm_key,(l1_max_shm_len+2)*hl_size,0666);
	shm_address=shmat(shm_id,NULL,0);
	bzero((char *)shm_address,(l1_max_shm_len+2)*hl_size);
	

	int sockfd;
	struct sockaddr_in servaddr,cliaddr;
	socklen_t len;
	pthread_t tid[max_thread_num];
	int i,j;
	int num_bufs=((ptr_buf_params)ptrBufParams)->num_bufs;
	int buf_size=((ptr_buf_params)ptrBufParams)->buf_size;
	
	char **rdBuf=(char **)malloc(num_bufs*sizeof(char *));
	for(i=0;i<num_bufs;i++){
		rdBuf[i]=(char *)malloc(buf_size*sizeof(char));
		bzero(rdBuf[i],buf_size);
	}
	ptr_fwd_arg fwdArg=(ptr_fwd_arg)malloc(num_bufs*sizeof(fwd_arg));

	sockfd=socket(AF_INET,SOCK_DGRAM,0);

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	servaddr.sin_port=htons(50008);
	bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
	
	i=j=0;

	for(;;)
	{
		i++;j++;
		if(i>=num_bufs) i=0;
		if(j>=max_thread_num) j=0;
		len = sizeof(cliaddr);
		//printf("blah\n");
		recvfrom(sockfd,rdBuf[i],buf_size,0,(struct sockaddr *)&cliaddr,&len);
		//printf("blah aftr\n");
		fwdArg[i].ptrBuf=rdBuf[i];
		fwdArg[i].buf_size=buf_size;
		strcpy(fwdArg[i].transAddr,inet_ntoa(cliaddr.sin_addr));
		//printf("%s : %s : %s - %s\n",fwdArg[i].transAddr,fwdArg[i].ptrBuf,fwdArg[i].ptrBuf+max_hname_len,fwdArg[i].ptrBuf+max_hname_len+6);
		pthread_create(&tid[j],(const pthread_attr_t *)NULL,(void *)forwarder,(void *)&fwdArg[i]);
		//usleep(2000);
		//forwarder((void *)&fwdArg[i]);
		//printf("doing thread %d\n",j);
	}
}

void transmitter(){
	int sockfd,i;
	struct sockaddr_in servaddr;
	key_t shm_key=ftok(l1_shm_key_path,l1_shm_key_id);
	int shm_id=shmget(shm_key,l1_max_shm_len+max_hname_len+2,0666);
	void *shm_addr=shmat(shm_id,NULL,0);
	//bzero((char *)shm_addr,max_shm_len);
	
	sockfd=socket(AF_INET,SOCK_DGRAM,0);

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port=htons(50008);
	
	while(1){
		if(!async) if(strcmp((char *)shm_addr,"1\0")!=0) continue;
		strcpy((char *)shm_addr+2,self);
		for(i=0;i<tl_size;i++){
			servaddr.sin_addr.s_addr=inet_addr(tl[i]);
			sendto(sockfd,(char *)shm_addr+2,l1_max_shm_len+max_hname_len,0,(struct sockaddr *)&servaddr,sizeof(servaddr));
			//printf("packet sent %s\n",(char *)shm_addr+max_hname_len+9);
		}
		if(!async) bzero((char *)shm_addr,l1_max_shm_len+max_hname_len+2);
		if(!async) strcpy((char *)shm_addr,"0\0");
	}
}

void forwarder(void *fwdArg){
	//printf("in thread\n");
	char transHname[max_hname_len];
	int buf_size=((ptr_fwd_arg)fwdArg)->buf_size;
	char *dataBuf=(char *)malloc(sizeof(char)*buf_size);
	strcpy(transHname,((ptr_fwd_arg)fwdArg)->transAddr);
	bzero(dataBuf,buf_size);
	bcopy(((ptr_fwd_arg)fwdArg)->ptrBuf,dataBuf,buf_size);
	bzero(((ptr_fwd_arg)fwdArg)->ptrBuf,buf_size);
	
	int sockfd,i;
	struct sockaddr_in servaddr;
	
	sockfd=socket(AF_INET,SOCK_DGRAM,0);

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port=htons(50008);

	for(i=0;i<tl_size;i++){
		if(strcmp(tl[i],transHname)!=0){
			servaddr.sin_addr.s_addr=inet_addr(tl[i]);
			sendto(sockfd,dataBuf,buf_size,0,(struct sockaddr *)&servaddr,sizeof(servaddr));
		}
	}
	/*
		Implement shm with decoder here
	*/
	for(i=0;i<hl_size;i++){
		if(strcmp(dataBuf,hl[i])==0){
			//printf("%s\n\n",dataBuf);
			if(strcmp((char *)shm_address+i*(l1_max_shm_len+2),"2\0")==0) {
				free(dataBuf);
				break;
			}
			else if(strcmp((char *)shm_address+i*(l1_max_shm_len+2),"1\0")==0){
				free(dataBuf);
				break;
			}
			else{
				strcpy((char *)shm_address+i*(l1_max_shm_len+2),"1\0");
				bcopy(dataBuf+max_hname_len,(char *)shm_address+i*(l1_max_shm_len+2)+2,l1_max_shm_len);
				strcpy((char *)shm_address+i*(l1_max_shm_len+2),"2\0");
				free(dataBuf);
				break;
			}
		}
	}
	pthread_detach(pthread_self());
	
}
