#include <iostream>
extern "C"{
#include <errno.h>
#include <SDL.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <avcodec.h>
#include <avformat.h>
#include <avdevice.h>
#include <swscale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <sys/types.h>
#include <strings.h>
}


#define WIDTH 640
#define HEIGHT 480

#define max_pkt_size_digit 5
#define l1_shm_key_path "temp2"
#define l1_shm_key_id 1
#define l1_max_shm_len 3072
#define MAX_BUDDY 64
pthread_mutex_t mutex;
int global_id=0;
char cap[MAX_BUDDY][12]={"cap0.mp4","cap1.mp4","cap2.mp4","cap3.mp4","cap4.mp4","cap5.mp4","cap6.mp4","cap7.mp4","cap8.mp4","cap9.mp4"};
const int drops = 1;
using namespace std;
void decoder(void *shm_addr);
int main(int argc, char *argv[]){
	int num_hosts = atoi(argv[1])-1;
	/*
	Implement shm_addr
	*/
	key_t shm_key=ftok(l1_shm_key_path,l1_shm_key_id);
	int shm_id=shmget(shm_key,(l1_max_shm_len+2)*num_hosts,0666|IPC_CREAT);
	void *shm_addr=shmat(shm_id,NULL,0);
	bzero((char *)shm_addr,(l1_max_shm_len+2)*num_hosts);
	
	pthread_t tid[MAX_BUDDY];
	int i;
	for(i=0;i<num_hosts;i++){
		pthread_create(&tid[i],(const pthread_attr_t *)NULL,(void* (*)(void*))decoder,(void *)((char *)shm_addr+i*l1_max_shm_len));
		global_id=i;
		usleep(100);
	}
	for(i=0;i<num_hosts;i++){
		pthread_join(tid[i],NULL);
	}
}
void decoder(void *shm_addr)
{
	//strcpy((char *)shm_addr,"0\0");
	// initialize the register information about the formats,codecs,devices etc using the these calls
	int local_id=global_id;
	avcodec_register_all();
	avdevice_register_all();
	av_register_all();
	AVFormatContext *pFormatCtx;
	AVOutputFormat* pOutputFmt;
	pFormatCtx = av_alloc_format_context();
        if(NULL == pFormatCtx)  return;
        pOutputFmt = guess_format(NULL,cap[local_id], NULL);
        if(NULL == pOutputFmt ){
                perror("guess format");
                av_free(pFormatCtx);
                return;
        }
        if(CODEC_ID_NONE == pOutputFmt->video_codec){
                av_free(pFormatCtx);
                av_free(pOutputFmt);
                return;
        }
        //After guessing the output format we convey the format and the filename to the format context
        pFormatCtx->oformat = pOutputFmt;
        snprintf(pFormatCtx->filename, sizeof(pFormatCtx->filename),cap[local_id]);
        
        AVCodecContext *pCodecCtx;
	AVStream *video_st;
	int width=640;
        int height=480;
        int bit_rate=200000;
        enum PixelFormat pix_fmt=PIX_FMT_YUV420P;
	AVRational time_base;
	time_base.den = 24;
	time_base.num = 1;

        video_st = av_new_stream(pFormatCtx, 0);
        if(NULL == video_st){
                perror("alloc stream");
                return;
        }
        //We set the codec parameters and corresponding flags
        pCodecCtx = video_st->codec;
        pCodecCtx->codec_id = pOutputFmt->video_codec;
        pCodecCtx->codec_type = CODEC_TYPE_VIDEO;
        pCodecCtx->bit_rate = bit_rate;
        pCodecCtx->width = width;
        pCodecCtx->height = height;
        pCodecCtx->pix_fmt = pix_fmt;
        pCodecCtx->time_base.den = time_base.den;
        pCodecCtx->time_base.num = time_base.num;
        //setting the global headers for container format 
        if(!strcmp(pOutputFmt->name, "mp4") || !strcmp(pOutputFmt->name, "mov") || !strcmp(pOutputFmt->name, "3gp"))
        pCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        if(av_set_parameters(pFormatCtx, NULL) < 0){
        perror("set parameter");
        return;
        }
	dump_format(pFormatCtx, 0, cap[local_id], 1);

	//From the video stream we fetch the codec info and attempt to find a decoder for it
	AVCodec *pCodec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec == NULL){
	perror("find decoder");
	return;
	}
	pthread_mutex_lock(&mutex);
	if(avcodec_open(pCodecCtx, pCodec)<0){
	pthread_mutex_unlock(&mutex);
	perror("open decoder");
	return;
	}
	pthread_mutex_unlock(&mutex);
	//Creates a new thread for the gui 
	pthread_mutex_lock(&mutex);
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)){
	pthread_mutex_unlock(&mutex);
	perror("SDL_Init");
	return;
	}
	pthread_mutex_unlock(&mutex);
	SDL_Surface *screen;
	//Shows up the window
	pthread_mutex_lock(&mutex);
	screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
	if(!screen){
	pthread_mutex_unlock(&mutex);
	perror("set screen mode");
	return;
	}
	pthread_mutex_unlock(&mutex);
	//We allocate frames both for input and output 
	AVFrame *pFrame, *pFrameOut;
	pthread_mutex_lock(&mutex);
	pFrame = avcodec_alloc_frame();
	pFrameOut = avcodec_alloc_frame();
	pthread_mutex_unlock(&mutex);
	if(pFrameOut == NULL)
	return;
	//we create a yuv overlay over our window
	SDL_Overlay *bmp;
	pthread_mutex_lock(&mutex);
	bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,	SDL_YV12_OVERLAY, screen);
	pthread_mutex_unlock(&mutex);
	uint8_t *buffer;
	int frameFinished;
	AVPicture pict;
	//AVPacket packet;
	int pktsize=0;
	uint8_t *data=(uint8_t *)malloc(3072*sizeof(uint8_t));
	printf("pointer::%p\n",data);
	struct SwsContext* img_convert_ctx;
	//we allocate the buffer and associate with the output frame so that raw data can be put on the buffer
	pthread_mutex_lock(&mutex);
	buffer = (uint8_t*)malloc(avpicture_get_size(PIX_FMT_YUV420P, WIDTH, HEIGHT));
	avpicture_fill((AVPicture*)pFrameOut, buffer, PIX_FMT_YUV420P, WIDTH, HEIGHT);
	pthread_mutex_unlock(&mutex);
	int i=0;
	int prevsize=0;
	SDL_Rect rect;
	strcpy((char *)shm_addr,"0\0");
	while(1) {
	//printf("reached here 1\n");
	//if(strcmp((char *)shm_addr,"1")!=0) continue;
	/*
	read from shm
	*/
	//printf("hello %p\n",pthread_self());
	if(strcmp((char *)shm_addr,"2\0")!=0) continue;
	//printf("hello\n");
	//prevsize=pktsize;
	pktsize=atoi((char *)shm_addr+2);
	//bzero((char *)data,3072);
	//if(pktsize==prevsize) continue;
	//printf("lol\n \t%s\n \t%s\n",(char *)shm_addr,(char *)shm_addr+2);
	bzero((char *)data,3072);
	bcopy((char *)shm_addr+max_pkt_size_digit+3,(char *)data,pktsize);
	bzero((char *)shm_addr,l1_max_shm_len+2);
	//if(packet.size)printf("%d\n",packet.size);
	//strcpy((char *)shm_addr,"0\0");
	//printf("before decode\n");
	//if(data[10]==0 && data[1000]==0)continue;
	//strcpy((char *)shm_addr,"2\0");
	pthread_mutex_lock(&mutex);
	avcodec_decode_video(pCodecCtx, pFrame, &frameFinished,data, pktsize);
	pthread_mutex_unlock(&mutex);
	//printf("after decode\n");
	if(frameFinished) {
	//we lock the pixel before making any changes
	pthread_mutex_lock(&mutex);
	SDL_LockYUVOverlay(bmp);
	pthread_mutex_unlock(&mutex);
	//we pass the pointers of all the three planes of pixels and the length of the dimensions 
	pict.data[0] = bmp->pixels[0];
	pict.data[1] = bmp->pixels[2];
	pict.data[2] = bmp->pixels[1];
	
	pict.linesize[0] = bmp->pitches[0];
	pict.linesize[1] = bmp->pitches[1];
	pict.linesize[2] = bmp->pitches[2];
	//printf("before convert \n");
	//we convert from raw YUYV422 to YUV420P format
	pthread_mutex_lock(&mutex);
	img_convert_ctx = sws_getContext(pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt,pCodecCtx->width, pCodecCtx->height, PIX_FMT_YUV420P, SWS_BICUBIC,NULL,NULL, NULL);
	//printf("after convert \n");
	sws_scale(img_convert_ctx,pFrame->data,pFrame->linesize,0,pCodecCtx->height,pict.data,pict.linesize);
	pthread_mutex_unlock(&mutex);
	//printf("after sws_scale\n");
	//we map the changed format frame to the output frame
//	if(++i == drops){
	pFrameOut->data[0] = pict.data[0];
	pFrameOut->data[1] = pict.data[1];
	pFrameOut->data[2] = pict.data[2];
	pFrameOut->linesize[0] = pict.linesize[0];
	pFrameOut->linesize[1] = pict.linesize[1];
	pFrameOut->linesize[2] = pict.linesize[2];
	//encode and save the frames in mp4 format
//	i = 0;
//		}
		//we unlock the pixels
	pthread_mutex_lock(&mutex);
	SDL_UnlockYUVOverlay(bmp);
	pthread_mutex_unlock(&mutex);
	//we format a rectangle of the size of image and display those pixels 
	rect.x = 0;
	rect.y = 0;
	rect.w = pCodecCtx->width;
	rect.h = pCodecCtx->height;
	pthread_mutex_lock(&mutex);
	SDL_DisplayYUVOverlay(bmp, &rect);
	//printf("before free context\n");
	sws_freeContext(img_convert_ctx);
	pthread_mutex_unlock(&mutex);
	//printf("after freee context\n");
	}
	//av_free_packet(&packet);
	
/*	SDL_Event event;
	//Standard SDL eventhandler 
	SDL_PollEvent(&event);
	switch(event.type){
	case SDL_QUIT:
	SDL_Quit();
	return ;
	break;	
	default:
	break;
	}*/
	strcpy((char *)shm_addr,"0\0");
	}
	pthread_mutex_lock(&mutex);
	av_free(buffer);
	av_free(pFrameOut);
	av_free(pFrame);
	pthread_mutex_unlock(&mutex);
	pthread_mutex_lock(&mutex);
	avcodec_close(pCodecCtx);
	pthread_mutex_unlock(&mutex);
	return ;
}

