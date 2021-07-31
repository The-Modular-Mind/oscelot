#pragma once
#include <functional>
#include "plugin.hpp"

// #include <mutex>
#include "./oscpack/osc/OscPacketListener.h"
#include "OscMessage.hpp"

class OscReceiver : public osc::OscPacketListener {
   public:
	int port;
	int queueMaxSize = 8192;
	std::queue<OscMessage> queue;

	~OscReceiver() { stop(); }

	/// set up the receiver with the port to listen for messages on
	/// and start listening
	///
	/// multiple receivers can share the same port if port reuse is
	/// enabled (true by default)
	///
	/// \return true if listening started
	bool setup(int port) {
		if (listenSocket) {
			stop();
		}
		this->port = port;
		return start();
	}

	/// start listening manually using the current settings
	///
	/// this is not required if you called setup(port)
	/// or setup(settings) with start set to true
	///
	/// \return true if listening started or was already running
	bool start() {
		if (listenSocket) {
			return true;
		}

		UdpListeningReceiveSocket *socket = nullptr;
		try {
			IpEndpointName name(IpEndpointName::ANY_ADDRESS, port);
			socket = new UdpListeningReceiveSocket(name, this);
			auto deleter = [](UdpListeningReceiveSocket *socket) {
				// tell the socket to shutdown
				socket->Break();
				delete socket;
			};
			auto newPtr = std::unique_ptr<UdpListeningReceiveSocket, decltype(deleter)>(socket, deleter);
			listenSocket = std::move(newPtr);
		} catch (std::exception &e) {
			FATAL("oscReceiver couldn't create receiver on port %i, %s", port, e.what());
			if (socket != nullptr) {
				delete socket;
				socket = nullptr;
			}
			return false;
		}

		listenThread = std::thread(listenerProcess, this);

		// detach thread so we don't have to wait on it before creating a new socket
		// or on destruction, the custom deleter for the socket unique_ptr already
		// does the right thing
		listenThread.detach();

		return true;
	}

	void listenerProcess() {
		while (listenSocket) {
			try {
				listenSocket->Run();
			} catch (std::exception &e) {
				FATAL("oscReceiver error: %s", e.what());
			}
		}
	}

	/// stop listening
	void stop() { listenSocket.reset(); }

		void onMessage(OscMessage message) {
		// std::unique_lock<std::mutex> lock(queueMutex);

		// Push to queue
		if ((int)queue.size() < queueMaxSize) queue.push(message);
	}

	/** If a Message is available, writes `message` and return true */
	bool shift(OscMessage *message) {
		// std::unique_lock<std::mutex> lock(queueMutex);
		if (!message) return false;
		if (!queue.empty()) {
			*message = queue.front();
			queue.pop();
			return true;
		}
		return false;
	}

   protected:
	/// process an incoming osc message and add it to the queue
	virtual void ProcessMessage(const osc::ReceivedMessage &m, const IpEndpointName &remoteEndpoint) override {
		// convert the message to an OscMessage
		OscMessage msg;

		// set the address
		msg.setAddress(m.AddressPattern());

		// set the sender ip/host
		char endpointHost[IpEndpointName::ADDRESS_STRING_LENGTH];
		remoteEndpoint.AddressAsString(endpointHost);
		msg.setRemoteEndpoint(endpointHost, remoteEndpoint.port);

		// transfer the arguments
		for (osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin(); arg != m.ArgumentsEnd(); ++arg) {
			if (arg->IsInt32()) {
				msg.addIntArg(arg->AsInt32Unchecked());
			} else if (arg->IsFloat()) {
				msg.addFloatArg(arg->AsFloatUnchecked());
			} else if (arg->IsString()) {
				msg.addStringArg(arg->AsStringUnchecked());
			} else {
				FATAL("oscReceiver ProcessMessage(): argument in message %s %s", m.AddressPattern(),
				      " is an unknown type ", (char)arg->TypeTag());
				break;
			}
		}

		onMessage(msg);
	}

   private:
	/// socket to listen on, unique for each port
	/// shared between objects if allowReuse is true
	std::unique_ptr<UdpListeningReceiveSocket, std::function<void(UdpListeningReceiveSocket *)>> listenSocket;
	// std::mutex queueMutex;

	std::thread listenThread;  ///< listener thread
};