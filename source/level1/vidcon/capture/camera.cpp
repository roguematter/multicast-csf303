#include <iostream>
extern "C"{
#include <errno.h>
#include <linux/videodev2.h>
#include <avcodec.h>
#include <avformat.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
}
#include "camera.h"




//Repeatedly calls ioctl until successful
static int xioctl(int fd, int request, void *arg)
{
int ret;
do ret = ioctl(fd, request, arg);
while (-1 == ret && EINTR == errno);
return ret;
}

Ccamera::Ccamera()
{
	m_width = 640;
	m_height = 480;
	m_device ="/dev/video0";
	fmt_ctx = NULL;
	video_st = NULL;
	iformat = av_find_input_format("video4linux2");
}

Ccamera::~Ccamera()
{
	if(video_st != NULL)
	close();
}

AVStream* Ccamera::open_stream()
{
	AVFormatParameters params,* formatParams =&params;
	memset(formatParams, 0, sizeof(params));
	fmt_ctx = avformat_alloc_context();

	if(-1 == av_open_input_file(&fmt_ctx, m_device, iformat, 0, formatParams)){
	perror("Camera open: open input file");
	return NULL;
	}
	//finds the characterstics of the stream like the fps and attempt decoding it if possible
	av_find_stream_info(fmt_ctx);
	#ifdef DEBUiG
	dump_format(fmt_ctx, 0, m_device, 0);
	#endif
	//fetching the video stream from the format context data structure and put it in video_st
	video_st = fmt_ctx->streams[0];
	return video_st;
}

int Ccamera::close()
{
	av_close_input_file(fmt_ctx);
	video_st = NULL;
	return 0;
}

		
