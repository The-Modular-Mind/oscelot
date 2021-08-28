#pragma once
#include "plugin.hpp"
#include "components/LedTextField.hpp"
#include "Oscelot.hpp"

namespace TheModularMind {
namespace Oscelot {

struct OscelotExpanderBase {
	virtual float* expGetValues() { return NULL; }
	virtual std::string* expGetLabels() { return NULL; }
};

struct ExpanderPayload {
	OscelotExpanderBase* base;
	int expanderId;
	
	ExpanderPayload(OscelotExpanderBase* base, int expanderId) {
		this->base = base;
		this->expanderId = expanderId;
	}
};

} // namespace Oscelot
} // namespace TheModularMind