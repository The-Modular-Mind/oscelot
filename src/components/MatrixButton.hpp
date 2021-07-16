#pragma once
#include "../plugin.hpp"

namespace StoermelderPackOne {

struct MatrixButton : app::SvgSwitch {
	MatrixButton() {
		momentary=true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/forward0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/forward1.svg")));
		fb->removeChild(shadow);
		delete shadow;
	}
};

struct MatrixBackButton : app::SvgSwitch {
	MatrixBackButton() {
		momentary=true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/back0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/back1.svg")));
		fb->removeChild(shadow);
		delete shadow;
	}
};

} // namespace StoermelderPackOne