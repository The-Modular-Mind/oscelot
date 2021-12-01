#pragma once
#include "OscArgs.hpp"

namespace TheModularMind {

class OscMessage {
   public:
	OscMessage() : remoteHost(""), remotePort(0) {}

	OscMessage(const OscMessage &oscMessage) { copy(oscMessage); }

	~OscMessage() { clear(); }

	OscMessage &operator=(const OscMessage &oscMessage) { return copy(oscMessage); }

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
				FATAL("OscMessage copy(): bad/unimplemented argument type %i", oscMessage.getArgType(i));
				break;
			}
		}
		return *this;
	}

	void clear() {
		address = "";
		remoteHost = "";
		remotePort = 0;
		for (unsigned int i = 0; i < args.size(); ++i) {
			delete args[i];
		}
		args.clear();
	}

	void setRemoteEndpoint(const std::string &host, int port) {
		remoteHost = host;
		remotePort = port;
	}

	osc::TypeTagValues getArgType(std::size_t index) const {
		if (index >= args.size()) {
			FATAL("OscMessage.getArgType(): index %lld out of bounds", index);
			return osc::NIL_TYPE_TAG;
		} else {
			return args[index]->getType();
		}
	}

	void setAddress(const std::string &address) { this->address = address; }
	std::string getAddress() const { return address; }
	std::string getRemoteHost() const { return remoteHost; }
	int getRemotePort() const { return remotePort; }
	std::size_t getNumArgs() const { return args.size(); }

	std::int32_t getArgAsInt(std::size_t index) const { return ((OscArgInt32 *)args[index])->get(); }
	float getArgAsFloat(std::size_t index) const { return ((OscArgFloat *)args[index])->get(); }
	std::string getArgAsString(std::size_t index) const { return ((OscArgString *)args[index])->get(); }

	void addOscArg(OscArg* argument) { args.push_back(argument); }
	void addIntArg(std::int32_t argument) { args.push_back(new OscArgInt32(argument)); }
	void addFloatArg(float argument) { args.push_back(new OscArgFloat(argument)); }
	void addStringArg(const std::string &argument) { args.push_back(new OscArgString(argument)); }

   private:
	std::string address;
	std::vector<OscArg *> args;
	std::string remoteHost;
	int remotePort;
};
}  // namespace TheModularMind