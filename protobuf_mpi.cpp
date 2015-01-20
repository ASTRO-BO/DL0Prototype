/***************************************************************************
 Protobuf message streamer/receiver using OpenMPI.
 ---------------------------------------------------------------------------
 copyright            : (C) 2014 Andrea Zoli
 email                : zoli@iasfbo.inaf.it
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software for non commercial purpose              *
 *   and for public research institutes; you can redistribute it and/or    *
 *   modify it under the terms of the GNU General Public License.          *
 *   For commercial purpose see appropriate license terms                  *
 *                                                                         *
 ***************************************************************************/
#include <mpi.h>
#include <L0.pb.h>
#include <BasicDefs.hpp>
#include <iostream>
#include <packet/Packet.h>
#include <packet/PacketStream.h>
#include <packet/PacketBufferV.h>
#include <CTAMDArray.h>
#include <chrono>
#include <ctime>
#include <cstdlib>

CTADataModel::CameraEvent* packetToCameraEventMessage(PacketLib::Packet* packet)
{
	PacketLib::DataFieldHeader* dfh = packet->getPacketDataFieldHeader();
	PacketLib::SourceDataField* sdf = packet->getPacketSourceDataField();
	const unsigned int telescopeID = dfh->getFieldValue_16ui("TelescopeID");
	// FIXME dateMJD should be something like:
	// unsigned int LTtime = dfh->getFieldValue_64ui("LTtime");
	// handle the 64 bits
	const unsigned int dateMJD = 1;
	CTADataModel::EventType eventType = CTADataModel::PHYSICAL;
	const unsigned int eventNumber = dfh->getFieldValue_32ui("eventNumber");
	const unsigned int arrayEvtNum = dfh->getFieldValue_32ui("ArrayID");
	PacketLib::byte* data = packet->getData()->getStream();
	PacketLib::dword size = packet->getData()->size();

	const unsigned int num_samples = sdf->getFieldValue_32ui("Number of samples");

	// Fill a new event
	CTADataModel::CameraEvent* event = new CTADataModel::CameraEvent();
	event->set_telescopeid(telescopeID);
	event->set_datemjd(dateMJD);
	event->set_eventtype(eventType);
	event->set_eventnumber(eventNumber);
	event->set_arrayevtnum(arrayEvtNum);

	CTADataModel::PixelsChannel* higain = event->mutable_higain();
	CTADataModel::WaveFormData* waveforms = higain->mutable_waveforms();
	CTADataModel::CTAArray* samples = waveforms->mutable_samples();
	samples->set_data((const char*)data, size);
	waveforms->set_num_samples(num_samples);

	return event;
}

