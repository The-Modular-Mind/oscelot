#pragma once
#include "plugin.hpp"
#include "osc/OscSender.hpp"
#include "osc/OscReceiver.hpp"
#include "components/LedTextField.hpp"
#include "components/MeowMory.hpp"
#include "osc/OscController.hpp"

namespace TheModularMind {
namespace Oscelot {

static const int MAX_PARAMS = 640;
static const std::string RXPORT_DEFAULT = "8881";
static const std::string TXPORT_DEFAULT = "8880";
static const std::string OSCMSG_MODULE_START = "/oscelot/moduleMeowMory/start";
static const std::string OSCMSG_MODULE_END = "/oscelot/moduleMeowMory/end";
static const std::string OSCMSG_BANK_START = "/oscelot/bankMeowMory/start";
static const std::string OSCMSG_BANK_END = "/oscelot/bankMeowMory/end";
static const std::string OSCMSG_MODULE_LIST = "/oscelot/moduleList";
static const std::string OSCMSG_PREV_MODULE = "/oscelot/prev";
static const std::string OSCMSG_NEXT_MODULE = "/oscelot/next";
static const std::string OSCMSG_BANK_SELECT = "/oscelot/bank";
static const std::string OSCMSG_LIST_MODULES = "/oscelot/listmodules";
static const std::string OSCMSG_STORE_CLIENT_STATE = "/oscelot/storestate";
static const std::string OSCMSG_GET_CLIENT_STATE = "/oscelot/getstate";


} // namespace Oscelot
} // namespace TheModularMind