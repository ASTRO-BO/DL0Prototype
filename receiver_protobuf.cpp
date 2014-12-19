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
#include <CTAMDArray.h>
#include <chrono>
#include <ctime>

#undef DEBUG

#define NANO 1000000000L
double timediff(struct timespec start, struct timespec stop){
    double secs = (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) / (double)NANO;
    return secs;
}

int main (int argc, char *argv [])
{
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
				const unsigned int telescopeID = event.telescopeid();
/*				const CTADataModel::PixelsChannel& higain = event.higain();
				const CTADataModel::WaveFormData& waveforms = higain.waveforms();
				int nsamples = waveforms.num_samples();
				const CTADataModel::CTAArray& samples = waveforms.samples();
				unsigned char *buff = (unsigned char*) samples.data().c_str();
				unsigned int buffSize = samples.data().size();

				/// get npixels of the camera from telescopeID
				CTAConfig::CTAMDTelescopeType* teltype = array_conf.getTelescope(telescopeID)->getTelescopeType();
				int telTypeSim = teltype->getID();
				const unsigned int npixels = teltype->getCameraType()->getNpixels();*/

			}
		}
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
