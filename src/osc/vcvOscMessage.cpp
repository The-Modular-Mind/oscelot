// copyright (c) openFrameworks team 2010-2017
// copyright (c) Damian Stewart 2007-2009
#include "vcvOscMessage.h"

//--------------------------------------------------------------
vcvOscMessage::vcvOscMessage() : remoteHost(""), remotePort(0) {}

//--------------------------------------------------------------
vcvOscMessage::~vcvOscMessage()
{
	clear();
}
vcvOscMessage::vcvOscMessage(const vcvOscMessage &other){
	copy(other);
}

//--------------------------------------------------------------
vcvOscMessage& vcvOscMessage::operator=(const vcvOscMessage &other){
	return copy(other);
}

//--------------------------------------------------------------
vcvOscMessage& vcvOscMessage::copy(const vcvOscMessage &other){
	if(this == &other) return *this;
	clear();

	// copy address & remote info
	address = other.address;
	remoteHost = other.remoteHost;
	remotePort = other.remotePort;

	// copy arguments
    for(std::size_t i = 0; i < other.args.size(); ++i){
		switch(other.getArgType(i)){
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
				FATAL("vcvOscMessage copy(): bad argument type ", other.getArgType(i) ,(char) other.getArgType(i));	
				break;
		}
	}
	
	return *this;
}

//--------------------------------------------------------------
void vcvOscMessage::clear()
{
	address = "";
	remoteHost = "";
	remotePort = 0;
	for (unsigned int i = 0; i < args.size(); ++i)
	{
		delete args[i];
	}
	args.clear();
}

//--------------------------------------------------------------
void vcvOscMessage::setAddress(const std::string &address)
{
	this->address = address;
}

//--------------------------------------------------------------
std::string vcvOscMessage::getAddress() const
{
	return address;
}

//--------------------------------------------------------------
std::string vcvOscMessage::getRemoteHost() const
{
	return remoteHost;
}

//--------------------------------------------------------------
int vcvOscMessage::getRemotePort() const
{
	return remotePort;
}

// get methods
//--------------------------------------------------------------
std::size_t vcvOscMessage::getNumArgs() const
{
	return args.size();
}

//--------------------------------------------------------------
osc::TypeTagValues vcvOscMessage::getArgType(std::size_t index) const{
	if(index >= args.size()) {
		// ofLogError("vcvOscMessage") << "getArgType(): index "
		//                             << index << " out of bounds";
		return osc::NIL_TYPE_TAG;
	}
	else{
		return args[index]->getType();
	}
}

//--------------------------------------------------------------
std::int32_t vcvOscMessage::getArgAsInt(std::size_t index) const
{
	return ((vcvOscArgInt32 *)args[index])->get();
}

//--------------------------------------------------------------
float vcvOscMessage::getArgAsFloat(std::size_t index) const
{
	return ((vcvOscArgFloat *)args[index])->get();
}

//--------------------------------------------------------------
std::string vcvOscMessage::getArgAsString(std::size_t index) const
{
	return ((vcvOscArgString *)args[index])->get();
}

// set methods
//--------------------------------------------------------------
void vcvOscMessage::addIntArg(std::int32_t argument)
{
	args.push_back(new vcvOscArgInt32(argument));
}

//--------------------------------------------------------------
void vcvOscMessage::addFloatArg(float argument)
{
	args.push_back(new vcvOscArgFloat(argument));
}

//--------------------------------------------------------------
void vcvOscMessage::addStringArg(const std::string &argument)
{
	args.push_back(new vcvOscArgString(argument));
}

// util
//--------------------------------------------------------------
void vcvOscMessage::setRemoteEndpoint(const std::string &host, int port)
{
	remoteHost = host;
	remotePort = port;
}
