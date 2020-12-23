extern "C"{
#include <avformat.h>
}

/*The camera object has the information related to the incoming stream  the device path 
*/
class Ccamera{
	public:
	char *m_device;
	int m_width; 
	int m_height; 
	AVStream *video_st;
	AVFormatContext *fmt_ctx;
	AVInputFormat *iformat;
	
	Ccamera();
	~Ccamera();
	AVStream* open_stream(); 
	int close(); 
};


