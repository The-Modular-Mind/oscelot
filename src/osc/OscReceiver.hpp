#pragma once
#include <functional>
#include <queue>
#include "oscpack/osc/OscPacketListener.h"

namespace TheModularMind {

struct OscReceiver : public osc::OscPacketListener {
   public:
	int port;

	OscReceiver() {}

	~OscReceiver() { stop(); }

	bool start(int port) {
		if (listenSocket) {
			stop();
		}
		this->port = port;

		UdpListeningReceiveSocket *socket = nullptr;
		try {
			IpEndpointName name(IpEndpointName::ANY_ADDRESS, port);
			socket = new UdpListeningReceiveSocket(name, this);

			// Socket deleter
			auto deleter = [](UdpListeningReceiveSocket *socket) {
				socket->Break();
				delete socket;
			};

			listenSocket = std::unique_ptr<UdpListeningReceiveSocket, decltype(deleter)>(socket, deleter);

		} catch (std::exception &e) {
			FATAL("OscReceiver couldn't create receiver on port %i, %s", port, e.what());
			if (socket != nullptr) {
				delete socket;
				socket = nullptr;
			}
			return false;
		}

		listenThread = std::thread([this] { this->listenerProcess(); });
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

	void stop() { listenSocket.reset(); }

	bool shift(OscMessage *message) {
		if (!message) return false;
		if (!queue.empty()) {
			*message = queue.front();
			queue.pop();
			return true;
		}
		return false;
	}

   protected:
	/// process incoming OSC message and add it to the queue
	virtual void ProcessMessage(const osc::ReceivedMessage &receivedMessage, const IpEndpointName &remoteEndpoint) override {
		OscMessage msg;
		char endpointHost[IpEndpointName::ADDRESS_STRING_LENGTH];

		remoteEndpoint.AddressAsString(endpointHost);
		msg.setAddress(receivedMessage.AddressPattern());
		msg.setRemoteEndpoint(endpointHost, remoteEndpoint.port);

		for (auto arg = receivedMessage.ArgumentsBegin(); arg != receivedMessage.ArgumentsEnd(); ++arg) {
			if (arg->IsInt32()) {
				msg.addIntArg(arg->AsInt32Unchecked());
			} else if (arg->IsFloat()) {
				msg.addFloatArg(arg->AsFloatUnchecked());
			} else if (arg->IsString()) {
				msg.addStringArg(arg->AsStringUnchecked());
			} else {
				FATAL("OscReceiver ProcessMessage(): argument in message %s is an unknown type %d", receivedMessage.AddressPattern(), arg->TypeTag());
				break;
			}
		}
		queue.push(msg);
	}

   private:
	std::unique_ptr<UdpListeningReceiveSocket, std::function<void(UdpListeningReceiveSocket *)>> listenSocket;
	std::queue<OscMessage> queue;
	std::thread listenThread;
};
}  // namespace TheModularMind