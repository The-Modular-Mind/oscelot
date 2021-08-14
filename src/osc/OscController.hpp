#pragma once
#include "plugin.hpp"

namespace TheModularMind {

enum class CONTROLLERMODE { DIRECT = 0, PICKUP1 = 1, PICKUP2 = 2, TOGGLE = 3, TOGGLE_VALUE = 4 };

class OscController {
   public:
	static OscController *Create(std::string address, int controllerId, CONTROLLERMODE controllerMode = CONTROLLERMODE::DIRECT, float value = -1.f, uint32_t ts = 0);

	virtual ~OscController() {}

	float getValue() { return current; }

	virtual bool setValue(float value, uint32_t ts) {
		current = value;
		lastTs = ts;
		return true;
	}

	void reset() {
		controllerId = -1;
		lastTs = 0;
		current = -1.0f;
		lastValueIn = -1.f;
		lastValueIndicate = -1.f;
		lastValueOut = -1.f;
	}

	void resetValue() { current = -1.0f; }
	virtual void sendFeedback(float value, bool sendOnly) {}
	int getControllerId() { return controllerId; }
	void setControllerId(int controllerId) { this->controllerId = controllerId; }
	void setTs(uint32_t ts) { this->lastTs = ts; }
	uint32_t getTs() { return lastTs; }
	void setAddress(std::string address) { this->address = address; }
	std::string getAddress() { return address; }
	const char *getTypeString() { return type; }
	void setTypeString(const char *type) { this->type = type; }
	void setControllerMode(CONTROLLERMODE controllerMode) { this->controllerMode = controllerMode; }
	CONTROLLERMODE getControllerMode() { return controllerMode; }

	void setValueIn(float value) { lastValueIn = value; }
	float getValueIn() { return lastValueIn; }
	void setValueOut(float value) { lastValueOut = value; }
	float getValueOut() { return lastValueOut; }
	void setValueIndicate(float value) { lastValueIndicate = value; }
	float getValueIndicate() { return lastValueIndicate; }

   private:
	int controllerId = -1;
	uint32_t lastTs = 0;
	float current;
	std::string address;
	const char *type;
	CONTROLLERMODE controllerMode;

	float lastValueIn = -1.f;
	float lastValueIndicate = -1.f;
	float lastValueOut = -1.f;
};

class OscFader : public OscController {
   public:
	OscFader(std::string address, int controllerId, CONTROLLERMODE controllerMode, float value, uint32_t ts) {
		this->setTypeString("FDR");
		this->setAddress(address);
		this->setControllerId(controllerId);
		this->setControllerMode(controllerMode);
		OscController::setValue(value, ts);
	}

	virtual bool setValue(float value, uint32_t ts) override {
		if (ts == 0 || ts > this->getTs()) {
			return OscController::setValue(value, ts);
		}
		return false;
	}
};

class OscEncoder : public OscController {
   public:
	OscEncoder(std::string address, int controllerId, float value, uint32_t ts, int steps = 649) {
		this->setTypeString("ENC");
		this->setAddress(address);
		this->setControllerId(controllerId);
		this->setSteps(steps);
		this->setValue(value, ts);
	}

	virtual bool setValue(float value, uint32_t ts) override {
		if (ts == 0) {
			OscController::setValue(value, ts);
		} else if (ts > this->getTs()) {
			float newValue = this->getValue() + (value / float(steps));
			OscController::setValue(clamp(newValue, 0.f, 1.f), ts);
		}
		return this->getValue() >= 0.f;
	}

	void setSteps(int steps) { this->steps = steps; }

   private:
	int steps = 649;
};

class OscButton : public OscController {
   public:
	OscButton(std::string address, int controllerId, CONTROLLERMODE controllerMode, float value, uint32_t ts) {
		this->setTypeString("BTN");
		this->setAddress(address);
		this->setControllerId(controllerId);
		this->setControllerMode(controllerMode);
		OscController::setValue(value, ts);
	}

	virtual bool setValue(float value, uint32_t ts) override {
		if (ts == 0) {
			OscController::setValue(value, ts);
		} else if (ts > this->getTs()) {
			OscController::setValue(clamp(value, 0.f, 1.0f), ts);
		}
		return this->getValue() >= 0.f;
	}
};

bool endsWith(std::string const &fullString, std::string const &ending) {
	if (fullString.length() >= ending.length()) {
		return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
	} else {
		return false;
	}
}

OscController *OscController::Create(std::string address, int controllerId, CONTROLLERMODE controllerMode, float value, uint32_t ts) {
	if (endsWith(address, "/fader")) {
		return new OscFader(address, controllerId, controllerMode, value, ts);
	} else if (endsWith(address, "/encoder")) {
		return new OscEncoder(address, controllerId, value, ts);
	} else if (endsWith(address, "/button")) {
		return new OscButton(address, controllerId, controllerMode, value, ts);
	} else
		INFO("Not Implemented for address: %s", address.c_str());
	return nullptr;
};

}  // namespace TheModularMind