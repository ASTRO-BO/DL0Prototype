#include <L0.pb.h>
#include <Streamer.hpp>
#include <packet/PacketBufferV.h>


class ZMQStreamer
{
public:

	ZMQStreamer(const std::string configFilename, const std::string inputFilename, const std::string sockAddress)
		: _sockAddress(sockAddress)
	{
		_buff = new PacketLib::PacketBufferV(configFilename, inputFilename);
		_buff->load();
		_ps = new PacketLib::PacketStream(configFilename.c_str());
		_streamer = new CTATools::Core::Streamer("DAQStreamer", 1);
		_streamer->addConnection(ZMQ_PUSH, sockAddress);
	}

	virtual ~ZMQStreamer()
	{
		delete _buff;
		delete _ps;
		delete _streamer;
	}

	virtual void sendNextMessage() = 0;

protected:

	PacketLib::PacketBufferV* _buff;
	PacketLib::PacketStream* _ps;

	CTATools::Core::Streamer* _streamer;
	const std::string _sockAddress;
};

class ProtobufStreamer : public ZMQStreamer
{
public:

	ProtobufStreamer(const std::string configFilename, const std::string inputFilename, const std::string sockAddress)
		: ZMQStreamer(configFilename, inputFilename, sockAddress), _idx(0)
	{
		for(int i=0; i<_buff->size(); i++)
		{
			PacketLib::ByteStreamPtr rawPacket = _buff->getNext();
			PacketLib::Packet *p = _ps->getPacket(rawPacket);

			_events.push_back(_packetToCameraEventMessage(p));
		}
	}

	virtual ~ProtobufStreamer()
	{
	}

	virtual void sendNextMessage()
	{
		CTADataModel::CameraEvent* event = _events[_idx];

		_streamer->sendMessage(*event);

		_idx = (_idx+1) % _events.size();
	}

private:

	std::vector<CTADataModel::CameraEvent*> _events;
	unsigned int _idx;

	CTADataModel::CameraEvent* _packetToCameraEventMessage(PacketLib::Packet* packet)
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
};

class PacketLibStreamer : public ZMQStreamer
{
public:

	PacketLibStreamer(const std::string configFilename, const std::string inputFilename, const std::string sockAddress)
		: ZMQStreamer(configFilename, inputFilename, sockAddress), _idx(0)
	{
		for(int i=0; i<_buff->size(); i++)
		{
			PacketLib::ByteStreamPtr event = _buff->getNext();
			_events.push_back(event);
		}
	}

	virtual ~PacketLibStreamer()
	{
	}

	virtual void sendNextMessage()
	{
		PacketLib::ByteStreamPtr event = _events[_idx];

		zmq::message_t msg(event->size());
		memcpy(msg.data(), event->getStream(), event->size());
		_streamer->sendRawMessage(msg);

		_idx = (_idx+1) % _events.size();
	}

private:

	std::vector<PacketLib::ByteStreamPtr> _events;
	unsigned int _idx;
};

int main(int argc, char* argv[])
{
	if(argc <= 3)
	{
		std::cout << "Wrong arguments, please provide config file, raw input file and if send as protobuf or packetlib." << std::endl;
		std::cout << argv[0] << " config.xml input.raw protobuf|packetlib" << std::endl;
		return 1;
	}
	std::string config_filename = argv[1];
	std::string input_filename = argv[2];
	std::string type = argv[3];

	if(type.compare("protobuf") != 0 && type.compare("packetlib") != 0 )
	{
		std::cout << "You should pass protobuf or packetlib as 3rd parameter." << std::endl;
		std::cout << argv[0] << " config.xml input.raw protobuf|packetlib" << std::endl;
		return 1;
	}

	const std::string sockAddress = "tcp://localhost:5556";

	ZMQStreamer* streamer;
	if(type.compare("protobuf") == 0)
		streamer = new ProtobufStreamer(config_filename, input_filename, sockAddress);
	else if(type.compare("packetlib") == 0)
		streamer = new PacketLibStreamer(config_filename, input_filename, sockAddress);

	while(1)
	{
		streamer->sendNextMessage();
	}

	delete streamer;

	return 0;
}
