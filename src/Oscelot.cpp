#include "Oscelot.hpp"

#include <osdialog.h>

#include "MapModuleBase.hpp"
#include "components/OscelotParam.hpp"
#include "osc/OscController.hpp"
#include "plugin.hpp"
#include "ui/ParamWidgetContextExtender.hpp"

namespace TheModularMind {
namespace Oscelot {

// static const char PRESET_FILTERS[] = "VCV Rack module preset (.vcvm):vcvm";

struct MeowMoryParam {
	int paramId = -1;
	std::string address;
	int controllerId = -1;
	int encSensitivity = OscController::ENCODER_DEFAULT_SENSITIVITY;
	CONTROLLERMODE controllerMode;
	std::string label;
};

struct MeowMory {
	std::string pluginName;
	std::string moduleName;
	std::list<MeowMoryParam> paramMap;
	~MeowMory() { paramMap.clear(); }
};

enum OSCMODE { OSCMODE_DEFAULT = 0, OSCMODE_LOCATE = 1 };

struct OscelotModule : Module {
	enum ParamIds { PARAM_RECV, PARAM_SEND, PARAM_PREV, PARAM_NEXT, PARAM_APPLY, NUM_PARAMS };
	enum InputIds { NUM_INPUTS };
	enum OutputIds { NUM_OUTPUTS };
	enum LightIds { ENUMS(LIGHT_RECV, 3), ENUMS(LIGHT_SEND, 3), LIGHT_APPLY, LIGHT_PREV, LIGHT_NEXT, NUM_LIGHTS};

	OscReceiver oscReceiver;
	OscSender oscSender;
	std::string ip = "localhost";
	std::string rxPort = RXPORT_DEFAULT;
	std::string txPort = TXPORT_DEFAULT;

	int panelTheme = rand() % 3;

	/** Number of maps */
	int mapLen = 0;
	bool oscIgnoreDevices;
	bool clearMapsOnLoad;

	/** The mapped param handle of each channel */
	ParamHandle paramHandles[MAX_PARAMS];
	std::string textLabels[MAX_PARAMS];
	OscelotParam oscParam[MAX_PARAMS];
	OscController* oscControllers[MAX_PARAMS];
	ParamHandleIndicator paramHandleIndicator[MAX_PARAMS];

	/** Channel ID of the learning session */
	int learningId;
	/** Whether multiple slots or just one slot should be learned */
	bool learnSingleSlot = false;
	/** Whether the controllerId has been set during the learning session */
	bool learnedControllerId;
	int learnedControllerIdLast = -1;
	std::string lastLearnedAddress = "";
	/** Whether the param has been set during the learning session */
	bool learnedParam;
	bool textScrolling = true;
	bool locked;
	NVGcolor mappingIndicatorColor = nvgRGB(0x2f, 0xa5, 0xff);
	bool mappingIndicatorHidden = false;
	uint32_t ts = 0;

	OSCMODE oscMode = OSCMODE::OSCMODE_DEFAULT;
	bool oscResendPeriodically;
	dsp::ClockDivider oscResendDivider;
	dsp::ClockDivider processDivider;
	dsp::ClockDivider lightDivider;
	int processDivision;
	dsp::ClockDivider indicatorDivider;

	std::map<std::pair<std::string, std::string>, MeowMory> meowMoryStorage;
	int meowMoryModuleId = -1;
	std::string contextLabel = "";

	bool receiving;
	bool sending;
	bool oscTriggerNext;
	bool oscTriggerPrev;
	bool oscReceived = false;
	bool oscSent = false;

	dsp::BooleanTrigger connectTrigger;
	dsp::SchmittTrigger meowMoryPrevTrigger;
	dsp::SchmittTrigger meowMoryNextTrigger;
	dsp::SchmittTrigger meowMoryParamTrigger;

	OscelotModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PARAM_RECV, 0.0f, 1.0f, 0.0f, "Enable Receiver");
		configParam(PARAM_SEND, 0.0f, 1.0f, 0.0f, "Enable Sender");
		configParam(PARAM_PREV, 0.f, 1.f, 0.f, "Scan for previous module mapping");
		configParam(PARAM_NEXT, 0.f, 1.f, 0.f, "Scan for next module mapping");
		configParam(PARAM_APPLY, 0.f, 1.f, 0.f, "Apply mapping");

		for (int id = 0; id < MAX_PARAMS; id++) {
			paramHandleIndicator[id].color = mappingIndicatorColor;
			paramHandleIndicator[id].handle = &paramHandles[id];
			APP->engine->addParamHandle(&paramHandles[id]);
			oscParam[id].setLimits(0.0f, 1.0f, -1.0f);
		}
		indicatorDivider.setDivision(2048);
		lightDivider.setDivision(2048);
		oscResendDivider.setDivision(APP->engine->getSampleRate());
		onReset();
	}

	~OscelotModule() {
		for (int id = 0; id < MAX_PARAMS; id++) {
			APP->engine->removeParamHandle(&paramHandles[id]);
		}
	}

	void resetMapMemory() { meowMoryStorage.clear(); }

	void onReset() override {
		receiving = false;
		sending = false;
		receiverPower();
		senderPower();
		learningId = -1;
		learnedControllerId = false;
		learnedParam = false;
		clearMaps();
		mapLen = 1;
		for (int i = 0; i < MAX_PARAMS; i++) {
			oscControllers[i] = nullptr;
			textLabels[i] = "";
		}
		locked = false;
		oscIgnoreDevices = false;
		oscResendPeriodically = false;
		oscResendDivider.reset();
		processDivision = 64;
		processDivider.setDivision(processDivision);
		processDivider.reset();
		clearMapsOnLoad = false;
	}

	void onSampleRateChange() override { oscResendDivider.setDivision(APP->engine->getSampleRate() / 2); }

	bool isValidPort(std::string port) {
		bool isValid = false;
		try {
			if (port.length() > 0) {
				int portNumber = std::stoi(port);
				isValid = portNumber > 1023 && portNumber <= 65535;
			}
		} catch (const std::exception& ex) {
			isValid = false;
		}
		return isValid;
	}

	void receiverPower() {
		if (receiving) {
			if (!isValidPort(rxPort)) rxPort = RXPORT_DEFAULT;
			int port = std::stoi(rxPort);
			receiving = oscReceiver.start(port);
			if (receiving) INFO("Started OSC Receiver on port: %i", port);
		} else {
			oscReceiver.stop();
		}
	}

	void senderPower() {
		if (sending) {
			if (!isValidPort(txPort)) txPort = TXPORT_DEFAULT;
			int port = std::stoi(txPort);
			sending = oscSender.start(ip, port);
			if (sending) {
				INFO("Started OSC Sender on port: %i", port);
				oscResendFeedback();
			}
		} else {
			oscSender.stop();
		}
	}

	void sendOscFeedback(std::string address, int controllerId, float value, std::list<std::string> info) {
		OscBundle feedbackBundle;
		OscMessage valueMessage;
		OscMessage infoMessage;

		valueMessage.setAddress(address);
		valueMessage.addIntArg(controllerId);
		valueMessage.addFloatArg(value);

		infoMessage.setAddress(address + "/info");
		infoMessage.addIntArg(controllerId);
		for (auto&& s : info) {
			infoMessage.addStringArg(s);
		}

		feedbackBundle.addMessage(valueMessage);
		feedbackBundle.addMessage(infoMessage);
		oscSender.sendBundle(feedbackBundle);
	}

