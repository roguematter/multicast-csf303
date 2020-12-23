echo "................................................................................................"  >> logs/build_errors
echo "Last build attempt : \c" >> logs/build_errors
date >> logs/build_errors
echo "" >> logs/build_errors
rm bin/temp*
gcc -g -pthread -Wall source/multicast/mcast_init.c -o bin/init >> logs/build_errors
gcc -g -pthread -Wall source/multicast/mcast_resp.c -o bin/resp >> logs/build_errors
gcc -g -pthread -Wall source/streamer/streamer.c -o bin/streamer >> logs/build_errors
gcc -g -pthread -Wall source/level1/syncchat/chat_engine.c -o bin/chat_engine >> logs/build_errors
gcc -g -pthread -Wall source/level1/syncchat/client_engine.c -o bin/chat_client >> logs/build_errors
touch bin/temp0
touch bin/temp1
touch bin/temp2
cd source/level1/vidcon/capture
make
cp capture ../../../../bin/
cd ../decoder
make
cp decoder ../../../../bin/
cd ../../../../
echo "\n................................................................................................\n"  >> logs/build_errors
