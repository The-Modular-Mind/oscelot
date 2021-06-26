
#include "vcvOscSender.hpp"

//--------------------------------------------------------------
vcvOscSender::~vcvOscSender()
{
	clear();
}

//--------------------------------------------------------------
bool vcvOscSender::setup(std::string &host, int port)
{
	this->host = host;
	this->port = port;

	// manually set larger buffer size instead of oscpack per-message size
	// if (UdpSocket::GetUdpBufferSize() == 0)
	// {
	// 	UdpSocket::SetUdpBufferSize(65535);
	// }

	// check for empty host
	if (host == "")
	{
		host = "localhost";
	}

	// create socket
	UdpTransmitSocket *socket = nullptr;
	try
	{
		IpEndpointName name = IpEndpointName(host.c_str(), port);
		if (!name.address)
		{
			// ofLogError("vcvOscSender") << "bad host? " << settings.host;
			return false;
		}
		socket = new UdpTransmitSocket(name);
		sendSocket.reset(socket);
	}
	catch (std::exception &e)
	{
		std::string what = e.what();
		// ofLogError("vcvOscSender") << "couldn't create sender to "
		// 						   << settings.host << " on port "
		// 						   << settings.port << ": " << what;
		if (socket != nullptr)
		{
			delete socket;
			socket = nullptr;
		}
		sendSocket.reset();
		return false;
	}
	return true;
}

//--------------------------------------------------------------
void vcvOscSender::clear()
{
	sendSocket.reset();
}

//--------------------------------------------------------------
void vcvOscSender::sendBundle(const vcvOscBundle &bundle)
{
	if (!sendSocket)
	{
		// ofLogError("vcvOscSender") << "trying to send with empty socket";
		return;
	}

	// setting this much larger as it gets trimmed down to the size its using before being sent.
	// TODO: much better if we could make this dynamic? Maybe have vcvOscBundle return its size?
	static const int OUTPUT_BUFFER_SIZE = 327680;
	char buffer[OUTPUT_BUFFER_SIZE];
	osc::OutboundPacketStream p(buffer, OUTPUT_BUFFER_SIZE);

	// serialise the bundle and send
	appendBundle(bundle, p);
	sendSocket->Send(p.Data(), p.Size());
}

//--------------------------------------------------------------
void vcvOscSender::sendMessage(const vcvOscMessage &message, bool wrapInBundle)
{
	if (!sendSocket)
	{
		// ofLogError("vcvOscSender") << "trying to send with empty socket";
		WARN("vcvOscSender %s", "trying to send with empty socket");
		return;
	}

	// setting this much larger as it gets trimmed down to the size its using before being sent.
	// TODO: much better if we could make this dynamic? Maybe have vcvOscMessage return its size?
	static const int OUTPUT_BUFFER_SIZE = 327680;
	char buffer[OUTPUT_BUFFER_SIZE];
	osc::OutboundPacketStream p(buffer, OUTPUT_BUFFER_SIZE);

	// serialise the message and send
	if (wrapInBundle)
	{
		p << osc::BeginBundleImmediate;
	}
	appendMessage(message, p);
	if (wrapInBundle)
	{
		p << osc::EndBundle;
	}
	sendSocket->Send(p.Data(), p.Size());
}

// PRIVATE
//--------------------------------------------------------------
void vcvOscSender::appendBundle(const vcvOscBundle &bundle, osc::OutboundPacketStream &p)
{
	// recursively serialise the bundle
	p << osc::BeginBundleImmediate;
	for (int i = 0; i < bundle.getBundleCount(); i++)
	{
		appendBundle(bundle.getBundleAt(i), p);
	}
	for (int i = 0; i < bundle.getMessageCount(); i++)
	{
		appendMessage(bundle.getMessageAt(i), p);
	}
	p << osc::EndBundle;
}

//--------------------------------------------------------------
void vcvOscSender::appendMessage(const vcvOscMessage &message, osc::OutboundPacketStream &p)
{
	p << osc::BeginMessage(message.getAddress().c_str());
	for (size_t i = 0; i < message.getNumArgs(); ++i)
	{
		switch (message.getArgType(i))
		{
		case osc::INT32_TYPE_TAG:
			p << message.getArgAsInt(i);
			break;
		case osc::FLOAT_TYPE_TAG:
			p << message.getArgAsFloat(i);
			break;
		case osc::STRING_TYPE_TAG:
			p << message.getArgAsString(i).c_str();
			break;
		default:
			// ofLogError("vcvOscSender") << "appendMessage(): bad argument type "
			//    << message.getArgType(i) << " '" << (char)message.getArgType(i) << "'";
			break;
		}
	}
	p << osc::EndMessage;
}
