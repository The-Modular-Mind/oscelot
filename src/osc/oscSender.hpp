
#pragma once

#include "../../../oscpack/ip/UdpSocket.h"
#include "../../../oscpack/osc/OscOutboundPacketStream.h"
#include "../../../oscpack/osc/OscTypes.h"
#include "OscBundle.hpp"

/// \class vcvOscSender
/// \brief OSC message sender which sends to a specific host & port
class OscSender {
   public:
	OscSender() {}

	~OscSender() { clear(); }
	std::string host;
	int port = 0;

	/// set up the sender with the destination host name/ip and port
	/// \return true on success
	bool setup(std::string &host, int port) {
		this->host = host;
		this->port = port;

		// check for empty host
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
			std::string what = e.what();
			FATAL("OscSender couldn't create sender to %s:%i because of: %s", host.c_str(), port, what.c_str());
			if (socket != nullptr) {
				delete socket;
				socket = nullptr;
			}
			sendSocket.reset();
			return false;
		}
		return true;
	}

	/// clear the sender, does not clear host or port values
	void clear() { sendSocket.reset(); }

	bool isSending() { return !!sendSocket; }

	/// send the given bundle
	void sendBundle(const OscBundle &bundle) {
		if (!sendSocket) {
			FATAL("OscSender trying to send with empty socket");
			return;
		}

		// setting this much larger as it gets trimmed down to the size its using before being sent.
		// TODO: much better if we could make this dynamic? Maybe have OscBundle return its size?
		static const int OUTPUT_BUFFER_SIZE = 327680;
		char buffer[OUTPUT_BUFFER_SIZE];
		osc::OutboundPacketStream p(buffer, OUTPUT_BUFFER_SIZE);

		// serialise the bundle and send
		appendBundle(bundle, p);
		sendSocket->Send(p.Data(), p.Size());
	}

	/// send the given message
	/// if wrapInBundle is true (default), message sent in a timetagged bundle
	void sendMessage(const vcvOscMessage &message, bool wrapInBundle = true) {
		if (!sendSocket) {
			FATAL("OscSender trying to send with empty socket");
			return;
		}

		static const int OUTPUT_BUFFER_SIZE = 327680;
		char buffer[OUTPUT_BUFFER_SIZE];
		osc::OutboundPacketStream outputStream(buffer, OUTPUT_BUFFER_SIZE);

		if (wrapInBundle) {
			outputStream << osc::BeginBundleImmediate;
		}
		appendMessage(message, outputStream);
		if (wrapInBundle) {
			outputStream << osc::EndBundle;
		}
		sendSocket->Send(outputStream.Data(), outputStream.Size());
	}

   private:
	void appendBundle(const OscBundle &bundle, osc::OutboundPacketStream &p) {
		// recursively serialise the bundle
		p << osc::BeginBundleImmediate;
		for (int i = 0; i < bundle.getBundleCount(); i++) {
			appendBundle(bundle.getBundleAt(i), p);
		}
		for (int i = 0; i < bundle.getMessageCount(); i++) {
			appendMessage(bundle.getMessageAt(i), p);
		}
		p << osc::EndBundle;
	}

	void appendMessage(const vcvOscMessage &message, osc::OutboundPacketStream &p) {
		p << osc::BeginMessage(message.getAddress().c_str());
		for (size_t i = 0; i < message.getNumArgs(); ++i) {
			switch (message.getArgType(i)) {
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
				FATAL("OscSender.appendMessage(), Unimplemented type?: %i, %s", (int)message.getArgType(i),
				      (char)message.getArgType(i));
				break;
			}
		}
		p << osc::EndMessage;
	}

	std::unique_ptr<UdpTransmitSocket> sendSocket;  ///< sender socket
};