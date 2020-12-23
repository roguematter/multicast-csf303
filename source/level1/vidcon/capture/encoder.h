extern "C"{
#include <avformat.h>
#include <avcodec.h>
}
/*Encoder object contains the size of the frame,the bit-rate,pixel format,frame-rate,filename,headers or the format for the container ,the video stream and the size of the buffer associated with the encoder.
Functionality:It opens the stream using the open call and encodes the frames using encode call
*/
class Cencoder
{
	public:
	int width;
	int height;
	int bit_rate;
	enum PixelFormat pix_fmt;
	AVRational time_base;
	char *filename;
	int max_b_frames;
	AVFormatContext *pFormatCtx;
	AVOutputFormat *pOutputFmt;
	AVStream *video_st;
	int out_size;
	uint8_t *video_outbuf;
	int video_outbuf_size;
	Cencoder();
	~Cencoder();
	int open();
	int encode(AVFrame *pFrame, void *shm_addr);
};


