#pragma once
#include "../plugin.hpp"
#include "LedTextField.hpp"

namespace TheModularMind {

struct PawLight : SvgLight {
	std::shared_ptr<Svg> svg;
	std::string svgPath;
	float degrees = 0.0;

	PawLight() {
		settings::haloBrightness = 0.1f;
		color = nvgRGB(0xff, 0xfa, 0xcd);
		this->addBaseColor(color);
		this->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/paw.svg")));
	}

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);

		// Rotate Unlit SVG
		nvgTranslate(args.vg, box.size.x / 2.0, box.size.y / 2.0);
		nvgRotate(args.vg, nvgDegToRad(degrees));
		nvgTranslate(args.vg, box.size.x / -2.0, box.size.y / -2.0);

		sw->draw(args);
		nvgFillColor(args.vg, color);
		nvgFill(args.vg);
	}

	void drawLight(const DrawArgs& args) override {
		// Foreground
		if (color.a > 0.0) {
			nvgBeginPath(args.vg);

			// Rotate Lit SVG
			nvgTranslate(args.vg, box.size.x / 2.0, box.size.y / 2.0);
			nvgRotate(args.vg, nvgDegToRad(degrees));
			nvgTranslate(args.vg, box.size.x / -2.0, box.size.y / -2.0);

			sw->draw(args);
			nvgFillColor(args.vg, color);
			nvgFill(args.vg);
		}
	}
};

struct PawNextLight : PawLight {
	PawNextLight() { degrees = 90.0; }
};

struct PawPrevLight : PawLight {
	PawPrevLight() { degrees = -90.0; }
};

struct PawButton : app::SvgSwitch {
	PawButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/paw0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/paw1.svg")));
		fb->removeChild(shadow);
		delete shadow;
	}
};

struct PawScrew : app::SvgScrew {
	PawScrew() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Screw.svg"))); }
};

struct PawPort : app::SvgPort {
	PawPort() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Port.svg"))); }
};

struct LabelSliderHorizontal : app::SvgSlider {
	OscelotTextLabel* label;

	LabelSliderHorizontal() {
		horizontal = true;
		snap = true;
		setHandleSvg(Svg::load(asset::plugin(pluginInstance, "res/components/SliderHandle.svg")));
		setBackgroundSvg(Svg::load(asset::plugin(pluginInstance, "res/components/SliderHorizontal.svg")));
		minHandlePos = math::Vec(1, box.size.y / 2.0f - 12);
		maxHandlePos = math::Vec(box.size.x - handle->box.size.x - 1, box.size.y / 2.0f - 12);

		label = new OscelotTextLabel();
		label->box.size = mm2px(Vec(0.0, -3.3));
		label->textSize = 10.0f;
		label->drawBackground = false;
		this->addChild(label);
	}

	void step() override {
		SvgSlider::step();
		// Move the center of the label to the center of the handle
		label->box.pos = this->handle->box.pos.plus(this->handle->box.size.div(2)).minus(label->box.size.div(2));
	}
};

}  // namespace TheModularMind