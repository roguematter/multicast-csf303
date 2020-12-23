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

#define max_buddy 10			//maximum number of participants
#define max_buddy_digit 2		//
#define max_conn_q 5			//
#define max_se_buf_len 512		//maximum serialized data length
#define max_ack_buf_len 8		//
#define max_tmp_buf_len 16		//
#define max_cmd_len 4			//
#define max_ack_len 4			//
#define max_hname_len 64			//
#define max_int_digits 19		//
#define shm_key_path "temp0"
#define shm_key_id 1
#define max_shm_len 1024

#define port_def 50009			//default port to listen on

typedef struct _hostlist hostlist;
struct _hostlist{
	short unsigned int self;
	short unsigned int num_hosts;
	char hostname[max_buddy][max_hname_len];
	unsigned long int rtt[max_buddy];
};

hostlist hl;					//list of all participants

typedef struct _dstNode dstNode;
struct _dstNode{
	short unsigned int index;
	long int dst;
	char s_hop_reach;
};

dstNode dstVector[max_buddy-1];

long int eps_dst=0;
long int edge_cost[max_buddy][max_buddy];

char cmd[][max_cmd_len+1]={"init","rttc","updt","tble"};
char ack[][max_ack_len+1]={"inak","rtak","upak","tbak"};

char name_self[max_hname_len];

char tl[max_buddy][max_hname_len];
unsigned int tl_size=0;

static char *itoa(long int i);
static void rttgen(void *hname);
static void responder(void *conn_ptr);
static void sort(dstNode dstVector[],int st,int en);

