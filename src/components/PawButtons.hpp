#pragma once
#include "../plugin.hpp"

namespace TheModularMind {

struct PawForwardButton : app::SvgSwitch {
	PawForwardButton() {
		momentary=true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/forward0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/forward1.svg")));
	}
};

struct PawBackButton : app::SvgSwitch {
	PawBackButton() {
		momentary=true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/back0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/back1.svg")));
	}
};

struct PawScrew : app::SvgScrew {
	widget::TransformWidget* tw;

	PawScrew() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/Screw.svg")));
	}
};

} // namespace StoermelderPackOne