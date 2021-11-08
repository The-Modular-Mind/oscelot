#include "osc/OscController.hpp"

#include "plugin.hpp"

namespace TheModularMind {

class OscFader : public OscController {
   public:
	OscFader(std::string address, int controllerId, CONTROLLERMODE controllerMode, float value, uint32_t ts) {
		this->setTypeString("FDR");
		this->setAddress(address);
		this->setControllerId(controllerId);
		this->setControllerMode(controllerMode);
		OscController::setCurrentValue(value, ts);
	}

	virtual bool setCurrentValue(float value, uint32_t ts) override {
		if (ts == 0 || ts > this->getTs()) {
			return OscController::setCurrentValue(value, ts);
		}
		return false;
	}
};

class OscEncoder : public OscController {
   public:
	OscEncoder(std::string address, int controllerId, float value, uint32_t ts, int sensitivity = ENCODER_DEFAULT_SENSITIVITY) {
		this->setTypeString("ENC");
		this->setAddress(address);
		this->setControllerId(controllerId);
		this->setControllerMode(CONTROLLERMODE::DIRECT);
		this->setSensitivity(sensitivity);
		this->setCurrentValue(value, ts);
	}

	virtual bool setCurrentValue(float value, uint32_t ts) override {
		if (ts == 0) {
			OscController::setCurrentValue(value, ts);
		} else if (ts > this->getTs()) {
			float newValue = this->getCurrentValue() + (value / float(sensitivity));
			OscController::setCurrentValue(clamp(newValue, 0.f, 1.f), ts);
		}
		return this->getCurrentValue() >= 0.f;
	}

	void setSensitivity(int sensitivity) override { this->sensitivity = sensitivity; }
	int getSensitivity() override { return this->sensitivity; }

   private:
	int sensitivity = ENCODER_DEFAULT_SENSITIVITY;
};

class OscButton : public OscController {
   public:
	OscButton(std::string address, int controllerId, CONTROLLERMODE controllerMode, float value, uint32_t ts) {
		this->setTypeString("BTN");
		this->setAddress(address);
		this->setControllerId(controllerId);
		this->setControllerMode(controllerMode);
		OscController::setCurrentValue(value, ts);
	}

	virtual bool setCurrentValue(float value, uint32_t ts) override {
		if (ts == 0) {
			OscController::setCurrentValue(value, ts);
		} else if (ts > this->getTs()) {
			OscController::setCurrentValue(clamp(value, 0.f, 1.0f), ts);
		}
		return this->getCurrentValue() >= 0.f;
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