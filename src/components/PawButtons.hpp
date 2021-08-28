#pragma once
#include "../plugin.hpp"

namespace TheModularMind {

struct PawLight : rack::GrayModuleLightWidget {
	std::shared_ptr<Svg> svg;
	float degrees = 0.0;

	PawLight() {
		// color=nvgRGB(0xff, 0xf0, 0xf5);
		color = nvgRGB(0xff, 0xfa, 0xcd);
		this->addBaseColor(color);
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/paw.svg")));
	}

	void drawLight(const Widget::DrawArgs& args) override {
		nvgBeginPath(args.vg);

		nvgTranslate(args.vg, svg->handle->width / 2.0, svg->handle->height / 2.0);
		nvgRotate(args.vg, nvgDegToRad(degrees));
		nvgTranslate(args.vg, svg->handle->width / -2.0, svg->handle->height / -2.0);

		svgDraw(args.vg, svg->handle);
		nvgFillColor(args.vg, this->color);
		nvgFill(args.vg);
	}

	void setSvg(std::shared_ptr<Svg> svg) {
		this->svg = svg;

		if (svg && svg->handle) {
			this->box.size = math::Vec(svg->handle->width, svg->handle->height);
		} else {
			this->box.size = math::Vec();
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
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/paw0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/paw1.svg")));
		fb->removeChild(shadow);
		delete shadow;
	}
};

struct PawScrew : app::SvgScrew {
	PawScrew() { setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/Screw.svg"))); }
};

struct PawPort : app::SvgPort {
	PawPort() { setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/Port.svg"))); }
};

}  // namespace TheModularMind