void main(int argc,char *argv[]){
	pthread_t tid;
	struct sockaddr_in server_address,client_address;
	
	int address_len=sizeof(client_address);
	int sock,conn;
	int i,j;
	
	strcpy(name_self,argv[1]);

	/* :: initializing edge costs :: */
	for(i=0;i<max_buddy;i++){
		for(j=0;j<max_buddy;j++){
			edge_cost[i][j]=-1;
		}
	}
	for(i=0;i<max_buddy;i++){
		edge_cost[i][i]=0;
	}
	
	bzero((char *)&server_address,sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons(port_def);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	
	bind(sock,(struct sockaddr *)&server_address,sizeof(server_address));

	listen(sock,max_conn_q);

	while(1){
		conn=accept(sock,(struct sockaddr *)&client_address,(socklen_t *)&address_len);
		pthread_create(&tid,(const pthread_attr_t *)NULL,(void *)responder,(void *)&conn);
		bzero((char *)&client_address,sizeof(client_address));
		address_len=sizeof(client_address);
	}
	close(sock);
	exit(0);
}

/* :: protocol functions :: */

static void responder(void *conn_ptr){
	key_t shm_key=ftok(shm_key_path,shm_key_id);
	int shm_id=shmget(shm_key,max_shm_len,0666);
	void *shm_addr=shmat(shm_id,NULL,0);
	//bzero((char *)shm_addr,max_shm_len);
	
	pthread_t tid[max_buddy];
	
	int conn=(int)*(int *)conn_ptr;
	int i,j,pos,chng;
	long int alt_dst;
				
	char se_buf[max_se_buf_len];
	char tmp_buf[max_tmp_buf_len];
	
	bzero(se_buf,max_se_buf_len);
	bzero(tmp_buf,max_tmp_buf_len);
	
	read(conn,se_buf,max_se_buf_len);
	if(strcmp(se_buf,cmd[0])==0){
		sscanf(se_buf+max_cmd_len+1,"%s",tmp_buf);
		hl.num_hosts=atoi(tmp_buf);
		pos=max_cmd_len+max_buddy_digit+2;
		for(i=0;i<hl.num_hosts;i++){
			sscanf(se_buf+pos,"%s",hl.hostname[i]);
			pos+=strlen(hl.hostname[i])+1;
		}
		for(i=0;i<hl.num_hosts;i++){
			if(strcmp(name_self,hl.hostname[i])==0){
				hl.self=i;
				break;
			}
		}
		bzero(tmp_buf,max_tmp_buf_len);
		write(conn,ack[0],max_ack_len+1);
		read(conn,tmp_buf,max_tmp_buf_len);
		if(strcmp(tmp_buf,cmd[1])==0){
			bzero(tmp_buf,max_tmp_buf_len);
			write(conn,ack[1],max_ack_len+1);
			read(conn,tmp_buf,max_tmp_buf_len);
			if(strcmp(tmp_buf,cmd[2])==0){
				bzero(tmp_buf,max_tmp_buf_len);
				bzero(se_buf,max_se_buf_len);
				strcat(se_buf,ack[2]);
				strcat(se_buf+max_ack_len+1,itoa(hl.self));
				pos=max_buddy_digit+max_ack_len+2;
				for(i=hl.self+1;i<hl.num_hosts;i++){
					pthread_create(&tid[i],(const pthread_attr_t *)NULL,(void *)rttgen,(void *)hl.hostname[i]);
				}
				for(i=hl.self+1;i<hl.num_hosts;i++){
					pthread_join(tid[i],NULL);
				}
				for(i=hl.self+1;i<hl.num_hosts;i++){
					strcpy(tmp_buf,itoa(hl.rtt[i]));
					strcat(se_buf+pos,tmp_buf);
					pos+=strlen(tmp_buf)+1;
					bzero(tmp_buf,max_tmp_buf_len);
				}
				write(conn,se_buf,max_se_buf_len);
				close(conn);
				return;
			}
			else{
				printf("Error 0x005 : Expecting 'UPDT'\n");
				close(conn);
				return;
			}
		}
		else{
			printf("Error 0x005 : Expecting 'RTTC'\n");
			close(conn);
			return;
		}
	}
	else if(strcmp(se_buf,cmd[1])==0){
		write(conn,ack[1],max_ack_len+1);
		close(conn);
		return;
	}
	else if(strcmp(se_buf,cmd[3])==0){
		write(conn,ack[3],max_ack_len+1);
		close(conn);
		pos=max_cmd_len+1;
		for(i=0;i<hl.num_hosts;i++){
			for(j=0;j<hl.num_hosts;j++){
				sscanf(se_buf+pos,"%s",tmp_buf);
				edge_cost[i][j]=(long int)atoi(tmp_buf);
				pos+=strlen(tmp_buf)+1;
				bzero(tmp_buf,max_tmp_buf_len);
			}
		}
		
		/* :: displaying edge costs :: *//*
		printf("\n\n:: Calculating RTTs ::\n");
		*/
		int mem_cnt=0;
		for(i=0;i<hl.num_hosts;i++){
			for(j=0;j<hl.num_hosts;j++){
				if(i>j){
					if(edge_cost[i][j]>=0){
						eps_dst+=edge_cost[i][j];
						mem_cnt++;
					}
				}
				//printf("%s to %s in %ld us\n",hl.hostname[i],hl.hostname[j],edge_cost[i][j]);
			}
		}
		eps_dst/=mem_cnt;
		/**/
		/* :: initializing distance vector :: */
		for(j=i=0;i<max_buddy-1;i++){
			if(i>=hl.self) j=1;
			dstVector[i].index=i+j;
			dstVector[i].dst=edge_cost[hl.self][i+j];
			dstVector[i].s_hop_reach=(edge_cost[hl.self][i+j]<0)?0:1;
		}

		sort(dstVector,0,hl.num_hosts-2);
		strcpy(tl[0],hl.hostname[dstVector[0].index]);

		/* :: calculating transmission list :: */
		for(j=0;j<hl.num_hosts-1;j++){
			chng=0;
			for(i=j+1;i<hl.num_hosts-1;i++){
				alt_dst=(edge_cost[dstVector[j].index][dstVector[i].index]>0&&dstVector[j].dst>0)?edge_cost[dstVector[j].index][dstVector[i].index]+dstVector[j].dst:-1;
				if(alt_dst<0);
				else{
					if(dstVector[i].dst<0){
						dstVector[i].dst=alt_dst;
						dstVector[i].s_hop_reach=0;
						chng=1;
					}
					else{
						if(alt_dst>dstVector[i].dst+eps_dst);
						else{
							dstVector[i].dst=alt_dst;
							dstVector[i].s_hop_reach=0;
							chng=1;
						}
					}
				}
			}
			if(chng) sort(dstVector,j+1,hl.num_hosts-2);
		}
		for(i=1;i<hl.num_hosts-1;i++){
			if(dstVector[i].s_hop_reach==1) strcpy(tl[i],hl.hostname[dstVector[i].index]);
		}
		tl_size=i;

		/* :: displaying transmission list :: */
		//printf("\n\n:: Transmission List ::\n\n");
		strcpy((char *)shm_addr+2,itoa(tl_size));
		pos=2+max_buddy_digit+1;
		for(i=0;i<tl_size;i++){
			strcpy((char *)shm_addr+pos,tl[i]);
			pos+=strlen(tl[i])+1;
			//printf("%s\n",tl[i]);
		}
		strcpy((char *)shm_addr,"1");
		while(strcmp((char *)shm_addr,"1")==0);
		strcpy((char *)shm_addr+2,itoa(hl.num_hosts-1));
		pos=2+max_buddy_digit+1;
		for(i=0;i<hl.num_hosts;i++){
			if(i==hl.self) continue;
			strcpy((char *)shm_addr+pos,hl.hostname[i]);
			pos+=strlen(hl.hostname[i])+1;
			//printf("%s\n",hl.hostname[i]);
		}
		strcpy((char *)shm_addr,"1");
		return;
	}
	else{
		printf("%s\n",se_buf);
		printf("Error 0x005 : Expecting 'INIT' or 'RTTC' or 'TBLE'\n");
		close(conn);
		return;
	}
}

static void rttgen(void *hname){
	struct sockaddr_in server_address;
	struct hostent *server;
	struct timeval tstart,tstop;
	int sock=0;
	int i;
	char ack_buf[max_ack_buf_len];
	
	/* :: initializing buffers :: */
	bzero(ack_buf,max_ack_buf_len);
	
	bzero((char *)&server_address,sizeof(server_address));
	server_address.sin_family=AF_INET;
	server_address.sin_port=htons(port_def);
	
	/* :: initializing connection :: */
	//printf(":: Connecting to %s ::\n",(char *)hname);
	server=gethostbyname((char *)hname);
	if(server==NULL){
		perror("Error 0x002");
		return;
	}
	bcopy((char *)server->h_addr,(char *)&server_address.sin_addr.s_addr,server->h_length);
	
	sock=socket(AF_INET,SOCK_STREAM,0);
	if(connect(sock,(struct sockaddr *)&server_address,sizeof(server_address))<0){
		perror("Error 0x003");
		close(sock);
		return;
	}

	gettimeofday(&tstart,NULL);
	write(sock,cmd[1],max_cmd_len+1);
	read(sock,ack_buf,max_ack_buf_len);
	gettimeofday(&tstop,NULL);
	if(strcmp(ack_buf,ack[1])==0){
		for(i=0;i<hl.num_hosts;i++){
			if(strcmp(hl.hostname[i],(char *)hname)==0) break;
		}
		hl.rtt[i]=(long int)((tstop.tv_sec-tstart.tv_sec)*1000000+(tstop.tv_usec-tstart.tv_usec));
	}
	else{
		printf("Error 0x004 : Expecting 'RTAK'\n");
		close(sock);
		return;
	}
	close(sock);
	return;
}

/* :: auxiliary functions :: */

void sort(dstNode dstVector[],int st,int en){
	int pivot;
	int left=st;
	int right=en;
	dstNode tmp;
	
	if(en>st){
		pivot=(st+en)/2;
		while(left<=pivot&&right>=pivot){
			while((dstVector[left].dst<dstVector[pivot].dst||dstVector[pivot].dst<0)&&left<=pivot&&dstVector[left].dst>0)
				left=left+1;
			while((dstVector[right].dst>dstVector[pivot].dst||dstVector[right].dst<0)&&right>=pivot&&dstVector[pivot].dst>0)
				right=right-1;

			tmp=dstVector[left];
			dstVector[left]=dstVector[right];
			dstVector[right]=tmp;
			
			left++;
			right--;

			if(left-1==pivot)
				pivot=++right;
			else if (right+1==pivot)
				pivot=--left;
		}
		sort(dstVector,st,pivot-1);
		sort(dstVector,pivot+1,en);
	}
}

static char *itoa(long int i){
	char buf[max_int_digits+2];
	char *p=buf+max_int_digits+1;
	if(i>=0){
		do{
			*--p='0'+(i%10);
			i/=10;
		}while(i!=0);
		return p;
	}
	else{
		do{
			*--p='0'-(i%10);
			i/=10;
		}while(i!=0);
		*--p='-';
	}
	return p;
}
