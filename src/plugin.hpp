#include "rack.hpp"
#include "components.hpp"
#include "helpers.hpp"
#include "pluginsettings.hpp"

using namespace rack;


extern Plugin* pluginInstance;

extern StoermelderSettings pluginSettings;

extern Model* modelMidiCat;
extern Model* modelMidiCatMem;
extern Model* modelMidiCatCtx;


namespace StoermelderPackOne {

bool registerSingleton(std::string name, Widget* mw);
bool unregisterSingleton(std::string name, Widget* mw);
Widget* getSingleton(std::string name);

} // namespace StoermelderPackOne