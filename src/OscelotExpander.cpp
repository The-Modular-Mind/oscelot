#include "OscelotExpander.hpp"

namespace TheModularMind {
namespace Oscelot {

struct OscelotExpander : Module {
	enum ParamIds { NUM_PARAMS };
	enum InputIds { NUM_INPUTS };
	enum OutputIds { ENUMS(TRIG_OUTPUT, 8), ENUMS(CV_OUTPUT, 8), ENUMS(POLY_OUTPUT, 2), NUM_OUTPUTS };
	enum LightIds { NUM_LIGHTS };

	int panelTheme = rand() % 3;
	int expanderId;
	float startVoltage = -5.0f, endVoltage = 5.0f;
	dsp::ClockDivider processDivider;
	dsp::PulseGenerator pulseGenerator[8];
	simd::float_4 last[2];
	std::string labels[8];

	OscelotExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		onReset();
	}

	void onReset() override {
		processDivider.setDivision(64);
		processDivider.reset();

		for (int i = 0; i < 8; i++) {
			labels[i] = "";
			last[i / 4][i % 4] = 0.0f;
			pulseGenerator[i].reset();
		}
		expanderId = 0;
		startVoltage = -5.0f;
		endVoltage = 5.0f;
		rightExpander.consumerMessage = NULL;
		rightExpander.messageFlipRequested = false;
	}

	void process(const ProcessArgs& args) override {
		if (processDivider.process()) {
			Module* expanderMother = leftExpander.module;
			OscelotExpanderBase* module;

			if (!expanderMother || (expanderMother->model != modelOSCelot && expanderMother->model != modelOscelotExpander) || !expanderMother->rightExpander.consumerMessage) return;

			ExpanderPayload* expPayload = reinterpret_cast<ExpanderPayload*>(expanderMother->rightExpander.consumerMessage);
			module = reinterpret_cast<OscelotExpanderBase*>(expPayload->base);
			expanderId = expPayload->expanderId;

			if (expanderId + 8 > MAX_PARAMS) return;

			float* values = module->expGetValues();
			std::string* controllerLabels = module->expGetLabels();
			outputs[POLY_OUTPUT].setChannels(8);
			outputs[POLY_OUTPUT_LAST].setChannels(8);

			for (int i = 0; i < 8; i++) {
				float v = values[i + expanderId];
				labels[i] = controllerLabels[i + expanderId];

				bool trig = simd::ifelse(last[i / 4][i % 4] != v, 1, 0);

				if (trig) {
					pulseGenerator[i].trigger(1e-3);
					last[i / 4][i % 4] = v;
				}

				simd::float_4 trigVoltage = pulseGenerator[i].process(args.sampleTime) ? 10.f : 0.f;
				simd::float_4 cvVoltage = simd::rescale(v, 0.0f, 1.0f, startVoltage, endVoltage);

				outputs[TRIG_OUTPUT + i].setVoltage(trigVoltage[0]);
				outputs[POLY_OUTPUT].setVoltage(trigVoltage[0], i);
				outputs[CV_OUTPUT + i].setVoltage(cvVoltage[0]);
				outputs[POLY_OUTPUT_LAST].setVoltage(cvVoltage[0], i);
			}

			rightExpander.producerMessage = new ExpanderPayload(module, expanderId + 8);
			rightExpander.messageFlipRequested = true;
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));
		json_object_set_new(rootJ, "startVoltage", json_real(startVoltage));
		json_object_set_new(rootJ, "endVoltage", json_real(endVoltage));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		panelTheme = json_integer_value(json_object_get(rootJ, "panelTheme"));
		startVoltage = json_real_value(json_object_get(rootJ, "startVoltage"));
		endVoltage = json_real_value(json_object_get(rootJ, "endVoltage"));
	}
};

struct OscLabelWidget : widget::OpaqueWidget {
	OscelotExpander* module;
	OscelotTextLabel* labelWidgets[8];
	OscelotTextLabel* idLabel;
	NVGcolor color = nvgRGB(0xDA, 0xa5, 0x20);
	NVGcolor white = nvgRGB(0xfe, 0xff, 0xe0);
	Vec lblPos = box.pos;

	void step() override {
		if (!module) return;

		for (int i = 0; i < 8; i++) {
			if (labelWidgets[i]->text != module->labels[i]) labelWidgets[i]->text = module->labels[i];
		}
	}

	void setLabels() {
		clearChildren();

		OscelotTextLabel* l = createWidget<OscelotTextLabel>(lblPos);
		l->box.size = mm2px(Vec(25.4, 1));
		l->text = "POLY";
		addChild(l);
		idLabel = l;
		lblPos = lblPos.plus(Vec(0.0f, 16.0f));

		for (int i = 0; i < 8; i++) {
			lblPos = lblPos.plus(Vec(0.0f, 36.0f));
			OscelotTextLabel* l = createWidget<OscelotTextLabel>(lblPos);
			l->box.size = mm2px(Vec(25.4, 1));
			l->text = module->labels[i];
			addChild(l);
			labelWidgets[i] = l;
		}
	}
};

