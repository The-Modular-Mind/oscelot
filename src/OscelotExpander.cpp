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
	int startVoltageIndex = 1;
	int endVoltageIndex = 7;
	float controlVoltages[9] = { -10.0f, -5.0f, -3.0f, -1.0f , 0.0f, 1.0f, 3.0f, 5.0f, 10.0f };
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
			outputs[CV_OUTPUT + i].clearVoltages();
			outputs[POLY_OUTPUT_LAST].clearVoltages();
		}
		expanderId = 0;
		rightExpander.producerMessage = NULL;
		rightExpander.messageFlipRequested = false;
	}

	void process(const ProcessArgs& args) override {
		if (processDivider.process()) {
			Module* expanderMother = leftExpander.module;
			OscelotExpanderBase* module;

			if (!expanderMother || (expanderMother->model != modelOSCelot && expanderMother->model != modelOscelotExpander) || !expanderMother->rightExpander.consumerMessage) {
				onReset();
				return;
			}

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
				if (v < 0.0f) return;
				labels[i] = controllerLabels[i + expanderId];

				bool trig = simd::ifelse(last[i / 4][i % 4] != v, 1, 0);

				if (trig) {
					pulseGenerator[i].trigger(1e-3);
					last[i / 4][i % 4] = v;
				}

				simd::float_4 trigVoltage = pulseGenerator[i].process(args.sampleTime) ? 10.f : 0.f;
				simd::float_4 cvVoltage = simd::rescale(v, 0.0f, 1.0f, controlVoltages[startVoltageIndex], controlVoltages[endVoltageIndex]);

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
		json_object_set_new(rootJ, "startVoltageIndex", json_real(startVoltageIndex));
		json_object_set_new(rootJ, "endVoltageIndex", json_real(endVoltageIndex));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		panelTheme = json_integer_value(json_object_get(rootJ, "panelTheme"));
		startVoltageIndex = json_real_value(json_object_get(rootJ, "startVoltageIndex"));
		endVoltageIndex = json_real_value(json_object_get(rootJ, "endVoltageIndex"));
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

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel(string::f("CV Range: %.0fV to %.0fV", module->controlVoltages[ module->startVoltageIndex], module->controlVoltages[ module->endVoltageIndex])));
		
		menu->addChild(createSubmenuItem("Configure CV", "", [=](Menu* menu) {
			menu->addChild(createSubmenuItem(string::f("Voltage Range (%.0fV)", abs(module->controlVoltages[module->startVoltageIndex] - module->controlVoltages[module->endVoltageIndex])), "", [=](Menu* menu) {
					menu->addChild(createCheckMenuItem(
						"-1V to 1V", "", [=]() { return module->startVoltageIndex == 3 && module->endVoltageIndex == 5; },
						[=]() {
							module->startVoltageIndex = 3;
							module->endVoltageIndex = 5;
						}));
					menu->addChild(createCheckMenuItem(
						"-3V to 3V", "", [=]() { return module->startVoltageIndex == 2 && module->endVoltageIndex == 6; },
						[=]() {
							module->startVoltageIndex = 2;
							module->endVoltageIndex = 6;
						}));
					menu->addChild(createCheckMenuItem(
						"-5V to 5V", "", [=]() { return module->startVoltageIndex == 1 && module->endVoltageIndex == 7; },
						[=]() {
							module->startVoltageIndex = 1;
							module->endVoltageIndex = 7;
						}));
					menu->addChild(createCheckMenuItem(
						"-10V to 10V", "", [=]() { return module->startVoltageIndex == 0 && module->endVoltageIndex == 8; },
						[=]() {
							module->startVoltageIndex = 0;
							module->endVoltageIndex = 8;
						}));
					menu->addChild(createCheckMenuItem(
						"0V to 1V", "", [=]() { return module->startVoltageIndex == 4 && module->endVoltageIndex == 5; },
						[=]() {
							module->startVoltageIndex = 4;
							module->endVoltageIndex = 5;
						}));
					menu->addChild(createCheckMenuItem(
						"0V to 3V", "", [=]() { return module->startVoltageIndex == 4 && module->endVoltageIndex == 6; },
						[=]() {
							module->startVoltageIndex = 4;
							module->endVoltageIndex = 6;
						}));
					menu->addChild(createCheckMenuItem(
						"0V to 5V", "", [=]() { return module->startVoltageIndex == 4 && module->endVoltageIndex == 7; },
						[=]() {
							module->startVoltageIndex = 4;
							module->endVoltageIndex = 7;
						}));
					menu->addChild(createCheckMenuItem(
						"0V to 10V", "", [=]() { return module->startVoltageIndex == 4 && module->endVoltageIndex == 8; },
						[=]() {
							module->startVoltageIndex = 4;
							module->endVoltageIndex = 8;
						}));
			    }));
			menu->addChild(createIndexPtrSubmenuItem("Start Voltage", { "-10 V", "-5 V", "-3 V", "-1 V" , "0 V", "1 V", "3 V", "5 V", "10 V" }, &module->startVoltageIndex));
			menu->addChild(createIndexPtrSubmenuItem("End Voltage", { "-10 V", "-5 V", "-3 V", "-1 V" , "0 V", "1 V", "3 V", "5 V", "10 V" }, &module->endVoltageIndex));
		}));
	}
};
}  // namespace Oscelot
}  // namespace TheModularMind

Model* modelOscelotExpander = createModel<TheModularMind::Oscelot::OscelotExpander, TheModularMind::Oscelot::OscelotExpanderWidget>("OSCelotExpander");