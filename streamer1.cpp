#include <L0.pb.h>
#include <Streamer.hpp>
#include <packet/PacketBufferV.h>

CTADataModel::CameraEvent* packetToCameraEventMessage(PacketLib::Packet* packet)
{
    PacketLib::DataFieldHeader* dfh = packet->getPacketDataFieldHeader();
    PacketLib::SourceDataField* sdf = packet->getPacketSourceDataField();
    const unsigned int telescopeID = sdf->getFieldValue_16ui("TelescopeID");
    // FIXME dateMJD should be something like:
    // unsigned int LTtime = dfh->getFieldValue_64ui("LTtime");
    // handle the 64 bits
    const unsigned int dateMJD = 1;
    CTADataModel::EventType eventType = CTADataModel::PHYSICAL;
    const unsigned int eventNumber = sdf->getFieldValue_32ui("eventNumber");
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
	if(argc <= 2)
	{
		std::cout << "Wrong arguments, please provide the .xml or .stream and the raw input file." << std::endl;
		std::cout << "Example: " << argv[0] << " rta_fadc_all.xml Aar.raw" << std::endl;
		return 1;
	}
	std::string config_filename = argv[1];
	std::string input_filename = argv[2];

	PacketLib::PacketBufferV buff(config_filename, input_filename);
	buff.load();
	PacketLib::PacketStream ps(config_filename.c_str());

	CTATools::Core::Streamer streamer("DAQStreamer", 1);
    streamer.addConnection(ZMQ_PUSH, "tcp://localhost:5556");

	std::vector<CTADataModel::CameraEvent*> events;
	for(int i=0; i<buff.size(); i++)
	{
		PacketLib::ByteStreamPtr rawPacket = buff.getNext();
		PacketLib::Packet *p = ps.getPacket(rawPacket);

		events.push_back(packetToCameraEventMessage(p));
	}

	unsigned int i = 0;
	unsigned int eventcounter = 0;
	while(1)
	{
		CTADataModel::CameraEvent* event = events[i];

		int messageSize = event->ByteSize();

		streamer.sendMessage(*event);

		i = (i+1) % events.size();
		eventcounter++;
	}

	//sendEOS();
	return 0;
}
