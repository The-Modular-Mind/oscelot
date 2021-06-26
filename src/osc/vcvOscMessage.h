// copyright (c) openFrameworks team 2010-2017
// copyright (c) Damian Stewart 2007-2009
#pragma once
#include "vcvOscArg.h"

/// \class vcvOscMessage
/// \brief an OSC message with address and arguments
class vcvOscMessage
{
public:
	vcvOscMessage();
	~vcvOscMessage();
	/// clear this message
	void clear();
	vcvOscMessage(const vcvOscMessage &other);
	vcvOscMessage &operator=(const vcvOscMessage &other);
	/// for operator= and copy constructor
	vcvOscMessage &copy(const vcvOscMessage &other);
	/// set the message address, must start with a /
	void setAddress(const std::string &address);

	/// \return the OSC address
	std::string getAddress() const;

	/// \return the remote host name/ip (deprecated)
	// OF_DEPRECATED_MSG("Use getRemoteHost() instead", std::string getRemoteIp() const);

	/// \return the remote host name/ip or "" if not set
	std::string getRemoteHost() const;

	/// \return the remote port or 0 if not set
	int getRemotePort() const;

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
	std::size_t getNumArgs() const;

	/// \param index The index of the queried item.
	/// \return argument type code for a given index
	osc::TypeTagValues getArgType(std::size_t index) const;

	/// get argument as an integer, converts numeric types automatically
	/// prints a warning when converting higher precision types
	/// \param index The index of the queried item.
	/// \return given argument value as a 32-bit int
	std::int32_t getArgAsInt(std::size_t index) const;

	/// get argument as a float, converts numeric types automatically
	/// prints a warning when converting higher precision types
	/// \param index The index of the queried item.
	/// \return given argument value as a float
	float getArgAsFloat(std::size_t index) const;

	/// get argument as a string, converts numeric types with a warning
	/// \param index The index of the queried item.
	/// \return given argument value as a string
	std::string getArgAsString(std::size_t index) const;

	/// \section Argument Setters

	/// add a 32-bit integer
	void addIntArg(std::int32_t argument);

	/// add a 32-bit float
	void addFloatArg(float argument);

	/// add a string
	void addStringArg(const std::string &argument);

	/// set host and port of the remote endpoint,
	/// this is mainly used by vcvOscReceiver
	void setRemoteEndpoint(const std::string &host, int port);

private:
	std::string address;		   ///< OSC address, must start with a /
	std::vector<vcvOscArg *> args; ///< current arguments

	std::string remoteHost; ///< host name/ip the message was sent from
	int remotePort;			///< port the message was sent from
};
