#pragma once
#include "plugin.hpp"
#include "components/PawButtons.hpp"
#include "ui/ThemedModuleWidget.hpp"

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