	void process(const ProcessArgs& args) override {
		ts++;
		OscMessage rxMessage;
		while (oscReceiver.shift(&rxMessage)) {
			oscReceived = processOscMessage(rxMessage);
		}

		// Process trigger
		if (lightDivider.process() || oscReceived) {
			if (receiving) {
				if (oscReceived) {
					// Blue
					lights[LIGHT_RECV].setBrightness(0.0f);
					lights[LIGHT_RECV + 1].setBrightness(0.0f);
					lights[LIGHT_RECV + 2].setBrightness(1.0f);
				} else {
					// Green
					lights[LIGHT_RECV].setBrightness(0.0f);
					lights[LIGHT_RECV + 1].setBrightness(1.0f);
					lights[LIGHT_RECV + 2].setBrightness(0.0f);
				}
			} else {
				// Orange
				lights[LIGHT_RECV].setBrightness(1.0f);
				lights[LIGHT_RECV + 1].setBrightness(0.4f);
				lights[LIGHT_RECV + 2].setBrightness(0.0f);
			}

			if (sending) {
				if (oscSent) {
					// Blue
					lights[LIGHT_SEND].setBrightness(0.0f);
					lights[LIGHT_SEND + 1].setBrightness(0.0f);
					lights[LIGHT_SEND + 2].setBrightness(1.0f);
					oscSent = false;
				} else {
					// Green
					lights[LIGHT_SEND].setBrightness(0.0f);
					lights[LIGHT_SEND + 1].setBrightness(1.0f);
					lights[LIGHT_SEND + 2].setBrightness(0.0f);
				}
			} else {
				// Orange
				lights[LIGHT_SEND].setBrightness(1.0f);
				lights[LIGHT_SEND + 1].setBrightness(0.4f);
				lights[LIGHT_SEND + 2].setBrightness(0.0f);
			}
		}

		// Only step channels when some osc event has been received. Additionally
		// step channels for parameter changes made manually every 128th loop. 
		if (processDivider.process() || oscReceived) {
			// Step channels
			for (int id = 0; id < mapLen; id++) {
				if (!oscControllers[id]) continue;
				int controllerId = oscControllers[id]->getControllerId();

				// Get Module
				Module* module = paramHandles[id].module;
				if (!module) continue;

				// Get ParamQuantity
				int paramId = paramHandles[id].paramId;
				ParamQuantity* paramQuantity = module->paramQuantities[paramId];
				if (!paramQuantity) continue;

				if (!paramQuantity->isBounded()) continue;

				switch (oscMode) {
				case OSCMODE::OSCMODE_DEFAULT: {
					oscParam[id].paramQuantity = paramQuantity;
					float t = -1.0f;

					// Check if controllerId value has been set and changed
					if (controllerId >= 0 && oscReceived) {
						switch (oscControllers[id]->getControllerMode()) {
						case CONTROLLERMODE::DIRECT:
							if (oscControllers[id]->getValueIn() != oscControllers[id]->getValue()) {
								oscControllers[id]->setValueIn(oscControllers[id]->getValue());
								t = oscControllers[id]->getValue();
							}
							break;
						case CONTROLLERMODE::PICKUP1:
							if (oscControllers[id]->getValueIn() != oscControllers[id]->getValue()) {
								if (oscParam[id].isNear(oscControllers[id]->getValueIn())) {
									t = oscControllers[id]->getValue();
								}
								oscControllers[id]->setValueIn(oscControllers[id]->getValue());
							}
							break;
						case CONTROLLERMODE::PICKUP2:
							if (oscControllers[id]->getValueIn() != oscControllers[id]->getValue()) {
								if (oscParam[id].isNear(oscControllers[id]->getValueIn(), oscControllers[id]->getValue())) {
									t = oscControllers[id]->getValue();
								}
								oscControllers[id]->setValueIn(oscControllers[id]->getValue());
							}
							break;
						case CONTROLLERMODE::TOGGLE:
							if (oscControllers[id]->getValue() > 0 && (oscControllers[id]->getValueIn() == -1.f || oscControllers[id]->getValueIn() >= 0.f)) {
								t = oscParam[id].getLimitMax();
								oscControllers[id]->setValueIn(-2.f);
							} else if (oscControllers[id]->getValue() == 0.f && oscControllers[id]->getValueIn() == -2.f) {
								t = oscParam[id].getLimitMax();
								oscControllers[id]->setValueIn(-3.f);
							} else if (oscControllers[id]->getValue() > 0.f && oscControllers[id]->getValueIn() == -3.f) {
								t = oscParam[id].getLimitMin();
								oscControllers[id]->setValueIn(-4.f);
							} else if (oscControllers[id]->getValue() == 0.f && oscControllers[id]->getValueIn() == -4.f) {
								t = oscParam[id].getLimitMin();
								oscControllers[id]->setValueIn(-1.f);
							}
							break;
						case CONTROLLERMODE::TOGGLE_VALUE:
							if (oscControllers[id]->getValue() > 0 && (oscControllers[id]->getValueIn() == -1.f || oscControllers[id]->getValueIn() >= 0.f)) {
								t = oscControllers[id]->getValue();
								oscControllers[id]->setValueIn(-2.f);
							} else if (oscControllers[id]->getValue() == 0.f && oscControllers[id]->getValueIn() == -2.f) {
								t = oscParam[id].getValue();
								oscControllers[id]->setValueIn(-3.f);
							} else if (oscControllers[id]->getValue() > 0.f && oscControllers[id]->getValueIn() == -3.f) {
								t = oscParam[id].getLimitMin();
								oscControllers[id]->setValueIn(-4.f);
							} else if (oscControllers[id]->getValue() == 0.f && oscControllers[id]->getValueIn() == -4.f) {
								t = oscParam[id].getLimitMin();
								oscControllers[id]->setValueIn(-1.f);
							}
							break;
						}
					}

					// Set a new value for the mapped parameter
					if (t >= 0.f) {
						oscParam[id].setValue(t);
					}

					// Apply value on the mapped parameter (respecting slew and scale)
					oscParam[id].process(args.sampleTime * float(processDivision));

					// Retrieve the current value of the parameter (ignoring slew and scale)
					float v = oscParam[id].getValue();

					// OSC feedback
					if (oscControllers[id]->getValueOut() != v) {
						if (controllerId >= 0 && oscControllers[id]->getControllerMode() == CONTROLLERMODE::DIRECT) oscControllers[id]->setValueIn(v);
						if (sending) {
							sendOscFeedback(oscControllers[id]->getAddress(), oscControllers[id]->getControllerId(), v, getParamInfo(id));
							oscSent = true;
						}
						oscControllers[id]->setValue(v, 0);
						oscControllers[id]->setValueOut(v);
					}
				} break;

				case OSCMODE::OSCMODE_LOCATE: {
					bool indicate = false;
					if ((controllerId >= 0 && oscControllers[id]->getValue() >= 0) && oscControllers[id]->getValueIndicate() != oscControllers[id]->getValue()) {
						oscControllers[id]->setValueIndicate(oscControllers[id]->getValue());
						indicate = true;
					}
					if (indicate) {
						ModuleWidget* mw = APP->scene->rack->getModule(paramQuantity->module->id);
						paramHandleIndicator[id].indicate(mw);
					}
				} break;
				}
			}
		}
		oscReceived = false;

		if (indicatorDivider.process()) {
			float t = indicatorDivider.getDivision() * args.sampleTime;
			for (int i = 0; i < mapLen; i++) {
				paramHandleIndicator[i].color = mappingIndicatorHidden ? color::BLACK_TRANSPARENT : mappingIndicatorColor;
				if (paramHandles[i].moduleId >= 0) {
					paramHandleIndicator[i].process(t, learningId == i);
				}
			}
		}

		if (oscResendPeriodically && oscResendDivider.process()) {
			oscResendFeedback();
		}
	}

	std::list<std::string> getParamInfo(int id) {
		std::list<std::string> s;
		if (id >= mapLen) return s;
		if (paramHandles[id].moduleId < 0) return s;

		ModuleWidget* mw = APP->scene->rack->getModule(paramHandles[id].moduleId);
		if (!mw) return s;

		Module* m = mw->module;
		if (!m) return s;

		int paramId = paramHandles[id].paramId;
		if (paramId >= (int)m->params.size()) return s;
		
		ParamQuantity* paramQuantity = m->paramQuantities[paramId];
		s.push_back(mw->model->name);
		s.push_back(paramQuantity->label);
		s.push_back(paramQuantity->getDisplayValueString());
		s.push_back(paramQuantity->getUnit());
		return s;
	}

	void setMode(OSCMODE oscMode) {
		if (this->oscMode == oscMode) return;
		this->oscMode = oscMode;
		switch (oscMode) {
		case OSCMODE::OSCMODE_LOCATE:
			for (int i = 0; i < MAX_PARAMS; i++)
				if (oscControllers[id]) oscControllers[id]->setValueIndicate(std::fmax(0, oscControllers[i]->getValueIn()));
			break;
		default:
			break;
		}
	}

	bool processOscMessage(OscMessage msg) {
		std::string address = msg.getAddress();
		bool oscReceived = false;

		// Check for OSC triggers
		if (address == "/oscelot/next") {
			oscTriggerNext = true;
			return oscReceived;
		} else if (address == "/oscelot/prev") {
			oscTriggerPrev = true;
			return oscReceived;
		} else if (msg.getNumArgs() < 2) {
			WARN("Discarding OSC message. Need 2 args: id(int) and value(float). OSC message had address: %s and %i args", msg.getAddress().c_str(), msg.getNumArgs());
			return oscReceived;
		}

		uint8_t controllerId = msg.getArgAsInt(0);
		float value = msg.getArgAsFloat(1);
		// Learn
		if (learningId >= 0 && (learnedControllerIdLast != controllerId || lastLearnedAddress != address)) {
			oscControllers[learningId] = OscController::Create(address, controllerId, CONTROLLERMODE::DIRECT, value, ts);
			if (oscControllers[learningId]) {
				learnedControllerId = true;
				lastLearnedAddress = address;
				learnedControllerIdLast = controllerId;
				commitLearn();
				updateMapLen();
			}
		} else {
			for (int id = 0; id < mapLen; id++) {
				if (oscControllers[id] && (oscControllers[id]->getControllerId() == controllerId && oscControllers[id]->getAddress() == address)) {
					oscReceived = true;
					oscControllers[id]->setValue(value, ts);
					return oscReceived;
				}
			}
		}
		return oscReceived;
	}

	void oscResendFeedback() {
		for (int i = 0; i < MAX_PARAMS; i++) {
			if (oscControllers[i]) {
				oscControllers[i]->setValueOut(-1.f);
			}
		}
	}

