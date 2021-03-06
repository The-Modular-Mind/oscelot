#include "plugin.hpp"
#include <thread>

namespace TheModularMind {

template < typename MODULE, typename BASE = ModuleWidget >
struct ThemedModuleWidget : BASE {
	MODULE* module;
	std::string baseName;
	std::string manualName;
	int panelTheme = -1;

	struct SplitPanel : SvgPanel {
		ThemedModuleWidget<MODULE, BASE>* w;
		SvgPanel* t;
		SvgPanel* u;
		void draw(const DrawArgs& args) override {
			if (!w) return;
			nvgScissor(args.vg, w->box.size.x / 4.f, 0, w->box.size.x / 4.f, w->box.size.y);
			SvgPanel::draw(args);
			nvgResetScissor(args.vg);

			nvgScissor(args.vg, t->box.size.x * 2.f / 4.f, 0, t->box.size.x * 2.f / 4.f, t->box.size.y);
			t->draw(args);

			nvgScissor(args.vg, u->box.size.x * 3.f / 4.f, 0, u->box.size.x * 3.f / 4.f, u->box.size.y);
			u->draw(args);
			nvgResetScissor(args.vg);
		}
	};

	ThemedModuleWidget(MODULE* module, std::string baseName, std::string manualName = "") {
		this->module = module;
		this->baseName = baseName;
		this->manualName = manualName;

		if (module) {
			// Normal operation
			BASE::setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, panel())));
		}
		else {
			// Module Browser
			BASE::setPanel(Svg::load(asset::plugin(pluginInstance, "res/" + baseName + "_Brass.svg")));
			SplitPanel* splitPanel = new SplitPanel();
			SvgPanel* t = new SvgPanel();
			SvgPanel* u = new SvgPanel();
			t->setBackground(Svg::load(asset::plugin(pluginInstance, "res/" + baseName + "_BlueSteel.svg")));
			u->setBackground(Svg::load(asset::plugin(pluginInstance, "res/" + baseName + "_BlackSteel.svg")));
			splitPanel->t = t;
			splitPanel->u = u;

			splitPanel->w = this;
			splitPanel->setBackground(Svg::load(asset::plugin(pluginInstance, "res/" + baseName + "_GunMetal.svg")));
			BASE::addChild(splitPanel);
		}
	}

	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuSeparator());
		menu->addChild(createIndexPtrSubmenuItem("Panel", {"Gun Metal", "Blue Steel", "Yellow Brass", "Black Steel"}, &module->panelTheme));
		BASE::appendContextMenu(menu);
	}

	void step() override {
		if (module && module->panelTheme != panelTheme) {
			panelTheme = module->panelTheme;
			BASE::setPanel(Svg::load(asset::plugin(pluginInstance, panel())));
		}
		BASE::step();
	}

	std::string panel() {
		switch (panelTheme) {
			default:
			case 0:
				return "res/" + baseName + "_GunMetal.svg";
			case 1:
				return "res/" + baseName + "_BlueSteel.svg";
			case 2:
				return "res/" + baseName + "_Brass.svg";
			case 3:
				return "res/" + baseName + "_BlackSteel.svg";
		}
	}
};

} // namespace TheModularMind