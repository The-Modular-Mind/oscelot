// copyright (c) openFrameworks team 2010-2017
// copyright (c) Damian Stewart 2007-2009
#pragma once

#include "vcvOscMessage.h"

/// \class vcvOscBundle
/// \brief an OSC bundle of vcvOscMessages and/or other vcvOscBundles
class vcvOscBundle
{
public:
	vcvOscBundle() {}
	vcvOscBundle(const vcvOscBundle &other);
	vcvOscBundle &operator=(const vcvOscBundle &other);
	/// for operator= and copy constructor
	vcvOscBundle &copy(const vcvOscBundle &other);

	/// clear bundle & message contents
	void clear();

	/// add another bundle to the bundle
	void addBundle(const vcvOscBundle &element);

	/// add a message to the bundle
	void addMessage(const vcvOscMessage &message);

	/// \return the current bundle count
	int getBundleCount() const;

	/// \return the current message count
	int getMessageCount() const;

	/// \return the bundle at the given index
	const vcvOscBundle &getBundleAt(std::size_t i) const;

	/// \return the bundle at the given index
	vcvOscBundle &getBundleAt(std::size_t i);

	/// \return the message at the given index
	const vcvOscMessage &getMessageAt(std::size_t i) const;

	/// \return the message at the given index
	vcvOscMessage &getMessageAt(std::size_t i);

	/// output stream operator for string conversion and printing
	/// \return number of messages & bundles
	friend std::ostream &operator<<(std::ostream &os, const vcvOscBundle &sender);

private:
	std::vector<vcvOscMessage> messages; ///< bundled messages
	std::vector<vcvOscBundle> bundles;	 ///< bundled bundles
};