	void clearMap(int id, bool oscOnly = false) {
		learningId = -1;
		oscParam[id].reset();
		oscControllers[id] = nullptr;
		if (!oscOnly) {
			textLabels[id] = "";
			APP->engine->updateParamHandle(&paramHandles[id], -1, 0, true);
			updateMapLen();
		}
	}

	void clearMaps() {
		learningId = -1;
		for (int id = 0; id < MAX_PARAMS; id++) {
			textLabels[id] = "";
			oscParam[id].reset();
			oscControllers[id] = nullptr;
			APP->engine->updateParamHandle(&paramHandles[id], -1, 0, true);
		}
		mapLen = 1;
		meowMoryModuleId = -1;
	}

	void updateMapLen() {
		// Find last nonempty map
		int id;
		for (id = MAX_PARAMS - 1; id >= 0; id--) {
			if (paramHandles[id].moduleId >= 0 || oscControllers[id]) break;
		}
		mapLen = id + 1;
		// Add an empty "Mapping..." slot
		if (mapLen < MAX_PARAMS) {
			mapLen++;
		}
	}

	void commitLearn() {
		if (learningId < 0) return;
		if (!learnedControllerId) return;
		if (!learnedParam && paramHandles[learningId].moduleId < 0) return;
		// Reset learned state
		learnedControllerId = false;
		learnedParam = false;
		// Copy mode and sensitivity from the previous slot
		if (learningId > 0 && oscControllers[learningId - 1]) {
			if (oscControllers[learningId - 1]->getSensitivity() != OscController::ENCODER_DEFAULT_SENSITIVITY) {
				oscControllers[learningId]->setSensitivity(oscControllers[learningId - 1]->getSensitivity());
			}
			oscControllers[learningId]->setControllerMode(oscControllers[learningId - 1]->getControllerMode());
		}

		// Find next incomplete map
		while (!learnSingleSlot && ++learningId < MAX_PARAMS) {
			if (!oscControllers[learningId] || paramHandles[learningId].moduleId < 0) return;
		}
		learningId = -1;
	}

	int enableLearn(int id, bool learnSingle = false) {
		if (id == -1) {
			// Find next incomplete map
			while (++id < MAX_PARAMS) {
				if (!oscControllers[id] && paramHandles[id].moduleId < 0) break;
			}
			if (id == MAX_PARAMS) {
				return -1;
			}
		}

		if (id == mapLen) {
			disableLearn();
			return -1;
		}
		if (learningId != id) {
			learningId = id;
			learnedControllerId = false;
			learnedControllerIdLast = -1;
			lastLearnedAddress = "";
			learnedParam = false;
			learnSingleSlot = learnSingle;
		}
		return id;
	}

	void disableLearn() { learningId = -1; }

	void disableLearn(int id) {
		if (learningId == id) {
			learningId = -1;
		}
	}

	void learnParam(int id, int moduleId, int paramId) {
		APP->engine->updateParamHandle(&paramHandles[id], moduleId, paramId, true);
		textLabels[id] = "";
		oscParam[id].reset();
		learnedParam = true;
		commitLearn();
		updateMapLen();
	}

	void moduleBind(Module* m, bool keepOscMappings) {
		if (!m) return;
		if (!keepOscMappings) {
			clearMaps();
		} else {
			// Clean up some additional mappings on the end
			for (int i = int(m->params.size()); i < mapLen; i++) {
				textLabels[i] = "";
				APP->engine->updateParamHandle(&paramHandles[i], -1, -1, true);
			}
		}
		for (size_t i = 0; i < m->params.size() && i < MAX_PARAMS; i++) {
			learnParam(int(i), m->id, int(i));
		}

		updateMapLen();
	}

	void meowMorySave(std::string pluginSlug, std::string moduleSlug) {
		MeowMory meowMory = MeowMory();
		Module* module = NULL;
		for (size_t i = 0; i < MAX_PARAMS; i++) {
			if (paramHandles[i].moduleId < 0) continue;
			if (paramHandles[i].module->model->plugin->slug != pluginSlug && paramHandles[i].module->model->slug == moduleSlug) continue;
			module = paramHandles[i].module;

			MeowMoryParam meowMoryParam = MeowMoryParam();
			meowMoryParam.paramId = paramHandles[i].paramId;
			meowMoryParam.controllerId = oscControllers[i] ? oscControllers[i]->getControllerId() : -1;
			meowMoryParam.address = oscControllers[i] ? oscControllers[i]->getAddress() : "";
			meowMoryParam.controllerMode = oscControllers[i] ? oscControllers[i]->getControllerMode() : CONTROLLERMODE::DIRECT;
			if (oscControllers[i] && oscControllers[i]->getSensitivity() != OscController::ENCODER_DEFAULT_SENSITIVITY) meowMoryParam.encSensitivity = oscControllers[i]->getSensitivity();
			meowMoryParam.label = textLabels[i];
			meowMory.paramMap.push_back(meowMoryParam);
		}
		meowMory.pluginName = module->model->plugin->name;
		meowMory.moduleName = module->model->name;
		meowMoryStorage[std::pair<std::string, std::string>(pluginSlug, moduleSlug)] = meowMory;
	}

	void meowMoryDelete(std::string pluginSlug, std::string moduleSlug) { meowMoryStorage.erase(std::pair<std::string, std::string>(pluginSlug, moduleSlug)); }

	void meowMoryApply(Module* m) {
		if (!m) return;
		auto key = std::pair<std::string, std::string>(m->model->plugin->slug, m->model->slug);
		auto it = meowMoryStorage.find(key);
		if (it == meowMoryStorage.end()) return;
		MeowMory meowMory = it->second;

		clearMaps();
		meowMoryModuleId = m->id;
		int i = 0;
		for (MeowMoryParam meowMoryParam : meowMory.paramMap) {
			learnParam(i, m->id, meowMoryParam.paramId);
			if (meowMoryParam.controllerId >= 0) {
				oscControllers[i] = OscController::Create(meowMoryParam.address, meowMoryParam.controllerId, meowMoryParam.controllerMode);
				if(meowMoryParam.encSensitivity) oscControllers[i]->setSensitivity(meowMoryParam.encSensitivity);
			}
			if (meowMoryParam.label != "") textLabels[i] = meowMoryParam.label;
			i++;
		}
		updateMapLen();
	}

	bool meowMoryTest(Module* m) {
		if (!m) return false;
		auto p = std::pair<std::string, std::string>(m->model->plugin->slug, m->model->slug);
		auto it = meowMoryStorage.find(p);
		if (it == meowMoryStorage.end()) return false;
		return true;
	}

