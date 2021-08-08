#include "plugin.hpp"
#include "Oscelot.hpp"
#include "components/LedTextField.hpp"

namespace TheModularMind {
namespace Oscelot {

struct OscelotCtxModule : OscelotCtxBase {
	enum ParamIds {
		PARAM_MAP,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		LIGHT_APPLY,
		NUM_LIGHTS
	};

	/** [Stored to JSON] */
	int panelTheme = 0;
	/** [Stored to JSON] */
	std::string oscelotId;

	OscelotCtxModule() {
		panelTheme = pluginSettings.panelThemeDefault;
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<BufferedTriggerParamQuantity>(PARAM_MAP, 0.f, 1.f, 0.f, "Start parameter mapping");
		onReset();
	}

	void onReset() override {
		Module::onReset();
		oscelotId = "";
	}

	std::string getOscelotId() override {
		return oscelotId;
	}

	json_t* dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));
		json_object_set_new(rootJ, "oscelotId", json_string(oscelotId.c_str()));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		panelTheme = json_integer_value(json_object_get(rootJ, "panelTheme"));
		oscelotId = json_string_value(json_object_get(rootJ, "oscelotId"));
	}
};


struct IdTextField : OscelotTextField {
	OscelotCtxModule* module;
	void step() override {
		OscelotTextField::step();
		if (!module) return;
		if (isFocused) module->oscelotId = text;
		else text = module->oscelotId;
	}
};

struct OscelotCtxWidget : ThemedModuleWidget<OscelotCtxModule> {
	OscelotCtxWidget(OscelotCtxModule* module)
		: ThemedModuleWidget<OscelotCtxModule>(module, "OscelotCtx", "Oscelot.md#ctx-expander") {
		setModule(module);

		addChild(createWidget<PawScrew>(Vec(box.size.x - RACK_GRID_WIDTH, 0)));
		addChild(createWidget<PawScrew>(Vec(box.size.x - RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(createParamCentered<TL1105>(Vec(15.0f, 258.6f), module, OscelotCtxModule::PARAM_MAP));

		IdTextField* textField = createWidget<IdTextField>(Vec());
		textField->textSize = 13.f;
		textField->maxTextLength = 8;
		textField->module = module;
		textField->box.size = Vec(54.f, 13.f);

		TransformWidget* tw = new TransformWidget;
		tw->addChild(textField);
		tw->box.pos = Vec(-12.f, 305.f);
		tw->box.size = Vec(120.f, 13.f);
		addChild(tw);

		math::Vec center = textField->box.getCenter();
		tw->identity();
		tw->translate(center);
		tw->rotate(-M_PI / 2);
		tw->translate(center.neg());
	}
};

} // namespace Oscelot
} // namespace StoermelderPackOne

Model* modelOscelotCtx = createModel<TheModularMind::Oscelot::OscelotCtxModule, TheModularMind::Oscelot::OscelotCtxWidget>("OscelotCtx");