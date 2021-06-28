#include "plugin.hpp"
#include "drivers/MidiLoopback.hpp"

Plugin* pluginInstance;

void init(rack::Plugin* p) {
	pluginInstance = p;

	
	p->addModel(modelMidiCat);
	p->addModel(modelMidiCatMem);
	p->addModel(modelMidiCatCtx);

	pluginSettings.readFromJson();

	if (pluginSettings.midiLoopbackDriverEnabled) {
		StoermelderPackOne::MidiLoopback::init();
	}
}


namespace StoermelderPackOne {

std::map<std::string, Widget*> singletons;

bool registerSingleton(std::string name, Widget* mw) {
	auto it = singletons.find(name);
	if (it == singletons.end()) {
		singletons[name] = mw;
		return true;
	}
	return false;
}

bool unregisterSingleton(std::string name, Widget* mw) {
	auto it = singletons.find(name);
	if (it != singletons.end() && it->second == mw) {
		singletons.erase(it);
		return true;
	}
	return false;
}

Widget* getSingleton(std::string name) {
	auto it = singletons.find(name);
	return it != singletons.end() ? it->second : NULL;
}

} // namespace StoermelderPackOne