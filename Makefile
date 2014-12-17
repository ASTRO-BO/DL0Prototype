all: streamer receiver1

streamer: streamer.cpp
	g++ -g -O0 -std=c++11 -Wall streamer.cpp -o streamer -lCTAToolsCore -lpacket -lprotobuf -lzmq

receiver1: receiver1.cpp
	g++ -g -O0 -std=c++11 -Wall receiver1.cpp -o receiver1 -lcfitsio -lCTAConfig -lCTAToolsCore -lRTAUtils -lpacket -lprotobuf -lzmq

clean:
	rm -f streamer receiver1
