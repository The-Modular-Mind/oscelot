#pragma once
#include "plugin.hpp"
#include "ui/ThemedModuleWidget.hpp"
#include "components/LedTextDisplay.hpp"


struct StoermelderBlackScrew : app::SvgScrew {
	widget::TransformWidget* tw;

	StoermelderBlackScrew() {
		fb->removeChild(sw);

		tw = new TransformWidget();
		tw->addChild(sw);
		fb->addChild(tw);

		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/Screw.svg")));

		tw->box.size = sw->box.size;
		box.size = tw->box.size;

		float angle = random::uniform() * M_PI;
		tw->identity();
		// Rotate SVG
		math::Vec center = sw->box.getCenter();
		tw->translate(center);
		tw->rotate(angle);
		tw->translate(center.neg());
	}
};


struct TriggerParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		return ParamQuantity::getLabel();
	}
	std::string getLabel() override {
		return "";
	}
};

struct BufferedTriggerParamQuantity : TriggerParamQuantity {
	float buffer = false;
	void setValue(float value) override {
		if (value >= 1.f) buffer = true;
		TriggerParamQuantity::setValue(value);
	}
	void resetBuffer() {
		buffer = false;
	}
};