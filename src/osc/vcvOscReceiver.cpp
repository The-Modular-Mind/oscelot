// copyright (c) openFrameworks team 2010-2017
// copyright (c) Damian Stewart 2007-2009
#include "vcvOscReceiver.h"

//--------------------------------------------------------------
vcvOscReceiver::~vcvOscReceiver()
{
	stop();
}

//--------------------------------------------------------------
bool vcvOscReceiver::setup(int port)
{
	if (listenSocket)
	{ // already running
		stop();
	}
	this->port = port;
	return start();
}

//--------------------------------------------------------------
bool vcvOscReceiver::start()
{
	if (listenSocket)
	{
		return true;
	}

	// manually set larger buffer size instead of oscpack per-message size
	// if(UdpSocket::GetUdpBufferSize() == 0){
	//    UdpSocket::SetUdpBufferSize(65535);
	// }

	// create socket
	UdpListeningReceiveSocket *socket = nullptr;
	try
	{
		IpEndpointName name(IpEndpointName::ANY_ADDRESS, port);
		socket = new UdpListeningReceiveSocket(name, this);
		auto deleter = [](UdpListeningReceiveSocket *socket)
		{
			// tell the socket to shutdown
			socket->Break();
			delete socket;
		};
		auto newPtr = std::unique_ptr<UdpListeningReceiveSocket, decltype(deleter)>(socket, deleter);
		listenSocket = std::move(newPtr);
	}
	catch (std::exception &e)
	{
		WARN("vcvOscReceiver couldn't create receiver on port ", e.what());
		if (socket != nullptr)
		{
			delete socket;
			socket = nullptr;
		}
		return false;
	}

	listenThread=std::thread(listenerProcess, this);

	// detach thread so we don't have to wait on it before creating a new socket
	// or on destruction, the custom deleter for the socket unique_ptr already
	// does the right thing
	listenThread.detach();

	return true;
}

void vcvOscReceiver::listenerProcess()
{
	while (listenSocket)
	{
		try
		{
			listenSocket->Run();
		}
		catch (std::exception &e)
		{
			WARN("vcvOscReceiver", e.what());
		}
	}
}

//--------------------------------------------------------------
void vcvOscReceiver::stop()
{
	listenSocket.reset();
}

// PROTECTED
//--------------------------------------------------------------
void vcvOscReceiver::ProcessMessage(const osc::ReceivedMessage &m, const IpEndpointName &remoteEndpoint)
{
	// convert the message to an vcvOscMessage
	vcvOscMessage msg;

	// set the address
	msg.setAddress(m.AddressPattern());

	// set the sender ip/host
	char endpointHost[IpEndpointName::ADDRESS_STRING_LENGTH];
	remoteEndpoint.AddressAsString(endpointHost);
	msg.setRemoteEndpoint(endpointHost, remoteEndpoint.port);

	// transfer the arguments
	for (osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin(); arg != m.ArgumentsEnd(); ++arg)
	{
		if (arg->IsInt32())
		{
			msg.addIntArg(arg->AsInt32Unchecked());
		}
		else if (arg->IsFloat())
		{
			msg.addFloatArg(arg->AsFloatUnchecked());
		}
		else if (arg->IsString())
		{
			msg.addStringArg(arg->AsStringUnchecked());
		}
		else
		{
			// ofLogError("vcvOscReceiver") << "ProcessMessage(): argument in message "
			// 	<< m.AddressPattern() << " is an unknown type "
			// 	<< (int) arg->TypeTag() << " '" << (char) arg->TypeTag() << "'";
			WARN("vcvOscReceiver ProcessMessage(): argument in message %s %s", m.AddressPattern(), " is an unknown type ", (char)arg->TypeTag());
			break;
		}
	}

	onMessage(msg);
}

void vcvOscReceiver::onMessage(vcvOscMessage message)
{
	// std::unique_lock<std::mutex> lock(queueMutex);

	// Push to queue
	if ((int)queue.size() < queueMaxSize)
		queue.push(message);
}

bool vcvOscReceiver::shift(vcvOscMessage *message)
{
	// std::unique_lock<std::mutex> lock(queueMutex);
	if (!message)
		return false;
	if (!queue.empty())
	{
		*message = queue.front();
		queue.pop();
		return true;
	}
	return false;
}