	void setProcessDivision(int d) {
		processDivision = d;
		processDivider.setDivision(d);
		processDivider.reset();
		lightDivider.setDivision(2048);
		lightDivider.reset();
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));
		json_object_set_new(rootJ, "receiving", json_boolean(receiving));
		json_object_set_new(rootJ, "sending", json_boolean(sending));
		json_object_set_new(rootJ, "ip", json_string(ip.c_str()));
		json_object_set_new(rootJ, "txPort", json_string(txPort.c_str()));
		json_object_set_new(rootJ, "rxPort", json_string(rxPort.c_str()));
		json_object_set_new(rootJ, "contextLabel", json_string(contextLabel.c_str()));
		json_object_set_new(rootJ, "textScrolling", json_boolean(textScrolling));
		json_object_set_new(rootJ, "mappingIndicatorHidden", json_boolean(mappingIndicatorHidden));
		json_object_set_new(rootJ, "locked", json_boolean(locked));
		json_object_set_new(rootJ, "processDivision", json_integer(processDivision));
		json_object_set_new(rootJ, "clearMapsOnLoad", json_boolean(clearMapsOnLoad));
		json_object_set_new(rootJ, "oscResendPeriodically", json_boolean(oscResendPeriodically));
		json_object_set_new(rootJ, "oscIgnoreDevices", json_boolean(oscIgnoreDevices));

		json_t* mapsJ = json_array();
		for (int id = 0; id < mapLen; id++) {
			json_t* mapJ = json_object();
			json_object_set_new(mapJ, "moduleId", json_integer(paramHandles[id].moduleId));
			json_object_set_new(mapJ, "paramId", json_integer(paramHandles[id].paramId));
			if (textLabels[id] != "") json_object_set_new(mapJ, "label", json_string(textLabels[id].c_str()));
			json_array_append_new(mapsJ, mapJ);
			if (oscControllers[id]) {
				json_object_set_new(mapJ, "controllerId", json_integer(oscControllers[id]->getControllerId()));
				json_object_set_new(mapJ, "controllerMode", json_integer((int)oscControllers[id]->getControllerMode()));
				json_object_set_new(mapJ, "address", json_string(oscControllers[id]->getAddress().c_str()));
				if (oscControllers[id]->getSensitivity() != OscController::ENCODER_DEFAULT_SENSITIVITY) json_object_set_new(mapJ, "encSensitivity", json_integer(oscControllers[id]->getSensitivity()));
			}
		}
		json_object_set_new(rootJ, "maps", mapsJ);
		json_t* meowMoryMapJ = json_array();
		for (auto it : meowMoryStorage) {
			json_t* meowMoryJJ = json_object();
			json_object_set_new(meowMoryJJ, "pluginSlug", json_string(it.first.first.c_str()));
			json_object_set_new(meowMoryJJ, "moduleSlug", json_string(it.first.second.c_str()));

			auto a = it.second;
			json_object_set_new(meowMoryJJ, "pluginName", json_string(a.pluginName.c_str()));
			json_object_set_new(meowMoryJJ, "moduleName", json_string(a.moduleName.c_str()));
			json_t* paramMapJ = json_array();
			for (auto p : a.paramMap) {
				json_t* paramMapJJ = json_object();
				json_object_set_new(paramMapJJ, "paramId", json_integer(p.paramId));
				if (p.controllerId != -1) {
					json_object_set_new(paramMapJJ, "controllerId", json_integer(p.controllerId));
					json_object_set_new(paramMapJJ, "address", json_string(p.address.c_str()));
					json_object_set_new(paramMapJJ, "controllerMode", json_integer((int)p.controllerMode));
					if (p.encSensitivity != OscController::ENCODER_DEFAULT_SENSITIVITY) json_object_set_new(paramMapJJ, "encSensitivity", json_integer((int)p.encSensitivity));
				}
				if (p.label != "") json_object_set_new(paramMapJJ, "label", json_string(p.label.c_str()));
				json_array_append_new(paramMapJ, paramMapJJ);
			}
			json_object_set_new(meowMoryJJ, "paramMap", paramMapJ);

			json_array_append_new(meowMoryMapJ, meowMoryJJ);
		}
		json_object_set_new(rootJ, "meowMory", meowMoryMapJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* panelThemeJ = json_object_get(rootJ, "panelTheme");
		json_t* oscResendPeriodicallyJ = json_object_get(rootJ, "oscResendPeriodically");
		json_t* contextLabelJ = json_object_get(rootJ, "contextLabel");
		json_t* textScrollingJ = json_object_get(rootJ, "textScrolling");
		json_t* mappingIndicatorHiddenJ = json_object_get(rootJ, "mappingIndicatorHidden");
		json_t* lockedJ = json_object_get(rootJ, "locked");
		json_t* processDivisionJ = json_object_get(rootJ, "processDivision");
		json_t* clearMapsOnLoadJ = json_object_get(rootJ, "clearMapsOnLoad");
		json_t* mapsJ = json_object_get(rootJ, "maps");

		if (panelThemeJ) panelTheme = json_integer_value(panelThemeJ);
		if (oscResendPeriodicallyJ) oscResendPeriodically = json_boolean_value(oscResendPeriodicallyJ);
		if (contextLabelJ) contextLabel = json_string_value(contextLabelJ);
		if (textScrollingJ) textScrolling = json_boolean_value(textScrollingJ);
		if (mappingIndicatorHiddenJ) mappingIndicatorHidden = json_boolean_value(mappingIndicatorHiddenJ);
		if (lockedJ) locked = json_boolean_value(lockedJ);
		if (processDivisionJ) processDivision = json_integer_value(processDivisionJ);
		if (clearMapsOnLoadJ) clearMapsOnLoad = json_boolean_value(clearMapsOnLoadJ);
		if (clearMapsOnLoad) clearMaps();

		if (mapsJ) {
			json_t* mapElement;
			size_t mapIndex;
			json_array_foreach(mapsJ, mapIndex, mapElement) {
				if (mapIndex >= MAX_PARAMS) {
					continue;
				}
				json_t* controllerIdJ = json_object_get(mapElement, "controllerId");
				json_t* moduleIdJ = json_object_get(mapElement, "moduleId");
				json_t* encSensitivityJ = json_object_get(mapElement, "encSensitivity");
				json_t* paramIdJ = json_object_get(mapElement, "paramId");
				json_t* labelJ = json_object_get(mapElement, "label");

				if (!(moduleIdJ || paramIdJ)) {
					APP->engine->updateParamHandle(&paramHandles[mapIndex], -1, 0, true);
				}
				if (controllerIdJ) {
					std::string address = json_string_value(json_object_get(mapElement, "address"));
					CONTROLLERMODE controllerMode = (CONTROLLERMODE)json_integer_value(json_object_get(mapElement, "controllerMode"));
					oscControllers[mapIndex] = OscController::Create(address, json_integer_value(controllerIdJ), controllerMode);
					if (encSensitivityJ)
						oscControllers[mapIndex]->setSensitivity(json_integer_value(encSensitivityJ));
				}
				if (labelJ) textLabels[mapIndex] = json_string_value(labelJ);

				int moduleId = moduleIdJ ? json_integer_value(moduleIdJ) : -1;
				int paramId = paramIdJ ? json_integer_value(paramIdJ) : 0;
				if (moduleId >= 0) {
					if (moduleId != paramHandles[mapIndex].moduleId || paramId != paramHandles[mapIndex].paramId) {
						APP->engine->updateParamHandle(&paramHandles[mapIndex], moduleId, paramId, false);
					}
				}
			}
		}
		updateMapLen();

		if (!oscIgnoreDevices) {
			json_t* oscIgnoreDevicesJ = json_object_get(rootJ, "oscIgnoreDevices");
			json_t* ipJ = json_object_get(rootJ, "ip");
			json_t* txPortJ = json_object_get(rootJ, "txPort");
			json_t* rxPortJ = json_object_get(rootJ, "rxPort");
			json_t* stateRJ = json_object_get(rootJ, "receiving");
			json_t* stateSJ = json_object_get(rootJ, "sending");

			if (oscIgnoreDevicesJ) oscIgnoreDevices = json_boolean_value(oscIgnoreDevicesJ);
			if (stateRJ) receiving = json_boolean_value(stateRJ);
			if (stateSJ) sending = json_boolean_value(stateSJ);
			if (ipJ) ip = json_string_value(ipJ);
			if (txPortJ) txPort = json_string_value(txPortJ);
			if (rxPortJ) rxPort = json_string_value(rxPortJ);
			receiverPower();
			senderPower();
		}

		resetMapMemory();
		json_t* meowMoryStorageJ = json_object_get(rootJ, "meowMory");
		size_t i;
		json_t* meowMoryEntry;
		json_array_foreach(meowMoryStorageJ, i, meowMoryEntry) {
			std::string pluginSlug = json_string_value(json_object_get(meowMoryEntry, "pluginSlug"));
			std::string moduleSlug = json_string_value(json_object_get(meowMoryEntry, "moduleSlug"));
			MeowMory meowMory = MeowMory();
			meowMory.pluginName = json_string_value(json_object_get(meowMoryEntry, "pluginName"));
			meowMory.moduleName = json_string_value(json_object_get(meowMoryEntry, "moduleName"));
			json_t* paramMapJ = json_object_get(meowMoryEntry, "paramMap");
			size_t j;
			json_t* meowMoryElement;
			json_array_foreach(paramMapJ, j, meowMoryElement) {
				json_t* controllerIdJ = json_object_get(meowMoryElement, "controllerId");
				json_t* encSensitivityJ = json_object_get(meowMoryElement, "encSensitivity");
				json_t* labelJ = json_object_get(meowMoryElement, "label");
				MeowMoryParam meowMoryParam = MeowMoryParam();
				meowMoryParam.paramId = json_integer_value(json_object_get(meowMoryElement, "paramId"));
				meowMoryParam.controllerId = controllerIdJ ? json_integer_value(controllerIdJ) : -1;
				meowMoryParam.address = controllerIdJ ? json_string_value(json_object_get(meowMoryElement, "address")) : "";
				meowMoryParam.controllerMode = controllerIdJ ? (CONTROLLERMODE)json_integer_value(json_object_get(meowMoryElement, "controllerMode")) : CONTROLLERMODE::DIRECT;
				if (encSensitivityJ) meowMoryParam.encSensitivity = json_integer_value(encSensitivityJ);
				meowMoryParam.label = labelJ ? json_string_value(labelJ) : "";
				meowMory.paramMap.push_back(meowMoryParam);
			}
			meowMoryStorage[std::pair<std::string, std::string>(pluginSlug, moduleSlug)] = meowMory;
		}
	}
};

struct OscelotChoice : MapModuleChoice<MAX_PARAMS, OscelotModule> {
	OscelotChoice() {
		textOffset = Vec(6.f, 14.7f);
		color = nvgRGB(0xfe, 0xff, 0xe0);
	}

	std::string getSlotPrefix() override {
		if (module->oscControllers[id]) {
			return string::f("%s-%02d | ", module->oscControllers[id]->getTypeString(), module->oscControllers[id]->getControllerId());
		} else if (module->paramHandles[id].moduleId >= 0) {
			return ".... ";
		} else {
			return "";
		}
	}

	std::string getSlotLabel() override { return module->textLabels[id]; }

