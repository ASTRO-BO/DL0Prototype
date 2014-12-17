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
	zmq::socket_t* sock = new zmq::socket_t(context, ZMQ_PULL);
	sock->bind("tcp://*:5556");

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
		sock->recv(&message);

		message_count++;
		message_size = message.size();

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
				event.ParseFromString(buffers.Get(i));

				CTAConfig::CTAMDTelescopeType* teltype = array_conf.getTelescope(event.telescopeid())->getTelescopeType();

				// get TelType
				int telTypeSim = teltype->getID();

#ifdef DEBUG
				std::cout << "telType: " << telTypeSim;
				switch(telTypeSim) {
					case 138704810:
					{
						std::cout << " large" << std::endl;
						break;
					}
					case 10408418:
					{
						std::cout << " medium" << std::endl;
						break;
					}
					case 3709425:
					{
						std::cout << " small" << std::endl;
						break;
					}
					default:
					{
						std::cerr << "Warning: bad telescope type, skipping." << std::endl;
						continue;
					}
				}
#endif

				// get number of pixels
				int npixels = teltype->getCameraType()->getNpixels();

				// get samples and number of samples
				const CTADataModel::PixelsChannel& higain = event.higain();
				const CTADataModel::WaveFormData& waveforms = higain.waveforms();
				//const CTADataModel::CTAArray& samples = waveforms.samples();
				int nsamples = waveforms.num_samples();

				//unsigned char *buff = (unsigned char*) samples.data().c_str();
				//unsigned int buffSize = samples.data().size();
#ifdef DEBUG
				cout << "pixels: " << npixels << " samples: " << nsamples << endl;
#endif
			}
		}

		if(message_count == 100000) {

			unsigned long elapsed = zmq_stopwatch_stop(watch);
			if(elapsed == 0)
			{
				std::cout << "Huston, maybe some problem here with elapsed.." << std::endl;
				elapsed = 1;
			}
			unsigned long throughput = (unsigned long)
			((double) message_count / (double) elapsed * 1000000);
			megabits = (double) (throughput * message_size * 8) / 1000000;
			std::cout << "message size: " << message_size << " [B]" << std::endl;
			std::cout << "message count: " << message_count <<  std::endl;
			std::cout << "mean throughput: " << throughput << " [msg/s]" << std::endl;
			std::cout << "mean throughput: " << megabits << " [Mb/s]" << std::endl;
			watch = zmq_stopwatch_start();
			message_count = 0;
		}
	}

	return 0;
}
