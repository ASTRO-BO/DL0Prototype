all: streamer receiver_protobuf receiver_packetlib packetlib_mpi protobuf_mpi

MPIFLAGS=$(shell mpic++ --showme:compile)
MPIFLAGS+= $(shell mpic++ --showme:link)

streamer: streamer.cpp
	g++ -O2 -std=c++11 -Wall streamer.cpp -o streamer -lCTAToolsCore -lpacket -lprotobuf -lzmq

receiver_protobuf: receiver_protobuf.cpp
	g++ -O2 -std=c++11 -Wall receiver_protobuf.cpp -o receiver_protobuf -lcfitsio -lCTAConfig -lCTAToolsCore -lRTAUtils -lpacket -lprotobuf -lzmq

receiver_packetlib: receiver_packetlib.cpp
	g++ -O2 -std=c++11 -Wall receiver_packetlib.cpp -o receiver_packetlib -lcfitsio -lCTAConfig -lCTAToolsCore -lRTAUtils -lpacket -lprotobuf -lzmq

packetlib_mpi: packetlib_mpi.cpp
	mpic++ -std=c++11 -O2 -Wall packetlib_mpi.cpp -o packetlib_mpi -lcfitsio -lCTAConfig -lRTAUtils -lpacket $(MPIFLAGS)

protobuf_mpi: protobuf_mpi.cpp
	mpic++ -std=c++11 -O2 -Wall protobuf_mpi.cpp -o protobuf_mpi -lcfitsio -lCTAConfig -lRTAUtils -lCTAToolsCore -lpacket -lprotobuf $(MPIFLAGS)

clean:
	rm -f streamer receiver_protobuf receiver_packetlib packetlib_mpi protobuf_mpi
