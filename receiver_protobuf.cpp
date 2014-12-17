#include <L0.pb.h>
#include <zmq.hpp>
#include <zmq_utils.h>
#include <iostream>
#include <packet/Packet.h>
#include <CTAMDArray.h>

#undef DEBUG

int main (int argc, char *argv [])
{
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

		/// parsing CTAMessage
		CTADataModel::CTAMessage ctaMsg;
		if (!ctaMsg.ParseFromArray(message.data(), message.size()))
		{
			std::cerr << "Warning: Wrong message type, are sure you are connected to a CTATools Streamer?" << std::endl;
			return 1;
		}

		CTADataModel::CameraEvent event;
		google::protobuf::RepeatedField<int> types = ctaMsg.payload_type();
		google::protobuf::RepeatedPtrField<std::string> buffers = ctaMsg.payload_data();
		for(int i=0; i<	buffers.size(); i++)
		{
			if(types.Get(i) == CTADataModel::CAMERA_EVENT)
			{
				/// parsing CameraEvent
				event.ParseFromString(buffers.Get(i));
/*				const unsigned int telescopeID = event.telescopeid();
				const CTADataModel::PixelsChannel& higain = event.higain();
				const CTADataModel::WaveFormData& waveforms = higain.waveforms();
				int nsamples = waveforms.num_samples();
				const CTADataModel::CTAArray& samples = waveforms.samples();
				unsigned char *buff = (unsigned char*) samples.data().c_str();
				unsigned int buffSize = samples.data().size();

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
			}
		}
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