	void appendContextMenu(Menu* menu) override {
		struct UnmapOSCItem : MenuItem {
			OscelotModule* module;
			int id;
			void onAction(const event::Action& e) override { module->clearMap(id, true); }
		};  // struct UnmapOSCItem

		struct EncoderMenuItem : MenuItem {
			OscelotModule* module;
			int id;
			EncoderMenuItem() { rightText = RIGHT_ARROW; }

			struct LabelField : ui::TextField {
				OscelotModule* module;
				int id;
				void onSelectKey(const event::SelectKey& e) override {
					if (e.action == GLFW_PRESS && e.key == GLFW_KEY_ENTER) {
						module->oscControllers[id]->setSensitivity(std::stoi(text));

						ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
						overlay->requestDelete();
						e.consume(this);
					}

					if (!e.getTarget()) {
						ui::TextField::onSelectKey(e);
					}
				}
			};

			struct ResetItem : ui::MenuItem {
				OscelotModule* module;
				int id;
				void onAction(const event::Action& e) override { module->oscControllers[id]->setSensitivity(OscController::ENCODER_DEFAULT_SENSITIVITY); }
			};

			Menu* createChildMenu() override {
				Menu* menu = new Menu;

				LabelField* labelField = new LabelField;
				labelField->box.size.x = 60;
				labelField->module = module;
				labelField->text = std::to_string(module->oscControllers[id]->getSensitivity());
				labelField->id = id;
				menu->addChild(labelField);

				ResetItem* resetItem = new ResetItem;
				resetItem->text = "Reset";
				resetItem->module = module;
				resetItem->id = id;
				menu->addChild(resetItem);

				return menu;
			}
		};  // struct EncoderMenuItem

		struct ControllerModeMenuItem : MenuItem {
			OscelotModule* module;
			int id;

			ControllerModeMenuItem() { rightText = RIGHT_ARROW; }

			struct ControllerModeItem : MenuItem {
				OscelotModule* module;
				int id;
				CONTROLLERMODE controllerMode;

				void onAction(const event::Action& e) override { module->oscControllers[id]->setControllerMode(controllerMode); }
				void step() override {
					rightText = module->oscControllers[id]->getControllerMode() == controllerMode ? "✔" : "";
					MenuItem::step();
				}
			};

			Menu* createChildMenu() override {
				Menu* menu = new Menu;
				menu->addChild(construct<ControllerModeItem>(&MenuItem::text, "Direct", &ControllerModeItem::module, module, &ControllerModeItem::id, id,
				                                             &ControllerModeItem::controllerMode, CONTROLLERMODE::DIRECT));
				menu->addChild(construct<ControllerModeItem>(&MenuItem::text, "Pickup (snap)", &ControllerModeItem::module, module, &ControllerModeItem::id, id,
				                                             &ControllerModeItem::controllerMode, CONTROLLERMODE::PICKUP1));
				menu->addChild(construct<ControllerModeItem>(&MenuItem::text, "Pickup (jump)", &ControllerModeItem::module, module, &ControllerModeItem::id, id,
				                                             &ControllerModeItem::controllerMode, CONTROLLERMODE::PICKUP2));
				menu->addChild(construct<ControllerModeItem>(&MenuItem::text, "Toggle", &ControllerModeItem::module, module, &ControllerModeItem::id, id,
				                                             &ControllerModeItem::controllerMode, CONTROLLERMODE::TOGGLE));
				menu->addChild(construct<ControllerModeItem>(&MenuItem::text, "Toggle + Value", &ControllerModeItem::module, module, &ControllerModeItem::id, id,
				                                             &ControllerModeItem::controllerMode, CONTROLLERMODE::TOGGLE_VALUE));
				return menu;
			}
		};  // struct ControllerModeMenuItem

		if (module->oscControllers[id]) {
			menu->addChild(construct<UnmapOSCItem>(&MenuItem::text, "Clear OSC assignment", &UnmapOSCItem::module, module, &UnmapOSCItem::id, id));
			if (strcmp(module->oscControllers[id]->getTypeString(), "ENC") == 0)
				menu->addChild(construct<EncoderMenuItem>(&MenuItem::text, "Encoder Sensitivity", &EncoderMenuItem::module, module, &EncoderMenuItem::id, id));
			else
				menu->addChild(construct<ControllerModeMenuItem>(&MenuItem::text, "Input mode for Controller", &ControllerModeMenuItem::module, module, &ControllerModeMenuItem::id, id));
		}
	}
};

struct OscWidget : widget::OpaqueWidget {
	OscelotModule* module;
	OscelotTextField* ip;
	OscelotTextField* txPort;
	OscelotTextField* rxPort;
	NVGcolor color = nvgRGB(0xDA, 0xa5, 0x20);
	NVGcolor white = nvgRGB(0xfe, 0xff, 0xe0);

	void step() override {
		if (!module) return;

		ip->step();
		if (ip->isFocused)
			module->ip = ip->text;
		else
			ip->text = module->ip;

		txPort->step();
		if (txPort->isFocused)
			module->txPort = txPort->text;
		else
			txPort->text = module->txPort;

		rxPort->step();
		if (rxPort->isFocused)
			module->rxPort = rxPort->text;
		else
			rxPort->text = module->rxPort;
	}

	void setOSCPort(std::string ipT, std::string rPort, std::string tPort) {
		clearChildren();
		math::Vec pos;

		OscelotTextField* ip = createWidget<OscelotTextField>(pos);
		ip->box.size = mm2px(Vec(32, 5));
		ip->maxTextLength = 15;
		ip->text = ipT;
		addChild(ip);
		this->ip = ip;

		pos = ip->box.getTopRight();
		pos.x = pos.x + 1;
		OscelotTextField* txPort = createWidget<OscelotTextField>(pos);
		txPort->box.size = mm2px(Vec(12.5, 5));
		txPort->text = tPort;
		addChild(txPort);
		this->txPort = txPort;

		pos = txPort->box.getTopRight();
		pos.x = pos.x + 37;
		OscelotTextField* rxPort = createWidget<OscelotTextField>(pos);
		rxPort->box.size = mm2px(Vec(12.5, 5));
		rxPort->text = rPort;
		addChild(rxPort);
		this->rxPort = rxPort;
	}
};

struct OscelotDisplay : MapModuleDisplay<MAX_PARAMS, OscelotModule, OscelotChoice> {
	void step() override {
		if (module) {
			int mapLen = module->mapLen;
			for (int id = 0; id < MAX_PARAMS; id++) {
				choices[id]->visible = (id < mapLen);
			}
		}
		MapModuleDisplay<MAX_PARAMS, OscelotModule, OscelotChoice>::step();
	}
};

struct OscelotWidget : ThemedModuleWidget<OscelotModule>, ParamWidgetContextExtender {
	OscelotModule* module;
	OscelotDisplay* mapWidget;

	dsp::BooleanTrigger receiveTrigger;
	dsp::BooleanTrigger sendTrigger;
	dsp::SchmittTrigger meowMoryPrevTrigger;
	dsp::SchmittTrigger meowMoryNextTrigger;
	dsp::SchmittTrigger meowMoryParamTrigger;

	std::string contextLabel = "";

	enum class LEARN_MODE { OFF = 0, BIND_CLEAR = 1, BIND_KEEP = 2, MEM = 3 };

	LEARN_MODE learnMode = LEARN_MODE::OFF;

	OscelotWidget(OscelotModule* module) : ThemedModuleWidget<OscelotModule>(module, "Oscelot") {
		setModule(module);
		this->module = module;

		addChild(createWidget<PawScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<PawScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<PawScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<PawScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		mapWidget = createWidget<OscelotDisplay>(mm2px(Vec(6, 29)));
		mapWidget->box.size = mm2px(Vec(78, 54));
		mapWidget->setModule(module);
		addChild(mapWidget);

		OscWidget* oscConfigWidget = createWidget<OscWidget>(mm2px(Vec(6, 96)));
		oscConfigWidget->box.size = mm2px(Vec(77, 5));
		oscConfigWidget->module = module;
		if (module) {
			oscConfigWidget->setOSCPort(module ? module->ip : NULL, module ? module->rxPort : NULL, module ? module->txPort : NULL);
		}
		addChild(oscConfigWidget);

		// Send switch
		math::Vec inpPos = mm2px(Vec(54, 98.5));
		addChild(createParamCentered<TL1105>(inpPos, module, OscelotModule::PARAM_SEND));
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(inpPos, module, OscelotModule::LIGHT_SEND));

		// Receive switch
		inpPos = mm2px(Vec(79, 98.5));
		addChild(createParamCentered<TL1105>(inpPos, module, OscelotModule::PARAM_RECV));
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(inpPos, module, OscelotModule::LIGHT_RECV));

