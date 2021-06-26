// copyright (c) openFrameworks team 2010-2017
// copyright (c) Damian Stewart 2007-2009
#pragma once
#include "../plugin.hpp"
#include <functional>
// #include <mutex>

#include "vcvOscMessage.h"
#include "../../../oscpack/osc/OscTypes.h"
#include "../../../oscpack/osc/OscPacketListener.h"
#include "../../../oscpack/ip/UdpSocket.h"

/// \class vcvOscReceiver
/// \brief OSC message receiver which listens on a network port
class vcvOscReceiver : public osc::OscPacketListener
{
public:
	vcvOscReceiver(){};
	~vcvOscReceiver();
	int port;
	int queueMaxSize = 8192;
	std::queue<vcvOscMessage> queue;
	void onMessage(vcvOscMessage message);
	/** If a Message is available, writes `message` and return true */
	bool shift(vcvOscMessage *message);
	/// set up the receiver with the port to listen for messages on
	/// and start listening
	///
	/// multiple receivers can share the same port if port reuse is
	/// enabled (true by default)
	///
	/// \return true if listening started
	bool setup(int port);

	/// start listening manually using the current settings
	///
	/// this is not required if you called setup(port)
	/// or setup(settings) with start set to true
	///
	/// \return true if listening started or was already running
	bool start();
	void listenerProcess();

	/// stop listening, does not clear port value
	void stop();

protected:
	/// process an incoming osc message and add it to the queue
	virtual void ProcessMessage(const osc::ReceivedMessage &m, const IpEndpointName &remoteEndpoint) override;

private:
	/// socket to listen on, unique for each port
	/// shared between objects if allowReuse is true
	std::unique_ptr<UdpListeningReceiveSocket, std::function<void(UdpListeningReceiveSocket *)>> listenSocket;
	// std::mutex queueMutex;

	std::thread listenThread; ///< listener thread
							  // ofThreadChannel<vcvOscMessage> messagesChannel; ///< message passing thread channel
};
