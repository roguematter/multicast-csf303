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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
}

#include "camera.h"
#include "encoder.h"

#define l1_shm_key_path "temp1"
#define l1_shm_key_id 1
#define l1_max_shm_len 3072
#define max_hname_len 64

#define WIDTH 640
#define HEIGHT 480
const int drops = 3;
using namespace std;
int main()
{
	key_t shm_key=ftok(l1_shm_key_path,l1_shm_key_id);
	int shm_id=shmget(shm_key,l1_max_shm_len+max_hname_len+2,0666|IPC_CREAT);
	void *shm_addr=shmat(shm_id,NULL,0);
	bzero((char *)shm_addr,l1_max_shm_len+max_hname_len+2);
	strcpy((char *)shm_addr,"0\0");
	
	// initialize the register information about the formats,codecs,devices etc using the these calls
	avcodec_register_all();
	avdevice_register_all();
	av_register_all();
	Cencoder encoder;
	Ccamera camera;
	if(camera.iformat == NULL){perror("create camera");return -1;}
	AVStream *video_st;
	//The camera object fetches the stream
	video_st = camera.open_stream();
	AVCodecContext *pCodecCtx;
	//From the video stream we fetch the codec info and attempt to find a decoder for it
	pCodecCtx = video_st->codec;
	AVCodec *pCodec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec == NULL){
	perror("find decoder");
	return -1;
	}
	if(avcodec_open(pCodecCtx, pCodec)<0){
	perror("open decoder");
	return -1;
	}
	//Creates a new thread for the gui 
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)){
	perror("SDL_Init");
	return -1;
	}
	SDL_Surface *screen;
	//Shows up the window
	screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
	if(!screen){
	perror("set screen mode");
	return -1;
	}
	if(-1 == encoder.open())exit(-1);
	//We allocate frames both for input and output 
	AVFrame *pFrame, *pFrameOut;
	pFrame = avcodec_alloc_frame();
	
	pFrameOut = avcodec_alloc_frame(); if(pFrameOut == NULL)
	return -1;
	//we create a yuv overlay over our window
	SDL_Overlay *bmp;
	bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,
	SDL_YV12_OVERLAY, screen);
	
	uint8_t *buffer;
	int frameFinished;
	AVPicture pict;
	AVPacket packet;
	struct SwsContext* img_convert_ctx;
	//we allocate the buffer and associate with the output frame so that raw data can be put on the buffer
	buffer = (uint8_t*)malloc(avpicture_get_size(PIX_FMT_YUV420P, WIDTH, HEIGHT));
	avpicture_fill((AVPicture*)pFrameOut, buffer, PIX_FMT_YUV420P, WIDTH, HEIGHT);
	int i=0;
	while(1) {
	if(av_read_frame(camera.fmt_ctx, &packet)!=0){continue;}
	avcodec_decode_video(pCodecCtx, pFrame, &frameFinished,
	packet.data, packet.size);
	if(frameFinished) {
	//we lock the pixel before making any changes
	SDL_LockYUVOverlay(bmp);
	//we pass the pointers of all the three planes of pixels and the length of the dimensions 
	pict.data[0] = bmp->pixels[0];
	pict.data[1] = bmp->pixels[2];
	pict.data[2] = bmp->pixels[1];
	
	pict.linesize[0] = bmp->pitches[0];
	pict.linesize[1] = bmp->pitches[1];
	pict.linesize[2] = bmp->pitches[2];
	//we convert from raw YUYV422 to YUV420P format
	img_convert_ctx = sws_getContext(pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt,pCodecCtx->width, pCodecCtx->height, PIX_FMT_YUV420P, SWS_BICUBIC,NULL,NULL, NULL);
	sws_scale(img_convert_ctx,pFrame->data,pFrame->linesize,0,pCodecCtx->height,pict.data,pict.linesize);
	//we map the changed format frame to the output frame
	if(++i == drops){
	pFrameOut->data[0] = pict.data[0];
	pFrameOut->data[1] = pict.data[1];
	pFrameOut->data[2] = pict.data[2];
	pFrameOut->linesize[0] = pict.linesize[0];
	pFrameOut->linesize[1] = pict.linesize[1];
	pFrameOut->linesize[2] = pict.linesize[2];
	//encode and save the frames in mp4 format
	//if(strcmp((char *)shm_addr,"0")!=0) continue;
	encoder.encode(pFrameOut,shm_addr);
		
	i = 0;
		}
		//we unlock the pixels
	SDL_UnlockYUVOverlay(bmp);
	sws_freeContext(img_convert_ctx);
	//we format a rectangle of the size of image and display those pixels 
	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = pCodecCtx->width;
	rect.h = pCodecCtx->height;
	SDL_DisplayYUVOverlay(bmp, &rect);

	}
	av_free_packet(&packet);
	
	SDL_Event event;
	//Standard SDL eventhandler 
	SDL_PollEvent(&event);
	switch(event.type){
	case SDL_QUIT:
	SDL_Quit();
	return 0;
	break;	
	default:
	break;
	}
	}
	av_free(buffer);
	av_free(pFrameOut);
	av_free(pFrame);
	avcodec_close(pCodecCtx);
	return 0;
}