		// Eyes
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(19.8, 11.2)), module, OscelotModule::LIGHT_SEND));
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(27, 11.9)), module, OscelotModule::LIGHT_RECV));

		// Memory
		inpPos = mm2px(Vec(27, 114));
		addChild(createParamCentered<PawButton>(inpPos, module, OscelotModule::PARAM_PREV));
		addChild(createLightCentered<PawPrevLight>(inpPos, module, OscelotModule::LIGHT_PREV));

		inpPos = mm2px(Vec(46, 114));
		addChild(createParamCentered<PawButton>(inpPos, module, OscelotModule::PARAM_APPLY));
		addChild(createLightCentered<PawLight>(inpPos, module, OscelotModule::LIGHT_APPLY));

		inpPos = mm2px(Vec(65, 114));
		addChild(createParamCentered<PawButton>(inpPos, module, OscelotModule::PARAM_NEXT));
		addChild(createLightCentered<PawNextLight>(inpPos, module, OscelotModule::LIGHT_NEXT));
	}

	~OscelotWidget() {
		if (learnMode != LEARN_MODE::OFF) {
			glfwSetCursor(APP->window->win, NULL);
		}
	}

	void step() override {
		ThemedModuleWidget<OscelotModule>::step();
		if (module) {
			if (receiveTrigger.process(module->params[OscelotModule::PARAM_RECV].getValue() > 0.0f)) {
				module->receiving ^= true;
				module->receiverPower();
			}

			if (sendTrigger.process(module->params[OscelotModule::PARAM_SEND].getValue() > 0.0f)) {
				module->sending ^= true;
				module->senderPower();
			}

			if (module->oscTriggerPrev || meowMoryPrevTrigger.process(module->params[OscelotModule::PARAM_PREV].getValue())) {
				module->oscTriggerPrev = false;
				meowMoryPrevModule();
			}

			if (module->oscTriggerNext || meowMoryNextTrigger.process(module->params[OscelotModule::PARAM_NEXT].getValue())) {
				module->oscTriggerNext = false;
				meowMoryNextModule();
			}

			if (meowMoryParamTrigger.process(module->params[OscelotModule::PARAM_APPLY].getValue())) {
				enableLearn(LEARN_MODE::MEM);
			}

			module->lights[OscelotModule::LIGHT_APPLY].setBrightness(learnMode == LEARN_MODE::MEM ? 1.0 : 0.0);
			module->lights[OscelotModule::LIGHT_NEXT].setBrightness(module->params[OscelotModule::PARAM_NEXT].getValue() > 0.1 ? 1.0 : 0.0);
			module->lights[OscelotModule::LIGHT_PREV].setBrightness(module->params[OscelotModule::PARAM_PREV].getValue() > 0.1 ? 1.0 : 0.0);

			if (module->contextLabel != contextLabel) {
				contextLabel = module->contextLabel;
			}
		}

		ParamWidgetContextExtender::step();
	}

	void meowMoryPrevModule() {
		std::list<Widget*> modules = APP->scene->rack->moduleContainer->children;
		auto sort = [&](Widget* w1, Widget* w2) {
			auto t1 = std::make_tuple(w1->box.pos.y, w1->box.pos.x);
			auto t2 = std::make_tuple(w2->box.pos.y, w2->box.pos.x);
			return t1 > t2;
		};
		modules.sort(sort);
		meowMoryScanModules(modules);
	}

	void meowMoryNextModule() {
		std::list<Widget*> modules = APP->scene->rack->moduleContainer->children;
		auto sort = [&](Widget* w1, Widget* w2) {
			auto t1 = std::make_tuple(w1->box.pos.y, w1->box.pos.x);
			auto t2 = std::make_tuple(w2->box.pos.y, w2->box.pos.x);
			return t1 < t2;
		};
		modules.sort(sort);
		meowMoryScanModules(modules);
	}

	void meowMoryScanModules(std::list<Widget*>& modules) {
	f:
		std::list<Widget*>::iterator it = modules.begin();
		// Scan for current module in the list
		if (module->meowMoryModuleId != -1) {
			for (; it != modules.end(); it++) {
				ModuleWidget* mw = dynamic_cast<ModuleWidget*>(*it);
				Module* m = mw->module;
				if (m->id == module->meowMoryModuleId) {
					it++;
					break;
				}
			}
			// Module not found
			if (it == modules.end()) {
				it = modules.begin();
			}
		}
		// Scan for next module with stored mapping
		for (; it != modules.end(); it++) {
			ModuleWidget* mw = dynamic_cast<ModuleWidget*>(*it);
			Module* m = mw->module;
			if (module->meowMoryTest(m)) {
				module->meowMoryApply(m);
				return;
			}
		}
		// No module found yet -> retry from the beginning
		if (module->meowMoryModuleId != -1) {
			module->meowMoryModuleId = -1;
			goto f;
		}
	}

	void extendParamWidgetContextMenu(ParamWidget* pw, Menu* menu) override {
		if (!module) return;
		if (module->learningId >= 0) return;
		ParamQuantity* pq = pw->paramQuantity;
		if (!pq) return;

		struct OscelotBeginItem : MenuLabel {
			OscelotBeginItem() { text = "OSC'elot"; }
		};

		struct OscelotEndItem : MenuEntry {
			OscelotEndItem() { box.size = Vec(); }
		};

		struct MapMenuItem : MenuItem {
			OscelotModule* module;
			ParamQuantity* pq;
			int currentId = -1;

			MapMenuItem() { rightText = RIGHT_ARROW; }

			Menu* createChildMenu() override {
				struct MapItem : MenuItem {
					OscelotModule* module;
					int currentId;
					void onAction(const event::Action& e) override { module->enableLearn(currentId, true); }
				};

				struct MapEmptyItem : MenuItem {
					OscelotModule* module;
					ParamQuantity* pq;
					void onAction(const event::Action& e) override {
						int id = module->enableLearn(-1, true);
						if (id >= 0) module->learnParam(id, pq->module->id, pq->paramId);
					}
				};

				struct RemapItem : MenuItem {
					OscelotModule* module;
					ParamQuantity* pq;
					int id;
					int currentId;
					void onAction(const event::Action& e) override { module->learnParam(id, pq->module->id, pq->paramId); }
					void step() override {
						rightText = CHECKMARK(id == currentId);
						MenuItem::step();
					}
				};

				Menu* menu = new Menu;
				if (currentId < 0) {
					menu->addChild(construct<MapEmptyItem>(&MenuItem::text, "Learn OSC", &MapEmptyItem::module, module, &MapEmptyItem::pq, pq));
				} else {
					menu->addChild(construct<MapItem>(&MenuItem::text, "Learn OSC", &MapItem::module, module, &MapItem::currentId, currentId));
				}

				if (module->mapLen > 0) {
					menu->addChild(new MenuSeparator);
					for (int i = 0; i < module->mapLen; i++) {
						if (module->oscControllers[i]) {
							std::string text;
							if (module->textLabels[i] != "") {
								text = module->textLabels[i];
							} else {
								text = string::f("%s-%02d", module->oscControllers[i]->getTypeString(), module->oscControllers[i]->getControllerId());
							}
							menu->addChild(
							    construct<RemapItem>(&MenuItem::text, text, &RemapItem::module, module, &RemapItem::pq, pq, &RemapItem::id, i, &RemapItem::currentId, currentId));
						}
					}
				}
				return menu;
			}
		};

		std::list<Widget*>::iterator beg = menu->children.begin();
		std::list<Widget*>::iterator end = menu->children.end();
		std::list<Widget*>::iterator itCvBegin = end;
		std::list<Widget*>::iterator itCvEnd = end;

		for (auto it = beg; it != end; it++) {
			if (itCvBegin == end) {
				OscelotBeginItem* ml = dynamic_cast<OscelotBeginItem*>(*it);
				if (ml) {
					itCvBegin = it;
					continue;
				}
			} else {
				OscelotEndItem* ml = dynamic_cast<OscelotEndItem*>(*it);
				if (ml) {
					itCvEnd = it;
					break;
				}
			}
		}

		for (int id = 0; id < module->mapLen; id++) {
			if (module->paramHandles[id].moduleId == pq->module->id && module->paramHandles[id].paramId == pq->paramId) {
				std::string oscelotId = contextLabel != "" ? "on \"" + contextLabel + "\"" : "";
				std::list<Widget*> w;
				w.push_back(construct<MapMenuItem>(&MenuItem::text, string::f("Re-map %s", oscelotId.c_str()), &MapMenuItem::module, module, &MapMenuItem::pq, pq,
				                                   &MapMenuItem::currentId, id));
				w.push_back(construct<CenterModuleItem>(&MenuItem::text, "Go to mapping module", &CenterModuleItem::mw, this));
				w.push_back(new OscelotEndItem);

				if (itCvBegin == end) {
					menu->addChild(new MenuSeparator);
					menu->addChild(construct<OscelotBeginItem>());
					for (Widget* wm : w) {
						menu->addChild(wm);
					}
				} else {
					for (auto i = w.rbegin(); i != w.rend(); ++i) {
						Widget* wm = *i;
						menu->addChild(wm);
						auto it = std::prev(menu->children.end());
						menu->children.splice(std::next(itCvBegin), menu->children, it);
					}
				}
				return;
			}
		}

		if (contextLabel != "") {
			std::string oscelotId = contextLabel;
			MenuItem* mapMenuItem = construct<MapMenuItem>(&MenuItem::text, string::f("Map on \"%s\"", contextLabel.c_str()), &MapMenuItem::module, module, &MapMenuItem::pq, pq);
			if (itCvBegin == end) {
				menu->addChild(new MenuSeparator);
				menu->addChild(construct<OscelotBeginItem>());
				menu->addChild(mapMenuItem);
			} else {
				menu->addChild(mapMenuItem);
				auto it = std::find(beg, end, mapMenuItem);
				menu->children.splice(std::next(itCvEnd == end ? itCvBegin : itCvEnd), menu->children, it);
			}
			// }
		}
	}

	void onDeselect(const event::Deselect& e) override {
		ModuleWidget::onDeselect(e);
		if (learnMode != LEARN_MODE::OFF) {
			DEFER({ disableLearn(); });

			// Learn module
			Widget* w = APP->event->getDraggedWidget();
			if (!w) return;
			ModuleWidget* mw = dynamic_cast<ModuleWidget*>(w);
			if (!mw) mw = w->getAncestorOfType<ModuleWidget>();
			if (!mw || mw == this) return;
			Module* m = mw->module;
			if (!m) return;

			OscelotModule* module = dynamic_cast<OscelotModule*>(this->module);
			switch (learnMode) {
			case LEARN_MODE::BIND_CLEAR:
				module->moduleBind(m, false);
				break;
			case LEARN_MODE::BIND_KEEP:
				module->moduleBind(m, true);
				break;
			case LEARN_MODE::MEM:
				module->meowMoryApply(m);
				break;
			case LEARN_MODE::OFF:
				break;
			}
		}
	}

	void onHoverKey(const event::HoverKey& e) override {
		if (e.action == GLFW_PRESS) {
			switch (e.key) {
			case GLFW_KEY_D: {
				if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
					enableLearn(LEARN_MODE::BIND_KEEP);
				}
				if ((e.mods & RACK_MOD_MASK) == (GLFW_MOD_SHIFT | RACK_MOD_CTRL)) {
					enableLearn(LEARN_MODE::BIND_CLEAR);
				}
				break;
			}
			case GLFW_KEY_V: {
				if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
					enableLearn(LEARN_MODE::MEM);
				}
				break;
			}
			case GLFW_KEY_ESCAPE: {
				OscelotModule* module = dynamic_cast<OscelotModule*>(this->module);
				disableLearn();
				module->disableLearn();
				e.consume(this);
				break;
			}
			case GLFW_KEY_SPACE: {
				if (module->learningId >= 0) {
					OscelotModule* module = dynamic_cast<OscelotModule*>(this->module);
					module->enableLearn(module->learningId + 1);
					if (module->learningId == -1) disableLearn();
					e.consume(this);
				}
				break;
			}
			}
		}
		ThemedModuleWidget<OscelotModule>::onHoverKey(e);
	}

	void enableLearn(LEARN_MODE mode) {
		learnMode = learnMode == LEARN_MODE::OFF ? mode : LEARN_MODE::OFF;
		APP->event->setSelected(this);
		GLFWcursor* cursor = NULL;
		if (learnMode != LEARN_MODE::OFF) {
			cursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
		}
		glfwSetCursor(APP->window->win, cursor);
	}

	void disableLearn() {
		learnMode = LEARN_MODE::OFF;
		glfwSetCursor(APP->window->win, NULL);
	}

	void appendContextMenu(Menu* menu) override {
		ThemedModuleWidget<OscelotModule>::appendContextMenu(menu);
		assert(module);

		struct ResendOSCOutItem : MenuItem {
			OscelotModule* module;
			Menu* createChildMenu() override {
				struct NowItem : MenuItem {
					OscelotModule* module;
					void onAction(const event::Action& e) override { module->oscResendFeedback(); }
				};

				struct PeriodicallyItem : MenuItem {
					OscelotModule* module;
					void onAction(const event::Action& e) override { module->oscResendPeriodically ^= true; }
					void step() override {
						rightText = CHECKMARK(module->oscResendPeriodically);
						MenuItem::step();
					}
				};

				Menu* menu = new Menu;
				menu->addChild(construct<NowItem>(&MenuItem::text, "Now", &NowItem::module, module));
				menu->addChild(construct<PeriodicallyItem>(&MenuItem::text, "Periodically", &PeriodicallyItem::module, module));
				return menu;
			}
		};  // struct ResendOSCOutItem

		struct PresetLoadMenuItem : MenuItem {
			struct IgnoreOSCDevicesItem : MenuItem {
				OscelotModule* module;
				void onAction(const event::Action& e) override { module->oscIgnoreDevices ^= true; }
				void step() override {
					rightText = CHECKMARK(module->oscIgnoreDevices);
					MenuItem::step();
				}
			};  // struct IgnoreOSCDevicesItem

			struct ClearMapsOnLoadItem : MenuItem {
				OscelotModule* module;
				void onAction(const event::Action& e) override { module->clearMapsOnLoad ^= true; }
				void step() override {
					rightText = CHECKMARK(module->clearMapsOnLoad);
					MenuItem::step();
				}
			};  // struct ClearMapsOnLoadItem

			OscelotModule* module;
			PresetLoadMenuItem() { rightText = RIGHT_ARROW; }

			Menu* createChildMenu() override {
				Menu* menu = new Menu;
				menu->addChild(construct<IgnoreOSCDevicesItem>(&MenuItem::text, "Ignore OSC devices", &IgnoreOSCDevicesItem::module, module));
				menu->addChild(construct<ClearMapsOnLoadItem>(&MenuItem::text, "Clear mapping slots", &ClearMapsOnLoadItem::module, module));
				return menu;
			}
		};

		struct PrecisionMenuItem : MenuItem {
			struct PrecisionItem : MenuItem {
				OscelotModule* module;
				int sampleRate;
				int division;
				std::string text;
				PrecisionItem() { sampleRate = int(APP->engine->getSampleRate()); }
				void onAction(const event::Action& e) override { module->setProcessDivision(division); }
				void step() override {
					MenuItem::text = string::f("%s (%i Hz)", text.c_str(), sampleRate / division);
					rightText = module->processDivision == division ? "✔" : "";
					MenuItem::step();
				}
			};

			OscelotModule* module;
			PrecisionMenuItem() { rightText = RIGHT_ARROW; }

			Menu* createChildMenu() override {
				Menu* menu = new Menu;
				menu->addChild(construct<PrecisionItem>(&PrecisionItem::text, "Audio rate", &PrecisionItem::module, module, &PrecisionItem::division, 1));
				menu->addChild(construct<PrecisionItem>(&PrecisionItem::text, "Higher CPU", &PrecisionItem::module, module, &PrecisionItem::division, 8));
				menu->addChild(construct<PrecisionItem>(&PrecisionItem::text, "Moderate CPU", &PrecisionItem::module, module, &PrecisionItem::division, 64));
				menu->addChild(construct<PrecisionItem>(&PrecisionItem::text, "Lowest CPU", &PrecisionItem::module, module, &PrecisionItem::division, 256));
				return menu;
			}
		};  // struct PrecisionMenuItem

		struct OSCModeMenuItem : MenuItem {
			OSCModeMenuItem() { rightText = RIGHT_ARROW; }

			struct OSCModeItem : MenuItem {
				OscelotModule* module;
				OSCMODE oscMode;

				void onAction(const event::Action& e) override { module->setMode(oscMode); }
				void step() override {
					rightText = module->oscMode == oscMode ? "✔" : "";
					MenuItem::step();
				}
			};

			OscelotModule* module;
			Menu* createChildMenu() override {
				Menu* menu = new Menu;
				menu->addChild(construct<OSCModeItem>(&MenuItem::text, "Operating", &OSCModeItem::module, module, &OSCModeItem::oscMode, OSCMODE::OSCMODE_DEFAULT));
				menu->addChild(construct<OSCModeItem>(&MenuItem::text, "Locate and indicate", &OSCModeItem::module, module, &OSCModeItem::oscMode, OSCMODE::OSCMODE_LOCATE));
				return menu;
			}
		};  // struct OSCModeMenuItem

		menu->addChild(new MenuSeparator());
		menu->addChild(construct<PresetLoadMenuItem>(&MenuItem::text, "Preset load", &PresetLoadMenuItem::module, module));
		menu->addChild(construct<PrecisionMenuItem>(&MenuItem::text, "Precision", &PrecisionMenuItem::module, module));
		menu->addChild(construct<OSCModeMenuItem>(&MenuItem::text, "Mode", &OSCModeMenuItem::module, module));
		menu->addChild(construct<ResendOSCOutItem>(&MenuItem::text, "Re-send OSC feedback", &MenuItem::rightText, RIGHT_ARROW, &ResendOSCOutItem::module, module));

		struct UiMenuItem : MenuItem {
			OscelotModule* module;
			UiMenuItem() { rightText = RIGHT_ARROW; }

			Menu* createChildMenu() override {
				struct TextScrollItem : MenuItem {
					OscelotModule* module;
					void onAction(const event::Action& e) override { module->textScrolling ^= true; }
					void step() override {
						rightText = module->textScrolling ? "✔" : "";
						MenuItem::step();
					}
				};  // struct TextScrollItem

				struct MappingIndicatorHiddenItem : MenuItem {
					OscelotModule* module;
					void onAction(const event::Action& e) override { module->mappingIndicatorHidden ^= true; }
					void step() override {
						rightText = module->mappingIndicatorHidden ? "✔" : "";
						MenuItem::step();
					}
				};  // struct MappingIndicatorHiddenItem

				struct LockedItem : MenuItem {
					OscelotModule* module;
					void onAction(const event::Action& e) override { module->locked ^= true; }
					void step() override {
						rightText = module->locked ? "✔" : "";
						MenuItem::step();
					}
				};  // struct LockedItem

				struct ContextMenuItem : MenuItem {
					OscelotModule* module;

					ContextMenuItem() { rightText = RIGHT_ARROW; }

					struct LabelField : ui::TextField {
						OscelotModule* module;
						void onSelectKey(const event::SelectKey& e) override {
							if (e.action == GLFW_PRESS && e.key == GLFW_KEY_ENTER) {
								module->contextLabel = text;

								ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
								overlay->requestDelete();
								e.consume(this);
							}

							if (!e.getTarget()) {
								ui::TextField::onSelectKey(e);
							}
						}
					};

					struct ResetItem : ui::MenuItem {
						OscelotModule* module;
						void onAction(const event::Action& e) override { module->contextLabel = ""; }
					};

					Menu* createChildMenu() override {
						Menu* menu = new Menu;

						LabelField* labelField = new LabelField;
						labelField->placeholder = "Name this Cat";
						labelField->box.size.x = 100;
						labelField->module = module;
						labelField->text = module->contextLabel;
						menu->addChild(labelField);

						ResetItem* resetItem = new ResetItem;
						resetItem->text = "Reset";
						resetItem->module = module;
						menu->addChild(resetItem);

						return menu;
					}
				};  // struct ContextMenuItem

				Menu* menu = new Menu;
				menu->addChild(construct<ContextMenuItem>(&MenuItem::text, "Set Context Label", &ContextMenuItem::module, module));
				menu->addChild(construct<TextScrollItem>(&MenuItem::text, "Text scrolling", &TextScrollItem::module, module));
				menu->addChild(construct<MappingIndicatorHiddenItem>(&MenuItem::text, "Hide mapping indicators", &MappingIndicatorHiddenItem::module, module));
				menu->addChild(construct<LockedItem>(&MenuItem::text, "Lock mapping slots", &LockedItem::module, module));
				return menu;
			}
		};  // struct UiMenuItem

		struct ClearMapsItem : MenuItem {
			OscelotModule* module;
			void onAction(const event::Action& e) override { module->clearMaps(); }
		};  // struct ClearMapsItem

		struct ModuleLearnSelectMenuItem : MenuItem {
			OscelotWidget* mw;
			ModuleLearnSelectMenuItem() { rightText = RIGHT_ARROW; }
			Menu* createChildMenu() override {
				struct ModuleLearnSelectItem : MenuItem {
					OscelotWidget* mw;
					LEARN_MODE mode;
					void onAction(const event::Action& e) override { mw->enableLearn(mode); }
				};

				Menu* menu = new Menu;
				menu->addChild(construct<ModuleLearnSelectItem>(&MenuItem::text, "Clear first", &MenuItem::rightText, RACK_MOD_CTRL_NAME "+" RACK_MOD_SHIFT_NAME "+D",
				                                                &ModuleLearnSelectItem::mw, mw, &ModuleLearnSelectItem::mode, LEARN_MODE::BIND_CLEAR));
				menu->addChild(construct<ModuleLearnSelectItem>(&MenuItem::text, "Keep OSC assignments", &MenuItem::rightText, RACK_MOD_SHIFT_NAME "+D", &ModuleLearnSelectItem::mw,
				                                                mw, &ModuleLearnSelectItem::mode, LEARN_MODE::BIND_KEEP));
				return menu;
			}
		};  // struct ModuleLearnSelectMenuItem

		menu->addChild(new MenuSeparator());
		menu->addChild(construct<UiMenuItem>(&MenuItem::text, "User interface", &UiMenuItem::module, module));
		menu->addChild(new MenuSeparator());
		menu->addChild(construct<ClearMapsItem>(&MenuItem::text, "Clear mappings", &ClearMapsItem::module, module));
		menu->addChild(construct<ModuleLearnSelectMenuItem>(&MenuItem::text, "Map module", &ModuleLearnSelectMenuItem::mw, this));

		appendContextMenuMem(menu);
	}

	void appendContextMenuMem(Menu* menu) {
		OscelotModule* module = dynamic_cast<OscelotModule*>(this->module);
		assert(module);

		struct MapMenuItem : MenuItem {
			OscelotModule* module;
			MapMenuItem() { rightText = RIGHT_ARROW; }

			Menu* createChildMenu() override {
				struct OSCmapModuleItem : MenuItem {
					OscelotModule* module;
					std::string pluginSlug;
					std::string moduleSlug;
					MeowMory oscmapModule;
					OSCmapModuleItem() { rightText = RIGHT_ARROW; }
					Menu* createChildMenu() override {
						struct DeleteItem : MenuItem {
							OscelotModule* module;
							std::string pluginSlug;
							std::string moduleSlug;
							void onAction(const event::Action& e) override { module->meowMoryDelete(pluginSlug, moduleSlug); }
						};  // DeleteItem

						Menu* menu = new Menu;
						menu->addChild(construct<DeleteItem>(&MenuItem::text, "Delete", &DeleteItem::module, module, &DeleteItem::pluginSlug, pluginSlug, &DeleteItem::moduleSlug,
						                                     moduleSlug));
						return menu;
					}
				};  // OSCmapModuleItem

				std::list<std::pair<std::string, OSCmapModuleItem*>> l;
				for (auto it : module->meowMoryStorage) {
					MeowMory a = it.second;
					OSCmapModuleItem* oscmapModuleItem = new OSCmapModuleItem;
					oscmapModuleItem->text = string::f("%s %s", a.pluginName.c_str(), a.moduleName.c_str());
					oscmapModuleItem->module = module;
					oscmapModuleItem->oscmapModule = a;
					oscmapModuleItem->pluginSlug = it.first.first;
					oscmapModuleItem->moduleSlug = it.first.second;
					l.push_back(std::pair<std::string, OSCmapModuleItem*>(oscmapModuleItem->text, oscmapModuleItem));
				}

				l.sort();
				Menu* menu = new Menu;
				for (auto it : l) {
					menu->addChild(it.second);
				}
				return menu;
			}
		};  // MapMenuItem

		struct SaveMenuItem : MenuItem {
			OscelotModule* module;
			SaveMenuItem() { rightText = RIGHT_ARROW; }

			Menu* createChildMenu() override {
				struct SaveItem : MenuItem {
					OscelotModule* module;
					std::string pluginSlug;
					std::string moduleSlug;
					void onAction(const event::Action& e) override { module->meowMorySave(pluginSlug, moduleSlug); }
				};  // SaveItem

				typedef std::pair<std::string, std::string> ppair;
				std::list<std::pair<std::string, ppair>> list;
				std::set<ppair> s;
				for (size_t i = 0; i < MAX_PARAMS; i++) {
					int moduleId = module->paramHandles[i].moduleId;
					if (moduleId < 0) continue;
					Module* m = module->paramHandles[i].module;
					auto q = ppair(m->model->plugin->slug, m->model->slug);
					if (s.find(q) != s.end()) continue;
					s.insert(q);

					if (!m) continue;
					std::string l = string::f("%s %s", m->model->plugin->name.c_str(), m->model->name.c_str());
					auto p = std::pair<std::string, ppair>(l, q);
					list.push_back(p);
				}
				list.sort();

				Menu* menu = new Menu;
				for (auto it : list) {
					menu->addChild(
					    construct<SaveItem>(&MenuItem::text, it.first, &SaveItem::module, module, &SaveItem::pluginSlug, it.second.first, &SaveItem::moduleSlug, it.second.second));
				}
				return menu;
			}
		};  // SaveMenuItem

		struct ApplyItem : MenuItem {
			OscelotWidget* mw;
			void onAction(const event::Action& e) override { mw->enableLearn(LEARN_MODE::MEM); }
		};  // ApplyItem

		menu->addChild(new MenuSeparator());
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "...........:::MeowMory:::..........."));
		menu->addChild(construct<MapMenuItem>(&MenuItem::text, "Available mappings", &MapMenuItem::module, module));
		menu->addChild(construct<SaveMenuItem>(&MenuItem::text, "Store mapping", &SaveMenuItem::module, module));
		menu->addChild(construct<ApplyItem>(&MenuItem::text, "Apply mapping", &MenuItem::rightText, RACK_MOD_SHIFT_NAME "+V", &ApplyItem::mw, this));
	}
};

}  // namespace Oscelot
}  // namespace TheModularMind

Model* modelOSCelot = createModel<TheModularMind::Oscelot::OscelotModule, TheModularMind::Oscelot::OscelotWidget>("OSCelot");