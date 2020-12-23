#include <iostream>

extern "C"{
#include <stdio.h>
#include <errno.h>
#include <avformat.h>
#include <avcodec.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <strings.h>
}

#include "encoder.h"

#define l1_max_shm_len 3072
#define max_pkt_size_digit_len 5
#define max_hname_len 64

using namespace std;
//we are setting the frame Rate as 15/1 and using the standard size of image 640x480 and the v4l2 default stream uses the YUV420 format and we set a reasonable value for the bit rate and the output buffer size .As we dont want to keep it simple we are keeping 0 b-frames .But finally we may use b frame for more efficient encoding.
Cencoder::Cencoder()
{
	width = 640;
	height = 480;
	bit_rate = 200000;
	time_base.den = 24;
	time_base.num = 1;
	pix_fmt = PIX_FMT_YUV420P;
	filename = "cap.mp4";
	video_outbuf_size = 400000;
	video_outbuf = (uint8_t*)av_malloc(video_outbuf_size);
	max_b_frames = 0;
}
Cencoder::~Cencoder()
{
	if(video_st){
	avcodec_close(video_st->codec);
	av_free(video_outbuf);
	}
	av_write_trailer(pFormatCtx);
	av_freep(video_st);
	if(!(pOutputFmt->flags & AVFMT_NOFILE)){
	url_fclose((pFormatCtx->pb));
	}
	av_free(pFormatCtx);
}
int Cencoder::open()
{
	pFormatCtx = av_alloc_format_context();
	if(NULL == pFormatCtx)	return -1;
	pOutputFmt = guess_format(NULL, filename, NULL);
	if(NULL == pOutputFmt ){
		perror("guess format");
		av_free(pFormatCtx);
		return -1;
	}
	if(CODEC_ID_NONE == pOutputFmt->video_codec){
		av_free(pFormatCtx);
		av_free(pOutputFmt);
		return -1;
	}
	//After guessing the output format we convey the format and the filename to the format context
	pFormatCtx->oformat = pOutputFmt;
	snprintf(pFormatCtx->filename, sizeof(pFormatCtx->filename), filename);
	
	AVCodecContext *c;
	video_st = av_new_stream(pFormatCtx, 0);
	if(NULL == video_st){
		perror("alloc stream");
		return -1;
	}
	//We set the codec parameters and corresponding flags
	c = video_st->codec;
	c->codec_id = pOutputFmt->video_codec;
	c->codec_type = CODEC_TYPE_VIDEO;
	c->bit_rate = bit_rate;
	c->width = width;
	c->height = height;
	c->pix_fmt = pix_fmt;
	c->time_base.den = time_base.den;
	c->time_base.num = time_base.num;
	//setting the global headers for container format 
	if(!strcmp(pOutputFmt->name, "mp4") || !strcmp(pOutputFmt->name, "mov") || !strcmp(pOutputFmt->name, "3gp"))
	c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	if(av_set_parameters(pFormatCtx, NULL) < 0){
	perror("set parameter");
	return -1;
	}
	dump_format(pFormatCtx, 0, filename, 1);
	
	AVCodec *codec;
	//we find the encoder corresponding to our encoder
	codec = avcodec_find_encoder(c->codec_id);
	if(NULL == codec){
		perror("find encoder");
		return -1;
	}
	if(avcodec_open(c, codec) < 0){
		perror("open codec");
		return -1;
	}
}

int Cencoder::encode(AVFrame *pFrame, void *shm_addr)
{
	int ret;
	static int i=0;
	AVCodecContext *c;
	c = video_st->codec;
	//we encode the frame into MPEG4 format
	out_size = avcodec_encode_video(c, video_outbuf, video_outbuf_size, pFrame);
	//put the output buffer into a packet and then write them to the video file 
	if(out_size > 0){
		AVPacket pkt;
		av_init_packet(&pkt);
		if(c->coded_frame->key_frame)
		pkt.flags |= PKT_FLAG_KEY;
		pkt.stream_index = video_st->index;
		pkt.data = video_outbuf;
		pkt.size = out_size;
		pkt.pts=++i;
		pkt.dts=i;
		
		if(out_size>l1_max_shm_len+16) return out_size;
		else{
			if(strcmp((char *)shm_addr,"1\0")!=0){
				sprintf((char *)shm_addr+max_hname_len+2,"%d",out_size);
				bcopy((char *)pkt.data,(char *)shm_addr+max_hname_len+max_pkt_size_digit_len+3,out_size);
				//printf("encoder out (shm): %s\n",(char *)shm_addr+max_hname_len+max_pkt_size_digit_len+3);
				//printf("encoder out (data): %s\n",(char *)pkt.data);
				//((char *)shm_addr)[l1_max_shm_len+1]='\0';
				strcpy((char *)shm_addr,"1\0");
		    }
		}
	}
	return out_size;
}

