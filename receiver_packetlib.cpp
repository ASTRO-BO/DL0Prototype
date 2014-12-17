#include <L0.pb.h>
#include <zmq.hpp>
#include <zmq_utils.h>
#include <iostream>
#include <packet/Packet.h>
#include <packet/PacketStream.h>
#include <CTAMDArray.h>

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

	zmq::message_t message;

	CTAConfig::CTAMDArray array_conf;
	std::cout << "Preloading.." << std::endl;
	array_conf.loadConfig("AARPROD2", "PROD2_telconfig.fits.gz", "Aar.conf", "/home/zoli/local.rtaproto2/share/ctaconfig/");
	std::cout << "Load complete!" << std::endl;

	unsigned long message_count = 0;
	unsigned long message_size = 0;
	double megabits = 0.;
	void* watch = zmq_stopwatch_start();

	while(true)
	{
		sock.recv(&message);

		message_count++;
		message_size += message.size();

		/// decoding packetlib packet
		PacketLib::ByteStreamPtr stream = PacketLib::ByteStreamPtr(new PacketLib::ByteStream((PacketLib::byte*)message.data(), message.size(), false));
		PacketLib::Packet *packet = ps.getPacket(stream);
/*		PacketLib::DataFieldHeader* dfh = packet->getPacketDataFieldHeader();
		PacketLib::SourceDataField* sdf = packet->getPacketSourceDataField();
		const unsigned int telescopeID = sdf->getFieldValue_16ui("TelescopeID");
		const unsigned int nsamples = sdf->getFieldValue_32ui("Number of samples");
		PacketLib::byte* buff = packet->getData()->getStream();
		PacketLib::dword buffSize = packet->getData()->size();

		/// get npixels of the camera from telescopeID
		CTAConfig::CTAMDTelescopeType* teltype = array_conf.getTelescope(telescopeID)->getTelescopeType();
		int telTypeSim = teltype->getID();
		const unsigned int npixels = teltype->getCameraType()->getNpixels();*/

#ifdef DEBUG
		std::cout << "telType: " << telTypeSim;
		switch(telTypeSim) {
			case 138704810:
				std::cout << " large" << std::endl;
				break;
			case 10408418:
				std::cout << " medium" << std::endl;
				break;
			case 3709425:
				std::cout << " small" << std::endl;
				break;
		}
		std::cout << "pixels: " << npixels << std::endl;
		std::cout << " samples: " << nsamples << std::endl;
#endif
		if(message_count == 10000) {
			unsigned long elapsed = zmq_stopwatch_stop(watch);
			unsigned long throughput = (unsigned long)
			((double) message_count / (double) elapsed * 1000000);
			megabits = (double) (message_size * 8) / 1000000;
			std::cout << "message size: " << message_size << " [B]" << std::endl;
			std::cout << "message count: " << message_count <<  std::endl;
			std::cout << "mean throughput: " << throughput << " [msg/s]" << std::endl;
			std::cout << "mean throughput: " << megabits << " [Mb/s]" << std::endl;

			message_count = 0;
			message_size = 0;
			watch = zmq_stopwatch_start();
		}
	}

	return 0;
}
