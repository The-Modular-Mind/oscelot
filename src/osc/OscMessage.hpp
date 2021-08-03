#pragma once
#include "plugin.hpp"

#include "OscArgs.hpp"

class OscMessage {
   public:
	OscMessage() : remoteHost(""), remotePort(0) {}

	~OscMessage() { clear(); }

	/// clear this message
	OscMessage(const OscMessage &oscMessage) { copy(oscMessage); }

	/// operator=
	OscMessage &operator=(const OscMessage &oscMessage) { return copy(oscMessage); }

	/// copy constructor
	OscMessage &copy(const OscMessage &oscMessage) {
		if (this == &oscMessage) return *this;
		clear();

		address = oscMessage.address;
		remoteHost = oscMessage.remoteHost;
		remotePort = oscMessage.remotePort;

		for (std::size_t i = 0; i < oscMessage.args.size(); ++i) {
			switch (oscMessage.getArgType(i)) {
			case osc::INT32_TYPE_TAG:
				args.push_back(new OscArgInt32(oscMessage.getArgAsInt(i)));
				break;
			case osc::FLOAT_TYPE_TAG:
				args.push_back(new OscArgFloat(oscMessage.getArgAsFloat(i)));
				break;
			case osc::STRING_TYPE_TAG:
				args.push_back(new OscArgString(oscMessage.getArgAsString(i)));
				break;
			default:
				FATAL("OscMessage copy(): bad argument type ", oscMessage.getArgType(i), (char)oscMessage.getArgType(i));
				break;
			}
		}

		return *this;
	}

	/// clear this message
	void clear() {
		address = "";
		remoteHost = "";
		remotePort = 0;
		for (unsigned int i = 0; i < args.size(); ++i) {
			delete args[i];
		}
		args.clear();
	}

	/// set the OSC address
	void setAddress(const std::string &address) { this->address = address; }

	/// \return the OSC address
	std::string getAddress() const { return address; }

	/// \return the remote host name/ip
	std::string getRemoteHost() const { return remoteHost; }

	/// \return the remote port
	int getRemotePort() const { return remotePort; }

	/// \return number of arguments
	std::size_t getNumArgs() const { return args.size(); }

	/// \param index The index of the queried item.
	/// \return argument type code for a given index
	osc::TypeTagValues getArgType(std::size_t index) const {
		if (index >= args.size()) {
			FATAL("OscMessage.getArgType(): index %i out of bounds", index);
			return osc::NIL_TYPE_TAG;
		} else {
			return args[index]->getType();
		}
	}

	/// \param index The index of the queried item.
	/// \return given argument value as a 32-bit int
	std::int32_t getArgAsInt(std::size_t index) const { return ((OscArgInt32 *)args[index])->get(); }

	/// \param index The index of the queried item.
	/// \return given argument value as a float
	float getArgAsFloat(std::size_t index) const { return ((OscArgFloat *)args[index])->get(); }

	/// \param index The index of the queried item.
	/// \return given argument value as a string
	std::string getArgAsString(std::size_t index) const { return ((OscArgString *)args[index])->get(); }

	void addIntArg(std::int32_t argument) { args.push_back(new OscArgInt32(argument)); }

	void addFloatArg(float argument) { args.push_back(new OscArgFloat(argument)); }

	void addStringArg(const std::string &argument) { args.push_back(new OscArgString(argument)); }

	/// set host and port of the remote endpoint,
	void setRemoteEndpoint(const std::string &host, int port) {
		remoteHost = host;
		remotePort = port;
	}

   private:
	std::string address;
	std::vector<OscArg *> args;
	std::string remoteHost;  ///< host name/ip the message was sent from
	int remotePort;          ///< port the message was sent from
};
