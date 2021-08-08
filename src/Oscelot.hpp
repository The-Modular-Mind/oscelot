#pragma once
#include "plugin.hpp"
#include "osc/OscSender.hpp"
#include "osc/OscReceiver.hpp"
#include "components/LedTextField.hpp"

namespace TheModularMind {
namespace Oscelot {

static const int MAX_CHANNELS = 256;
static const std::string RXPORT_DEFAULT = "7009";
static const std::string TXPORT_DEFAULT = "7002";

struct OscelotCtxBase : Module {
	virtual std::string getOscelotId() { return ""; }
};

} // namespace Oscelot
} // namespace TheModularMind