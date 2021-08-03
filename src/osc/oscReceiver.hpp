#pragma once
#include <functional>
#include "plugin.hpp"

#include "./oscpack/osc/OscPacketListener.h"
#include "OscMessage.hpp"

class OscReceiver : public osc::OscPacketListener {
   public:
	int port;
	int queueMaxSize = 8192;
	std::queue<OscMessage> queue;

	OscReceiver() {}

	~OscReceiver() { stop(); }

	/// setup and start the receiver with the port to listen for messages on
	/// \return true if listening started
	bool setup(int port) {
		if (listenSocket) {
			stop();
		}
		this->port = port;
		return start();
	}

	/// start listening manually using the current settings
	/// \return true if listening started or already running
	bool start() {
		if (listenSocket) {
			return true;
		}

		UdpListeningReceiveSocket *socket = nullptr;
		try {
			IpEndpointName name(IpEndpointName::ANY_ADDRESS, port);
			socket = new UdpListeningReceiveSocket(name, this);

			// Socket deleter
			auto deleter = [](UdpListeningReceiveSocket *socket) {
				socket->Break();
				delete socket;
			};
			auto newPtr = std::unique_ptr<UdpListeningReceiveSocket, decltype(deleter)>(socket, deleter);
			listenSocket = std::move(newPtr);
		} catch (std::exception &e) {
			FATAL("OscReceiver couldn't create receiver on port %i, %s", port, e.what());
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
				FATAL("OscReceiver error: %s", e.what());
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
		OscMessage msg;
		msg.setAddress(m.AddressPattern());

		char endpointHost[IpEndpointName::ADDRESS_STRING_LENGTH];
		remoteEndpoint.AddressAsString(endpointHost);
		msg.setRemoteEndpoint(endpointHost, remoteEndpoint.port);

		for (osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin(); arg != m.ArgumentsEnd(); ++arg) {
			if (arg->IsInt32()) {
				msg.addIntArg(arg->AsInt32Unchecked());
			} else if (arg->IsFloat()) {
				msg.addFloatArg(arg->AsFloatUnchecked());
			} else if (arg->IsString()) {
				msg.addStringArg(arg->AsStringUnchecked());
			} else {
				FATAL("OscReceiver ProcessMessage(): argument in message %s %s", m.AddressPattern(),
				      " is an unknown type ", (char)arg->TypeTag());
				break;
			}
		}

		onMessage(msg);
	}

   private:
	/// socket to listen on
	std::unique_ptr<UdpListeningReceiveSocket, std::function<void(UdpListeningReceiveSocket *)>> listenSocket;
	std::thread listenThread;
};