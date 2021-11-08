#pragma once
#include "plugin.hpp"

namespace TheModularMind {

enum class CONTROLLERMODE { DIRECT = 0, PICKUP1 = 1, PICKUP2 = 2, TOGGLE = 3, TOGGLE_VALUE = 4 };

class OscController {
   public:
	static OscController *Create(std::string address, int controllerId, CONTROLLERMODE controllerMode = CONTROLLERMODE::DIRECT, float value = -1.f, uint32_t ts = 0);
	static const int ENCODER_DEFAULT_SENSITIVITY = 649;

	virtual ~OscController() {}

	float getCurrentValue() { return current; }

	virtual bool setCurrentValue(float value, uint32_t ts) {
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
	virtual void setSensitivity(int sensitivity) {}
	virtual int getSensitivity() { return ENCODER_DEFAULT_SENSITIVITY; }
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

}  // namespace TheModularMind