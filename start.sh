clear
echo ".......................................................................................\n"
cd bin
echo "run interface:\c"
read self
./streamer 0 $self &
./resp $self &
echo "quit?(enter):?\n\n"
read quit
killall -9 resp
killall -9 streamer
killall -9 chat_engine
killall -9 chat_client
killall -9 decoder
killall -9 capture
echo "\n.......................................................................................\n"
