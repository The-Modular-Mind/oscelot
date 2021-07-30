#pragma once
#include "plugin.hpp"

#include "OscArgs.hpp"

/// \class OscMessage
/// \brief an OSC message with address and arguments
class OscMessage {
   public:
	OscMessage() : remoteHost(""), remotePort(0) {}

	~OscMessage() { clear(); }

	/// clear this message
	OscMessage(const OscMessage &other) { copy(other); }

	/// operator=
	OscMessage &operator=(const OscMessage &other) { return copy(other); }

	/// copy constructor
	OscMessage &copy(const OscMessage &other) {
		if (this == &other) return *this;
		clear();

		// copy address & remote info
		address = other.address;
		remoteHost = other.remoteHost;
		remotePort = other.remotePort;

		// copy arguments
		for (std::size_t i = 0; i < other.args.size(); ++i) {
			switch (other.getArgType(i)) {
			case osc::INT32_TYPE_TAG:
				args.push_back(new vcvOscArgInt32(other.getArgAsInt(i)));
				break;
			case osc::FLOAT_TYPE_TAG:
				args.push_back(new vcvOscArgFloat(other.getArgAsFloat(i)));
				break;
			case osc::STRING_TYPE_TAG:
				args.push_back(new vcvOscArgString(other.getArgAsString(i)));
				break;
			default:
				FATAL("OscMessage copy(): bad argument type ", other.getArgType(i), (char)other.getArgType(i));
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

	/// \section Argument Getters
	///
	/// get the argument with the given index as an int, float, string, etc
	///
	/// some types can be automatically converted to a requested type,
	/// (ie. int to float) however it is best to ensure that the type matches
	/// what you are requesting:
	///
	///     int i = 0;
	///     if(message.getArgType(index) == VCVOSC_TYPE_INT32) {
	///         i = message.getArgAsInt32(index);
	///     }
	///
	/// or use the type tag char:
	///
	///		int i = 0;
	///     if(message.getArgTypeName(index) == "i") {
	///         i = message.getArgAsInt32(index);
	///     }
	///
	/// you can also check against the type string for all arguments:
	///
	///     int i = 0; float f = 0.0; std::string s = "";
	///     if(message.getTypeString() == "ifs") {
	///         i = message.getArgAsInt32(0);
	///         f = message.getArgAsFloat(1);
	///         s = message.getArgAsString(2);
	///     }
	///
	/// see vcvOscArg.h for argument type tag char values

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

	/// get argument as an integer, converts numeric types automatically
	/// prints a warning when converting higher precision types
	/// \param index The index of the queried item.
	/// \return given argument value as a 32-bit int
	std::int32_t getArgAsInt(std::size_t index) const { return ((vcvOscArgInt32 *)args[index])->get(); }

	/// get argument as a float, converts numeric types automatically
	/// prints a warning when converting higher precision types
	/// \param index The index of the queried item.
	/// \return given argument value as a float
	float getArgAsFloat(std::size_t index) const { return ((vcvOscArgFloat *)args[index])->get(); }

	/// get argument as a string, converts numeric types with a warning
	/// \param index The index of the queried item.
	/// \return given argument value as a string
	std::string getArgAsString(std::size_t index) const { return ((vcvOscArgString *)args[index])->get(); }

	void addIntArg(std::int32_t argument) { args.push_back(new vcvOscArgInt32(argument)); }

	void addFloatArg(float argument) { args.push_back(new vcvOscArgFloat(argument)); }

	void addStringArg(const std::string &argument) { args.push_back(new vcvOscArgString(argument)); }

	/// set host and port of the remote endpoint,
	void setRemoteEndpoint(const std::string &host, int port) {
		remoteHost = host;
		remotePort = port;
	}

   private:
	std::string address;            ///< OSC address, must start with a /
	std::vector<vcvOscArg *> args;  ///< current arguments

	std::string remoteHost;  ///< host name/ip the message was sent from
	int remotePort;          ///< port the message was sent from
};
