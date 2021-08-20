#pragma once
#include "plugin.hpp"
#include "osc/OscSender.hpp"
#include "osc/OscReceiver.hpp"
#include "components/LedTextField.hpp"

namespace TheModularMind {
namespace Oscelot {

static const int MAX_PARAMS = 300;
static const std::string RXPORT_DEFAULT = "8881";
static const std::string TXPORT_DEFAULT = "8880";

} // namespace Oscelot
} // namespace TheModularMind