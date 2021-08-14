#pragma once
#include "OscBundle.hpp"
#include "oscpack/ip/UdpSocket.h"
#include "oscpack/osc/OscOutboundPacketStream.h"
#include "plugin.hpp"

namespace TheModularMind {

class OscSender {
   public:
	std::string host;
	int port = 0;

	OscSender() {}

	~OscSender() { clear(); }

	/// set up the sender with the destination host name/ip and port
	/// \return true on success
	bool setup(std::string &host, int port) {
		this->host = host;
		this->port = port;
		if (host == "") {
			host = "localhost";
		}

		// create socket
		UdpTransmitSocket *socket = nullptr;
		try {
			IpEndpointName name = IpEndpointName(host.c_str(), port);
			if (!name.address) {
				FATAL("Bad hostname: %s", host.c_str());
				return false;
			}
			socket = new UdpTransmitSocket(name);
			sendSocket.reset(socket);

		} catch (std::exception &e) {
			FATAL("OscSender couldn't create sender to %s:%i because of: %s", host.c_str(), port, e.what());
			if (socket != nullptr) {
				delete socket;
				socket = nullptr;
			}
			sendSocket.reset();
			return false;
		}
		return true;
	}

	void clear() { sendSocket.reset(); }

	bool isSending() { return !!sendSocket; }

	void sendBundle(const OscBundle &bundle) {
		if (!sendSocket) {
			FATAL("OscSender trying to send with empty socket");
			return;
		}

		static const int OUTPUT_BUFFER_SIZE = 327680;
		char buffer[OUTPUT_BUFFER_SIZE];
		osc::OutboundPacketStream outputStream(buffer, OUTPUT_BUFFER_SIZE);
		appendBundle(bundle, outputStream);
		sendSocket->Send(outputStream.Data(), outputStream.Size());
	}

	void sendMessage(const OscMessage &message) {
		if (!sendSocket) {
			FATAL("OscSender trying to send with empty socket");
			return;
		}

		static const int OUTPUT_BUFFER_SIZE = 327680;
		char buffer[OUTPUT_BUFFER_SIZE];
		osc::OutboundPacketStream outputStream(buffer, OUTPUT_BUFFER_SIZE);
		appendMessage(message, outputStream);
		sendSocket->Send(outputStream.Data(), outputStream.Size());
	}

   private:
	std::unique_ptr<UdpTransmitSocket> sendSocket;

	void appendBundle(const OscBundle &bundle, osc::OutboundPacketStream &outputStream) {
		outputStream << osc::BeginBundleImmediate;
		for (int i = 0; i < bundle.getBundleCount(); i++) {
			appendBundle(bundle.getBundleAt(i), outputStream);
		}
		for (int i = 0; i < bundle.getMessageCount(); i++) {
			appendMessage(bundle.getMessageAt(i), outputStream);
		}
		outputStream << osc::EndBundle;
	}

	void appendMessage(const OscMessage &message, osc::OutboundPacketStream &outputStream) {
		outputStream << osc::BeginMessage(message.getAddress().c_str());
		for (size_t i = 0; i < message.getNumArgs(); ++i) {
			switch (message.getArgType(i)) {
			case osc::INT32_TYPE_TAG:
				outputStream << message.getArgAsInt(i);
				break;
			case osc::FLOAT_TYPE_TAG:
				outputStream << message.getArgAsFloat(i);
				break;
			case osc::STRING_TYPE_TAG:
				outputStream << message.getArgAsString(i).c_str();
				break;
			default:
				FATAL("OscSender.appendMessage(), Unimplemented type?: %i, %s", (int)message.getArgType(i), (char)message.getArgType(i));
				break;
			}
		}
		outputStream << osc::EndMessage;
	}
};
}  // namespace TheModularMind