struct OscelotExpanderWidget : ThemedModuleWidget<OscelotExpander> {
	OscelotExpanderWidget(OscelotExpander* module) : ThemedModuleWidget<OscelotExpander>(module, "OscelotExpander", "Oscelot.md#expander") {
		setModule(module);

		addChild(createWidget<PawScrew>(Vec(2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<PawScrew>(Vec(box.size.x - 3 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		OscLabelWidget* oscLabelWidget = createWidget<OscLabelWidget>(mm2px(Vec(0, 7)));
		oscLabelWidget->module = module;
		if (module) {
			oscLabelWidget->setLabels();
		}
		addChild(oscLabelWidget);

		Vec gatePos = mm2px(Vec(7, 14));
		Vec cvPos = mm2px(Vec(18.4, 14));

		addOutput(createOutputCentered<PawPort>(gatePos, module, OscelotExpander::POLY_OUTPUT));
		addOutput(createOutputCentered<PawPort>(cvPos, module, OscelotExpander::POLY_OUTPUT_LAST));
		gatePos = gatePos.plus(Vec(0.0f, 16.0f));
		cvPos = cvPos.plus(Vec(0.0f, 16.0f));

		for (int i = 0; i < 8; i++) {
			gatePos = gatePos.plus(Vec(0.0f, 36.0f));
			cvPos = cvPos.plus(Vec(0.0f, 36.0f));
			addOutput(createOutputCentered<PawPort>(gatePos, module, OscelotExpander::TRIG_OUTPUT + i));
			addOutput(createOutputCentered<PawPort>(cvPos, module, OscelotExpander::CV_OUTPUT + i));
		}
	}
	void appendContextMenu(Menu* menu) override {
		ThemedModuleWidget<OscelotExpander>::appendContextMenu(menu);
		assert(module);

		struct VoltageMenuItem : MenuItem {
			OscelotExpander* module;
			VoltageMenuItem() { rightText = RIGHT_ARROW; }
			Menu* createChildMenu() override {
				struct StartVoltageMenu : MenuItem {
					OscelotExpander* module;
					StartVoltageMenu() { rightText = RIGHT_ARROW; }

					Menu* createChildMenu() override {
						struct StartVoltageItem : MenuItem {
							OscelotExpander* module;
							float startVoltage;
							StartVoltageItem() {}
							void onAction(const event::Action& e) override { module->startVoltage = startVoltage; }
							void step() override {
								MenuItem::text = string::f("%.0fV", startVoltage);
								rightText = module->startVoltage == startVoltage ? "✔" : "";
								MenuItem::step();
							}
						};

						Menu* menu = new Menu;
						menu->addChild(construct<StartVoltageItem>(&StartVoltageItem::startVoltage, -10.0f, &StartVoltageItem::module, module));
						menu->addChild(construct<StartVoltageItem>(&StartVoltageItem::startVoltage, -5.0f, &StartVoltageItem::module, module));
						menu->addChild(construct<StartVoltageItem>(&StartVoltageItem::startVoltage, -3.0f, &StartVoltageItem::module, module));
						menu->addChild(construct<StartVoltageItem>(&StartVoltageItem::startVoltage, -1.0f, &StartVoltageItem::module, module));
						menu->addChild(construct<StartVoltageItem>(&StartVoltageItem::startVoltage, 0.0f, &StartVoltageItem::module, module));
						menu->addChild(construct<StartVoltageItem>(&StartVoltageItem::startVoltage, 1.0f, &StartVoltageItem::module, module));
						menu->addChild(construct<StartVoltageItem>(&StartVoltageItem::startVoltage, 3.0f, &StartVoltageItem::module, module));
						menu->addChild(construct<StartVoltageItem>(&StartVoltageItem::startVoltage, 5.0f, &StartVoltageItem::module, module));
						menu->addChild(construct<StartVoltageItem>(&StartVoltageItem::startVoltage, 10.0f, &StartVoltageItem::module, module));
						return menu;
					};
				};

				struct EndVoltageMenu : MenuItem {
					OscelotExpander* module;
					EndVoltageMenu() { rightText = RIGHT_ARROW; }

					Menu* createChildMenu() override {
						struct EndVoltageItem : MenuItem {
							OscelotExpander* module;
							float endVoltage;
							EndVoltageItem() {}
							void onAction(const event::Action& e) override { module->endVoltage = endVoltage; }
							void step() override {
								MenuItem::text = string::f("%.0fV", endVoltage);
								rightText = module->endVoltage == endVoltage ? "✔" : "";
								MenuItem::step();
							}
						};

						Menu* menu = new Menu;
						menu->addChild(construct<EndVoltageItem>(&EndVoltageItem::endVoltage, -10.0f, &EndVoltageItem::module, module));
						menu->addChild(construct<EndVoltageItem>(&EndVoltageItem::endVoltage, -5.0f, &EndVoltageItem::module, module));
						menu->addChild(construct<EndVoltageItem>(&EndVoltageItem::endVoltage, -3.0f, &EndVoltageItem::module, module));
						menu->addChild(construct<EndVoltageItem>(&EndVoltageItem::endVoltage, -1.0f, &EndVoltageItem::module, module));
						menu->addChild(construct<EndVoltageItem>(&EndVoltageItem::endVoltage, 0.0f, &EndVoltageItem::module, module));
						menu->addChild(construct<EndVoltageItem>(&EndVoltageItem::endVoltage, 1.0f, &EndVoltageItem::module, module));
						menu->addChild(construct<EndVoltageItem>(&EndVoltageItem::endVoltage, 3.0f, &EndVoltageItem::module, module));
						menu->addChild(construct<EndVoltageItem>(&EndVoltageItem::endVoltage, 5.0f, &EndVoltageItem::module, module));
						menu->addChild(construct<EndVoltageItem>(&EndVoltageItem::endVoltage, 10.0f, &EndVoltageItem::module, module));
						return menu;
					};
				};

				struct VoltageRangeMenu : MenuItem {
					OscelotExpander* module;
					VoltageRangeMenu() { rightText = RIGHT_ARROW; }

					Menu* createChildMenu() override {
						struct VoltageRangeItem : MenuItem {
							OscelotExpander* module;
							float startVoltage, endVoltage;
							VoltageRangeItem() { rightText = RIGHT_ARROW; }
							void onAction(const event::Action& e) override {
								module->startVoltage = startVoltage;
								module->endVoltage = endVoltage;
							}
							void step() override {
								MenuItem::text = string::f("%.0fV to %.0fV", startVoltage, endVoltage, abs(endVoltage - startVoltage));
								rightText = module->startVoltage == startVoltage ? module->endVoltage == endVoltage ? "✔" : "" : "";
								MenuItem::step();
							}
						};

						Menu* menu = new Menu;
						menu->addChild(construct<VoltageRangeItem>(&VoltageRangeItem::startVoltage, -1.0f, &VoltageRangeItem::module, module, &VoltageRangeItem::endVoltage, 1.0f));
						menu->addChild(construct<VoltageRangeItem>(&VoltageRangeItem::startVoltage, -3.0f, &VoltageRangeItem::module, module, &VoltageRangeItem::endVoltage, 3.0f));
						menu->addChild(construct<VoltageRangeItem>(&VoltageRangeItem::startVoltage, -5.0f, &VoltageRangeItem::module, module, &VoltageRangeItem::endVoltage, 5.0f));
						menu->addChild(construct<VoltageRangeItem>(&VoltageRangeItem::startVoltage, -10.0f, &VoltageRangeItem::module, module, &VoltageRangeItem::endVoltage, 10.0f));
						menu->addChild(construct<VoltageRangeItem>(&VoltageRangeItem::startVoltage, 0.0f, &VoltageRangeItem::module, module, &VoltageRangeItem::endVoltage, 1.0f));
						menu->addChild(construct<VoltageRangeItem>(&VoltageRangeItem::startVoltage, 0.0f, &VoltageRangeItem::module, module, &VoltageRangeItem::endVoltage, 3.0f));
						menu->addChild(construct<VoltageRangeItem>(&VoltageRangeItem::startVoltage, 0.0f, &VoltageRangeItem::module, module, &VoltageRangeItem::endVoltage, 5.0f));
						menu->addChild(construct<VoltageRangeItem>(&VoltageRangeItem::startVoltage, 0.0f, &VoltageRangeItem::module, module, &VoltageRangeItem::endVoltage, 10.0f));
						return menu;
					};
				};

				Menu* menu = new Menu;
				menu->addChild(construct<VoltageRangeMenu>(&MenuItem::text, "Voltage Range", &VoltageRangeMenu::module, module));
				menu->addChild(construct<StartVoltageMenu>(&MenuItem::text, "Start Voltage", &StartVoltageMenu::module, module));
				menu->addChild(construct<EndVoltageMenu>(&MenuItem::text, "End Voltage", &EndVoltageMenu::module, module));
				return menu;
			}
		};
		menu->addChild(new MenuSeparator());
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, string::f("CV Range:    %.0fV to %.0fV  ", module->startVoltage, module->endVoltage)));
		menu->addChild(construct<VoltageMenuItem>(&MenuItem::text, "Configure CV", &VoltageMenuItem::module, module));
	}
};
}  // namespace Oscelot
}  // namespace TheModularMind

Model* modelOscelotExpander = createModel<TheModularMind::Oscelot::OscelotExpander, TheModularMind::Oscelot::OscelotExpanderWidget>("OSCelotExpander");