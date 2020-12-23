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

#define l1_shm_key_path "temp1"
#define l1_shm_key_id 1
#define l1_max_shm_len 3072
#define max_hname_len 64

void main(int argc, char *argv[]){
	key_t shm_key=ftok(l1_shm_key_path,l1_shm_key_id);
	int shm_id=shmget(shm_key,l1_max_shm_len+max_hname_len+2,0666|IPC_CREAT);
	void *shm_addr=shmat(shm_id,NULL,0);
	bzero((char *)shm_addr,l1_max_shm_len+max_hname_len+2);
	strcpy((char *)shm_addr,"0");
	
	while(1){
		if(strcmp((char *)shm_addr,"0")!=0) continue;
		bzero((char *)shm_addr,l1_max_shm_len+max_hname_len+2);
		scanf(" %[^\n]s",(char *)shm_addr+max_hname_len*2+12);
		strcpy((char *)shm_addr+max_hname_len+2,argv[1]);
		strcpy((char *)shm_addr+max_hname_len*2+2," says: ");
		strcpy((char *)shm_addr,"1");
	}
}
