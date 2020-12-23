HPATH =/usr/include/
HEADERS = -I$(HPATH)libavformat -I$(HPATH)libavcodec -I$(HPATH)libavdevice -I$(HPATH)libswscale -I$(HPATH)libavfilter -I$(HPATH)libavutil -I$(HPATH)libpostproc -I/usr/include/SDL -I/usr/include
LINKS =  -L/usr/local/lib -L/usr/lib 
#LIBS = -lavutil -lavformat -lavcodec -lz  -lm -lswscale  -lcvaux -lhighgui -lcv -lcxcore -lavdevice 

LIBSNO = -lavutil -lavformat -lavcodec -lz  -lm -lswscale  -lavdevice -lavfilter -lpostproc -lSDL

all:camera.o encoder.o
	g++ -g -Wall $(HEADERS) $(LINKS) $(LIBSNO) capture.cpp -o capture camera.o encoder.o
encoder.o:encoder.cpp
	g++ -g -Wall $(HEADERS) $(LINKS) $(LIBSNO) -c  encoder.cpp
camera.o:camera.cpp
	g++ -c -g -Wall $(HEADERS) $(LINKS) $(LIBSNO)    camera.cpp
clean:
	rm *.o first a.out tutorial *~

