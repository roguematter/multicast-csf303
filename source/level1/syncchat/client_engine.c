#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>

#define l1_shm_key_path "temp2"
#define l1_shm_key_id 1
#define l1_max_shm_len 3072
#define max_hname_len 64
#define MAX_BUDDY 10

void decoder(void *shm_addr);
void main(int argc, char *argv[]){
	int num_hosts = atoi(argv[1])-1;
	key_t shm_key=ftok(l1_shm_key_path,l1_shm_key_id);
	int shm_id=shmget(shm_key,(l1_max_shm_len+2)*num_hosts,0666|IPC_CREAT);
	void *shm_addr=shmat(shm_id,NULL,0);
	bzero((char *)shm_addr,(l1_max_shm_len+2)*num_hosts);
	
	pthread_t tid[MAX_BUDDY];
	int i;
	for(i=0;i<num_hosts;i++){
		pthread_create(&tid[i],(const pthread_attr_t *)NULL,(void* (*)(void*))decoder,(void *)((char *)shm_addr+i*(l1_max_shm_len+2)));
	}
	for(i=0;i<num_hosts;i++){
		pthread_join(tid[i],NULL);
	}
}
void decoder(void *shm_addr){
	strcpy((char *)shm_addr,"0\0");
	while(1) {
		if(strcmp((char *)shm_addr,"2\0")!=0) continue;
		printf("%s%s%s\n",(char *)shm_addr+2,(char *)shm_addr+max_hname_len+2,(char *)shm_addr+max_hname_len+12);
		bzero((char *)shm_addr,l1_max_shm_len+2);
		strcpy((char *)shm_addr,"0\0");
	}
}
