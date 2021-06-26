// copyright (c) openFrameworks team 2010-2017
// copyright (c) Damian Stewart 2007-2009
#include "vcvOscBundle.h"

//--------------------------------------------------------------
vcvOscBundle::vcvOscBundle(const vcvOscBundle &other)
{
	copy(other);
}

//--------------------------------------------------------------
vcvOscBundle &vcvOscBundle::operator=(const vcvOscBundle &other)
{
	return copy(other);
}

//--------------------------------------------------------------
vcvOscBundle &vcvOscBundle::copy(const vcvOscBundle &other)
{
	if (this == &other)
		return *this;

	std::copy(other.bundles.begin(),
			  other.bundles.end(),
			  std::back_inserter(bundles));

	std::copy(other.messages.begin(),
			  other.messages.end(),
			  std::back_inserter(messages));

	return *this;
}

//--------------------------------------------------------------
void vcvOscBundle::clear()
{
	bundles.clear();
	messages.clear();
}

//--------------------------------------------------------------
void vcvOscBundle::addBundle(const vcvOscBundle &bundle)
{
	bundles.push_back(bundle);
}

//--------------------------------------------------------------
void vcvOscBundle::addMessage(const vcvOscMessage &message)
{
	messages.push_back(message);
}

//--------------------------------------------------------------
int vcvOscBundle::getBundleCount() const
{
	return bundles.size();
}

//--------------------------------------------------------------
int vcvOscBundle::getMessageCount() const
{
	return messages.size();
}

//--------------------------------------------------------------
const vcvOscBundle &vcvOscBundle::getBundleAt(std::size_t i) const
{
	return bundles[i];
}

//--------------------------------------------------------------
vcvOscBundle &vcvOscBundle::getBundleAt(std::size_t i)
{
	return bundles[i];
}

//--------------------------------------------------------------
const vcvOscMessage &vcvOscBundle::getMessageAt(std::size_t i) const
{
	return messages[i];
}

//--------------------------------------------------------------
vcvOscMessage &vcvOscBundle::getMessageAt(std::size_t i)
{
	return messages[i];
}

// friend functions
//--------------------------------------------------------------
std::ostream &operator<<(std::ostream &os, const vcvOscBundle &bundle)
{
	os << bundle.getMessageCount() << " message(s) "
	   << bundle.getBundleCount() << " bundle(s)";
	return os;
}
