#pragma once
#include "OscMessage.hpp"
#include "plugin.hpp"

namespace TheModularMind {

class OscBundle {
   public:
	OscBundle() {}

	OscBundle(const OscBundle &oscBundle) { copy(oscBundle); }

	OscBundle &operator=(const OscBundle &oscBundle) { return copy(oscBundle); }

	OscBundle &copy(const OscBundle &oscBundle) {
		if (this == &oscBundle) return *this;
		std::copy(oscBundle.bundles.begin(), oscBundle.bundles.end(), std::back_inserter(bundles));
		std::copy(oscBundle.messages.begin(), oscBundle.messages.end(), std::back_inserter(messages));
		return *this;
	}

	void clear() {
		bundles.clear();
		messages.clear();
	}

	void addBundle(const OscBundle &bundle) { bundles.push_back(bundle); }
	void addMessage(const OscMessage &message) { messages.push_back(message); }
	int getBundleCount() const { return bundles.size(); }
	int getMessageCount() const { return messages.size(); }
	const OscBundle &getBundleAt(std::size_t i) const { return bundles[i]; }
	const OscMessage &getMessageAt(std::size_t i) const { return messages[i]; }

   private:
	std::vector<OscMessage> messages;
	std::vector<OscBundle> bundles;
};
}  // namespace TheModularMind