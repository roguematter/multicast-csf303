clear
cd bin
rm temp*
touch temp0
touch temp1
touch temp2
echo ".......................................................................................\n"
echo "run interface:\c"
read self
echo "num hosts:\c"
read nhosts
echo "run mode:\c"
read mode
if [ $mode = chat ]
then
	echo "...run start.sh"
	./chat_client $nhosts &
	./chat_engine $self
else
	./capture & 
	./decoder $nhosts &
	./streamer 0 $self &
	./resp $self &
	echo "quit?(enter):?\n\n"
	read quit
	killall -9 resp
	killall -9 streamer
	killall -9 decoder
	killall -9 capture
fi
echo "\n.......................................................................................\n"
