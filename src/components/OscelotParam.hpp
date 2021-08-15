#pragma once
#include "plugin.hpp"

namespace TheModularMind {

struct OscelotParam {
	ParamQuantity* paramQuantity = NULL;
	float limitMin;
	float limitMax;
	float uninit;
	float min = 0.f;
	float max = 1.f;

	float valueIn;
	float value;
	float valueOut;

	OscelotParam() { reset(); }

	bool isNear(float value, float jump = -1.0f) {
		if (value == -1.f) return false;
		float p = getValue();
		float delta3p = (limitMax - limitMin + 1) * 0.01f;
		bool r = p - delta3p <= value && value <= p + delta3p;

		if (jump >= 0.f) {
			float delta7p = (limitMax - limitMin + 1) * 0.03f;
			r = r && p - delta7p <= jump && jump <= p + delta7p;
		}

		return r;
	}

	void setLimits(float min, float max, float uninit) {
		limitMin = min;
		limitMax = max;
		this->uninit = uninit;
	}
	float getLimitMin() { return limitMin; }
	float getLimitMax() { return limitMax; }

	void reset(bool resetSettings = true) {
		paramQuantity = NULL;
		valueIn = uninit;
		value = -1.f;
		valueOut = std::numeric_limits<float>::infinity();

		if (resetSettings) {
			min = 0.f;
			max = 1.f;
		}
	}

	void setParamQuantity(ParamQuantity* pq) {
		paramQuantity = pq;
		if (paramQuantity && valueOut == std::numeric_limits<float>::infinity()) {
			valueOut = paramQuantity->getScaledValue();
		}
	}

	void setMin(float v) {
		min = v;
		if (paramQuantity && valueIn != -1) setValue(valueIn);
	}
	float getMin() { return min; }

	void setMax(float v) {
		max = v;
		if (paramQuantity && valueIn != -1) setValue(valueIn);
	}
	float getMax() { return max; }

	void setValue(float i) {
		float f = rescale(i, limitMin, limitMax, min, max);
		f = clamp(f, 0.f, 1.f);
		valueIn = i;
		value = f;
	}

	void process(float sampleTime = -1.f, bool force = false) {
		if (valueOut == std::numeric_limits<float>::infinity()) return;

		if (valueOut != value || force) {
			paramQuantity->setScaledValue(value);
			valueOut = value;
		}
	}

	float getValue() {
		float f = paramQuantity->getScaledValue();
		if (valueOut == std::numeric_limits<float>::infinity()) value = valueOut = f;
		f = rescale(f, min, max, limitMin, limitMax);
		f = clamp(f, limitMin, limitMax);
		if (valueIn == uninit) valueIn = f;
		return f;
	}
};  // struct OscelotParam

}  // namespace TheModularMind