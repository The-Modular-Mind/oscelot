#pragma once

#include "vcvOscMessage.h"

/// \class OscBundle
/// \brief an OSC bundle of vcvOscMessages and/or other OscBundles
class OscBundle {
   public:
	OscBundle() {}

	OscBundle(const OscBundle &other) { copy(other); }

	/// operator=
	OscBundle &operator=(const OscBundle &other) { return copy(other); }

	/// copy constructor
	OscBundle &copy(const OscBundle &other) {
		if (this == &other) return *this;
		std::copy(other.bundles.begin(), other.bundles.end(), std::back_inserter(bundles));
		std::copy(other.messages.begin(), other.messages.end(), std::back_inserter(messages));
		return *this;
	}

	/// clear bundle & message contents
	void clear() {
		bundles.clear();
		messages.clear();
	}

	/// add another bundle to the bundle
	void addBundle(const OscBundle &bundle) { bundles.push_back(bundle); }

	/// add a message to the bundle
	void addMessage(const vcvOscMessage &message) { messages.push_back(message); }

	/// \return the current bundle count
	int getBundleCount() const { return bundles.size(); }

	/// \return the current message count
	int getMessageCount() const { return messages.size(); }

	/// \return the bundle at the given index
	const OscBundle &getBundleAt(std::size_t i) const { return bundles[i]; }

	/// \return the bundle at the given index
	OscBundle &getBundleAt(std::size_t i) { return bundles[i]; }

	/// \return the message at the given index
	const vcvOscMessage &getMessageAt(std::size_t i) const { return messages[i]; }

	/// \return the message at the given index
	vcvOscMessage &getMessageAt(std::size_t i) { return messages[i]; }

	/// output stream operator for string conversion and printing
	/// \return number of messages & bundles
	friend std::ostream &operator<<(std::ostream &os, const OscBundle &bundle) {
		os << bundle.getMessageCount() << " message(s) " << bundle.getBundleCount() << " bundle(s)";
		return os;
	}

   private:
	std::vector<vcvOscMessage> messages;  ///< bundled messages
	std::vector<OscBundle> bundles;       ///< bundled bundles
};