int main(int argc, char* argv[])
{
	if(argc <= 3)
	{
		std::cout << "Wrong arguments, please provide config file, raw input file and number of packets." << std::endl;
		std::cout << argv[0] << " <configfile> <inputfile> <npackets>" << std::endl;
		return 1;
	}
	const std::string configFilename = argv[1];
	const std::string inputFilename = argv[2];
	const unsigned long numevents = std::atol(argv[3]);

	// init MPI
	MPI::Init();
	int size = MPI::COMM_WORLD.Get_size();
	int rank = MPI::COMM_WORLD.Get_rank();
	char hostname[MPI_MAX_PROCESSOR_NAME];
	int hostnameLen;
	MPI::Get_processor_name(hostname, hostnameLen);
	std::cout << "host: " << hostname << " size: " << size << " rank: " << rank << std::endl;

	if(rank == 0) // streamer
	{
		// load events buffer
		PacketLib::PacketBufferV packets(configFilename, inputFilename);
		packets.load();
		PacketLib::PacketStream ps(configFilename.c_str());
		std::vector<CTADataModel::CameraEvent*> events;
		for(int i=0; i<packets.size(); i++)
		{
			PacketLib::ByteStreamPtr rawPacket = packets.getNext();
			PacketLib::Packet *p = ps.getPacket(rawPacket);
			events.push_back(packetToCameraEventMessage(p));
		}

		// send events
		unsigned int idx = 0, counter = 0;
		while(counter++ < numevents)
		{
			CTADataModel::CameraEvent* event = events[idx];

			// build the payload header
			char payload_head[8] = {0x22, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
			uint32 this_char = 1;
			uint32 encoded_payload_size = event->ByteSize();
			while (encoded_payload_size)
			{
				payload_head[this_char]  = 0x80;
				payload_head[this_char] |= encoded_payload_size & 0x7f;
				encoded_payload_size     = encoded_payload_size >> 7;
				this_char++;
			}
			if (this_char != 1)
				payload_head[--this_char] &= 0x7f;
			this_char++;

			// serialize first message header, then payload header, then payload
			CTADataModel::CTAMessage message_wrapper;
			message_wrapper.add_payload_type(CTADataModel::NO_TYPE);
			message_wrapper.set_source_name("wrappedname");
			message_wrapper.set_message_count(1);
			message_wrapper.set_payload_type(0, CTATools::Core::extractMessageType(*event));
			uint32 wrapper_size = message_wrapper.ByteSize();
			uint32 messageSize = event->ByteSize() + wrapper_size + this_char;

			char* message = new char[messageSize];
			message_wrapper.SerializeToArray(message, wrapper_size);
			memcpy(message+wrapper_size, payload_head, this_char);
			event->SerializeToArray(message+wrapper_size+this_char, event->ByteSize());

			MPI::COMM_WORLD.Send(&messageSize, 1, MPI_UNSIGNED_LONG, 1, 0);
			MPI::COMM_WORLD.Send(message, messageSize, MPI_UNSIGNED_CHAR, 1, 0);

			delete[] message;

			idx = (idx+1) % events.size();
		}
	}
	else if(rank == 1) // receiver
	{
		CTAConfig::CTAMDArray array_conf;
		std::cout << "Preloading.." << std::endl;
		array_conf.loadConfig("AARPROD2", "PROD2_telconfig.fits.gz", "Aar.conf", "conf/");
		std::cout << "Load complete!" << std::endl;

		unsigned long message_count = 0;
		unsigned long message_size = 0;

		PacketLib::PacketStream ps(configFilename.c_str());

		PacketLib::byte event[500000];
		unsigned long eventSize = 0;

		MPI::COMM_WORLD.Recv(&eventSize, 1, MPI_UNSIGNED_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG);

		std::chrono::time_point<std::chrono::system_clock> start, end;
		start = std::chrono::system_clock::now();
		while(message_count < numevents)
		{
			if(message_count > 0)
				MPI::COMM_WORLD.Recv(&eventSize, 1, MPI_UNSIGNED_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG);
			MPI::COMM_WORLD.Recv(&event, eventSize, MPI_UNSIGNED_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG);

			message_count++;
			message_size += eventSize;

			/// parsing CTAMessage
			CTADataModel::CTAMessage ctaMsg;
			if (!ctaMsg.ParseFromArray(event, eventSize))
			{
				std::cerr << "Warning: Wrong message type, are sure you are connected to a CTATools Streamer?" << std::endl;
				return 1;
			}

			CTADataModel::CameraEvent cameraEvent;
			google::protobuf::RepeatedField<int> types = ctaMsg.payload_type();
			google::protobuf::RepeatedPtrField<std::string> buffers = ctaMsg.payload_data();
			for(int i=0; i<	buffers.size(); i++)
			{
				if(types.Get(i) == CTADataModel::CAMERA_EVENT)
				{
					/// parsing CameraEvent
					cameraEvent.ParseFromString(buffers.Get(i));

					/// get telescope id
					const unsigned int telescopeID = cameraEvent.telescopeid();

					/// get the waveforms
					const CTADataModel::PixelsChannel& higain = cameraEvent.higain();
					const CTADataModel::WaveFormData& waveforms = higain.waveforms();
					const CTADataModel::CTAArray& samples = waveforms.samples();
					unsigned char *buff = (unsigned char*) samples.data().c_str();
					unsigned int buffSize = samples.data().size();

					/// get npixels and nsamples from ctaconfig using the telescopeID
					CTAConfig::CTAMDTelescopeType* teltype = array_conf.getTelescope(telescopeID)->getTelescopeType();
					int telTypeSim = teltype->getID();
					const unsigned int npixels = teltype->getCameraType()->getNpixels();
					const unsigned int nsamples = teltype->getCameraType()->getPixel(0)->getPixelType()->getNSamples();
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
	}

	MPI::Finalize();

	return 0;
}
