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