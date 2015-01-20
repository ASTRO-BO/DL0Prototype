/***************************************************************************
 PacketLib .packet streamer/receiver using OpenMPI.
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
#include <iostream>
#include <packet/Packet.h>
#include <packet/PacketStream.h>
#include <packet/PacketBufferV.h>
#include <CTAMDArray.h>
#include <chrono>
#include <ctime>
#include <cstdlib>



int main(int argc, char* argv[])
{
	if(argc <= 3)
	{
		std::cout << "Wrong arguments, please provide config file, raw input file and number of packets." << std::endl;
		std::cout << argv[0] << " <configfile> <inputfile> <npackets>" << std::endl;
		return 1;
	}
	const std::string configFilename(realpath(argv[1], NULL));
	const std::string inputFilename(realpath(argv[2], NULL));
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
		PacketLib::PacketBufferV events(configFilename, inputFilename);
		events.load();

		// send events
		unsigned int idx = 0, counter = 0;
		while(counter++ < numevents)
		{
			PacketLib::ByteStreamPtr event = events.getNext();
			// send number of events
			unsigned long eventSize = event->size();
			MPI::COMM_WORLD.Send(&eventSize, 1, MPI_UNSIGNED_LONG, 1, 0);
			MPI::COMM_WORLD.Send(event->getStream(), eventSize, MPI_UNSIGNED_CHAR, 1, 0);

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

			/// decoding packetlib packet
			PacketLib::ByteStreamPtr stream = PacketLib::ByteStreamPtr(new PacketLib::ByteStream(event, eventSize, false));
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
	}

	MPI::Finalize();

	return 0;
}
