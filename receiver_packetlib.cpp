/***************************************************************************
 receiver_packetlib.cpp
 -------------------
 copyright            : (C) 2014 Andrea Zoli, Andrea Bulgarelli,
                                 Valentina Fioretti
 email                : zoli@iasfbo.inaf.it, bulgarelli@iasfbo.inaf.it,
                        fioretti@iasfbo.inaf.it
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software for non commercial purpose              *
 *   and for public research institutes; you can redistribute it and/or    *
 *   modify it under the terms of the GNU General Public License.          *
 *   For commercial purpose see appropriate license terms                  *
 *                                                                         *
 ***************************************************************************/

#include <L0.pb.h>
#include <zmq.hpp>
#include <zmq_utils.h>
#include <iostream>
#include <packet/Packet.h>
#include <packet/PacketStream.h>
#include <CTAMDArray.h>
#include <chrono>
#include <ctime>

#undef DEBUG

int main (int argc, char *argv [])
{
	if(argc <= 1)
	{
		std::cout << "Wrong arguments, please provide config file." << std::endl;
		std::cout << argv[0] << " config.xml" << std::endl;
		return 1;
	}
	PacketLib::PacketStream ps(argv[1]);

	// open a ZMQ_PULL connection with the DAQ
	zmq::context_t context;
	zmq::socket_t sock(context, ZMQ_PULL);
	sock.bind("tcp://*:5556");

	CTAConfig::CTAMDArray array_conf;
	std::cout << "Preloading.." << std::endl;
	array_conf.loadConfig("AARPROD2", "PROD2_telconfig.fits.gz", "Aar.conf", "/home/zoli/local.rtaproto2/share/ctaconfig/");
	std::cout << "Load complete!" << std::endl;

	unsigned long message_count = 0;
	unsigned long message_size = 0;

	// wait for first message (for timer) and skip it
	std::cout << "Waiting for streamer.." << std::endl;
	zmq::message_t message;
	sock.recv(&message);
	unsigned long nummessages = *((long*)message.data());
	std::cout << "Receiving " << nummessages << " messages.." << std::endl;

	std::chrono::time_point<std::chrono::system_clock> start, end;
	start = std::chrono::system_clock::now();
	while(message_count < nummessages)
	{
		sock.recv(&message);

		message_count++;
		message_size += message.size();

		/// decoding packetlib packet
		PacketLib::ByteStreamPtr stream = PacketLib::ByteStreamPtr(new PacketLib::ByteStream((PacketLib::byte*)message.data(), message.size(), false));
		PacketLib::Packet *packet = ps.getPacket(stream);

		/// get telescope id
		PacketLib::DataFieldHeader* dfh = packet->getPacketDataFieldHeader();
		const unsigned int telescopeID = dfh->getFieldValue_16ui("TelescopeID");

		/// get the waveforms
		PacketLib::byte* buff = packet->getData()->getStream();
		PacketLib::dword buffSize = packet->getData()->size();

		/// get npixels and nsamples from ctaconfig using the telescopeID
		CTAConfig::CTAMDTelescopeType* teltype = array_conf.getTelescope(telescopeID)->getTelescopeType();
		int telTypeSim = teltype->getID();
		const unsigned int npixels = teltype->getCameraType()->getNpixels();
		const unsigned int nsamples = teltype->getCameraType()->getPixel(0)->getPixelType()->getNSamples();
	}
	end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed = end-start;
	double msgs = message_count / elapsed.count();
	double mbits = ((message_size / 1000000) * 8) / elapsed.count();
	std::cout << message_count << "messages sent in " << elapsed.count() << " s" << std::endl;
	std::cout << "mean message size: " << message_size / message_count << std::endl;
	std::cout << "throughput: " << msgs << " msg/s = " << mbits << " mbit/s" << std::endl;

	return 0;
}
