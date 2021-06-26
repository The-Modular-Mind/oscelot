#pragma once

#include "../../../oscpack/osc/OscTypes.h"
#include "../../../oscpack/osc/OscOutboundPacketStream.h"
#include "../../../oscpack/ip/UdpSocket.h"

#include "vcvOscBundle.h"

/// \class vcvOscSender
/// \brief OSC message sender which sends to a specific host & port
class vcvOscSender
{
public:
	vcvOscSender() {}
	~vcvOscSender();
	std::string host; ///< destination host name/ip
	int port = 0;

	/// set up the sender with the destination host name/ip and port
	/// \return true on success
	bool setup(std::string &host, int port);

	/// clear the sender, does not clear host or port values
	void clear();

	/// send the given message
	/// if wrapInBundle is true (default), message sent in a timetagged bundle
	void sendMessage(const vcvOscMessage &message, bool wrapInBundle = true);

	/// send the given bundle
	void sendBundle(const vcvOscBundle &bundle);

private:
	// helper methods for constructing messages
	void appendBundle(const vcvOscBundle &bundle, osc::OutboundPacketStream &p);
	void appendMessage(const vcvOscMessage &message, osc::OutboundPacketStream &p);

	std::unique_ptr<UdpTransmitSocket> sendSocket; ///< sender socket
};
