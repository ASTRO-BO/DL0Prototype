all: streamer1 receiver1

streamer1: streamer1.cpp
	g++ -g -O0 -Wall streamer1.cpp -o streamer1 -lCTAToolsCore -lpacket -lprotobuf -lzmq

receiver1: receiver1.cpp
	g++ -g -O0 -std=c++11 -Wall receiver1.cpp -o receiver1 -lcfitsio -lCTAConfig -lCTAToolsCore -lRTAUtils -lpacket -lprotobuf -lzmq

clean:
	rm -f streamer1 receiver1
