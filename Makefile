all: streamer receiver_protobuf receiver_packetlib

streamer: streamer.cpp
	g++ -g -O0 -std=c++11 -Wall streamer.cpp -o streamer -lCTAToolsCore -lpacket -lprotobuf -lzmq

receiver_protobuf: receiver_protobuf.cpp
	g++ -g -O0 -std=c++11 -Wall receiver_protobuf.cpp -o receiver_protobuf -lcfitsio -lCTAConfig -lCTAToolsCore -lRTAUtils -lpacket -lprotobuf -lzmq

receiver_packetlib: receiver_packetlib.cpp
	g++ -g -O0 -std=c++11 -Wall receiver_packetlib.cpp -o receiver_packetlib -lcfitsio -lCTAConfig -lCTAToolsCore -lRTAUtils -lpacket -lprotobuf -lzmq

clean:
	rm -f streamer receiver_protobuf receiver_packetlib
