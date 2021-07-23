#pragma once
#include "plugin.hpp"
#include "components/LedTextField.hpp"

namespace TheModularMind {
namespace Oscelot {

static const int MAX_CHANNELS = 256;

#define OSCOPTION_VELZERO_BIT 0

enum class CCMODE {
	DIRECT = 0,
	PICKUP1 = 1,
	PICKUP2 = 2,
	TOGGLE = 3,
	TOGGLE_VALUE = 4
};

struct OscelotCtxBase : Module {
	virtual std::string getOscelotId() { return ""; }
};


struct MemParam {
	int paramId = -1;
	std::string address;
	int cc = -1;
	CCMODE ccMode;
	std::string label;
	int oscOptions = 0;
};

struct MemModule {
	std::string pluginName;
	std::string moduleName;
	std::list<MemParam*> paramMap;
	~MemModule() {
		for (auto it : paramMap) delete it;
	}
};

} // namespace Oscelot
} // namespace StoermelderPackOne