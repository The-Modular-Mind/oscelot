#include "Oscelot.hpp"

#include <osdialog.h>

#include "MapModuleBase.hpp"
#include "components/OscelotParam.hpp"
#include "plugin.hpp"
#include "ui/ParamWidgetContextExtender.hpp"
#include "OscelotExpander.hpp"

namespace TheModularMind {
namespace Oscelot {

enum OSCMODE { OSCMODE_DEFAULT = 0, OSCMODE_LOCATE = 1 };

struct OscelotModule : Module, OscelotExpanderBase {
	enum ParamIds { PARAM_RECV, PARAM_SEND, PARAM_PREV, PARAM_NEXT, PARAM_APPLY, PARAM_BANK, NUM_PARAMS };
	enum InputIds { NUM_INPUTS };
	enum OutputIds { NUM_OUTPUTS };
	enum LightIds { ENUMS(LIGHT_RECV, 3), ENUMS(LIGHT_SEND, 3), LIGHT_APPLY, LIGHT_PREV, LIGHT_NEXT, NUM_LIGHTS};

	OscReceiver oscReceiver;
	OscSender oscSender;
	std::string ip = "localhost";
	std::string rxPort = RXPORT_DEFAULT;
	std::string txPort = TXPORT_DEFAULT;

	int panelTheme = rand() % 3;
	float expValues[MAX_PARAMS]={};
	std::string expLabels[MAX_PARAMS]={};
	/** Number of maps */
	int mapLen = 0;
	bool oscIgnoreDevices;
	bool clearMapsOnLoad;
    bool alwaysSendFullFeedback;
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

	std::map<std::string, ModuleMeowMory> meowMoryStorage;
	BankMeowMory meowMoryBankStorage[128];
	int currentBankIndex = 0;
	int64_t meowMoryModuleId = -1;
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
		configParam(PARAM_BANK, 0, 127, 0, "Bank", "", 0.f, 1.f, 1);

		for (int id = 0; id < MAX_PARAMS; id++) {
			paramHandleIndicator[id].color = mappingIndicatorColor;
			paramHandleIndicator[id].handle = &paramHandles[id];
			APP->engine->addParamHandle(&paramHandles[id]);
			oscParam[id].setLimits(0.0f, 1.0f, -1.0f);
		}
		indicatorDivider.setDivision(2048);
		lightDivider.setDivision(2048);
		oscResendDivider.setDivision(APP->engine->getSampleRate() / 2);
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
			expValues[i]=-1.0f;
			expLabels[i] = "None";
		}
		for (int bankIndex = 0; bankIndex < 128; bankIndex++) {
			meowMoryBankStorage[bankIndex] = BankMeowMory();
		}
		locked = false;
		oscIgnoreDevices = false;
		oscResendPeriodically = false;
		oscResendDivider.reset();
		processDivision = 512;
		processDivider.setDivision(processDivision);
		processDivider.reset();
		clearMapsOnLoad = false;
		alwaysSendFullFeedback = false;
		rightExpander.producerMessage = NULL;
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

	void sendOscFeedback(int id) {
		OscBundle feedbackBundle;
		OscMessage valueMessage;
		
		valueMessage.setAddress(oscControllers[id]->getAddress());
		valueMessage.addIntArg(oscControllers[id]->getControllerId());
		valueMessage.addFloatArg(oscControllers[id]->getCurrentValue());
		feedbackBundle.addMessage(valueMessage);

		if (alwaysSendFullFeedback || oscParam[id].hasChanged) {
			OscMessage infoMessage;
			oscParam[id].hasChanged = false;

			infoMessage.setAddress(oscControllers[id]->getAddress() + "/info");
			infoMessage.addIntArg(oscControllers[id]->getControllerId());
			for (auto&& infoArg : getParamInfo(id)) {
				infoMessage.addOscArg(infoArg);
			}
			feedbackBundle.addMessage(infoMessage);
		}

		oscSender.sendBundle(feedbackBundle);
	}

	void process(const ProcessArgs& args) override {
		ts++;
		if (params[PARAM_BANK].getValue() != currentBankIndex) {
			bankMeowMorySave(currentBankIndex);
			currentBankIndex = params[PARAM_BANK].getValue();
			bankMeowMoryApply(currentBankIndex);
		}
		OscMessage rxMessage;
		while (oscReceiver.shift(&rxMessage)) {
			oscReceived = processOscMessage(rxMessage);
		}

		// Process lights
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
					float currentControllerValue = -1.0f;

					// Check if controllerId value has been set and changed
					if (controllerId >= 0 && oscReceived) {
						switch (oscControllers[id]->getControllerMode()) {
						case CONTROLLERMODE::DIRECT:
							if (oscControllers[id]->getValueIn() != oscControllers[id]->getCurrentValue()) {
								oscControllers[id]->setValueIn(oscControllers[id]->getCurrentValue());
								currentControllerValue = oscControllers[id]->getCurrentValue();
							}
							break;
						case CONTROLLERMODE::PICKUP1:
							if (oscControllers[id]->getValueIn() != oscControllers[id]->getCurrentValue()) {
								if (oscParam[id].isNear(oscControllers[id]->getValueIn())) {
									currentControllerValue = oscControllers[id]->getCurrentValue();
								}
								oscControllers[id]->setValueIn(oscControllers[id]->getCurrentValue());
							}
							break;
						case CONTROLLERMODE::PICKUP2:
							if (oscControllers[id]->getValueIn() != oscControllers[id]->getCurrentValue()) {
								if (oscParam[id].isNear(oscControllers[id]->getValueIn(), oscControllers[id]->getCurrentValue())) {
									currentControllerValue = oscControllers[id]->getCurrentValue();
								}
								oscControllers[id]->setValueIn(oscControllers[id]->getCurrentValue());
							}
							break;
						case CONTROLLERMODE::TOGGLE:
							if (oscControllers[id]->getCurrentValue() > 0 && (oscControllers[id]->getValueIn() == -1.f || oscControllers[id]->getValueIn() >= 0.f)) {
								currentControllerValue = oscParam[id].getLimitMax();
								oscControllers[id]->setValueIn(-2.f);
							} else if (oscControllers[id]->getCurrentValue() == 0.f && oscControllers[id]->getValueIn() == -2.f) {
								currentControllerValue = oscParam[id].getLimitMax();
								oscControllers[id]->setValueIn(-3.f);
							} else if (oscControllers[id]->getCurrentValue() > 0.f && oscControllers[id]->getValueIn() == -3.f) {
								currentControllerValue = oscParam[id].getLimitMin();
								oscControllers[id]->setValueIn(-4.f);
							} else if (oscControllers[id]->getCurrentValue() == 0.f && oscControllers[id]->getValueIn() == -4.f) {
								currentControllerValue = oscParam[id].getLimitMin();
								oscControllers[id]->setValueIn(-1.f);
							}
							break;
						case CONTROLLERMODE::TOGGLE_VALUE:
							if (oscControllers[id]->getCurrentValue() > 0 && (oscControllers[id]->getValueIn() == -1.f || oscControllers[id]->getValueIn() >= 0.f)) {
								currentControllerValue = oscControllers[id]->getCurrentValue();
								oscControllers[id]->setValueIn(-2.f);
							} else if (oscControllers[id]->getCurrentValue() == 0.f && oscControllers[id]->getValueIn() == -2.f) {
								currentControllerValue = oscParam[id].getValue();
								oscControllers[id]->setValueIn(-3.f);
							} else if (oscControllers[id]->getCurrentValue() > 0.f && oscControllers[id]->getValueIn() == -3.f) {
								currentControllerValue = oscParam[id].getLimitMin();
								oscControllers[id]->setValueIn(-4.f);
							} else if (oscControllers[id]->getCurrentValue() == 0.f && oscControllers[id]->getValueIn() == -4.f) {
								currentControllerValue = oscParam[id].getLimitMin();
								oscControllers[id]->setValueIn(-1.f);
							}
							break;
						}
					}

					// Set a new value for the mapped parameter
					if (currentControllerValue >= 0.f) {
						oscParam[id].setValue(currentControllerValue);
					}

					// Apply value on the mapped parameter (respecting slew and scale)
					oscParam[id].process(args.sampleTime * float(processDivision));

					// Retrieve the current value of the parameter (ignoring slew and scale)
					float currentParamValue = oscParam[id].getValue();

					// OSC feedback
					if (oscControllers[id]->getValueOut() != currentParamValue) {
						if (controllerId >= 0 && oscControllers[id]->getControllerMode() == CONTROLLERMODE::DIRECT) oscControllers[id]->setValueIn(currentParamValue);

						oscControllers[id]->setCurrentValue(currentParamValue, 0);
						expValues[id]=currentParamValue;
						oscControllers[id]->setValueOut(currentParamValue);
						if (sending) {
							sendOscFeedback(id);
							oscSent = true;
						}
					}
				} break;

				case OSCMODE::OSCMODE_LOCATE: {
					bool indicate = false;
					if ((controllerId >= 0 && oscControllers[id]->getCurrentValue() >= 0) && oscControllers[id]->getValueIndicate() != oscControllers[id]->getCurrentValue()) {
						oscControllers[id]->setValueIndicate(oscControllers[id]->getCurrentValue());
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
		// Expander
		if (rightExpander.module && rightExpander.module->model == modelOscelotExpander && !rightExpander.producerMessage) {
			rightExpander.producerMessage = new ExpanderPayload((OscelotExpanderBase*)this, 0);
			rightExpander.requestMessageFlip();
		}
	}

	std::list<OscArg*> getParamInfo(int id) {
		std::list<OscArg*> s;
		if (id >= mapLen) return s;
		if (paramHandles[id].moduleId < 0) return s;

		ModuleWidget* mw = APP->scene->rack->getModule(paramHandles[id].moduleId);
		if (!mw) return s;

		Module* m = mw->getModule();
		if (!m) return s;

		int paramId = paramHandles[id].paramId;
		if (paramId >= (int)m->params.size()) return s;
		
		ParamQuantity* paramQuantity = m->paramQuantities[paramId];
		s.push_back(new OscArgString(mw->model->name));
		s.push_back(new OscArgFloat(paramQuantity->toScaled(paramQuantity->getDefaultValue())));
		s.push_back(new OscArgString(paramQuantity->getLabel()));
		s.push_back(new OscArgString(paramQuantity->getDisplayValueString()));
		s.push_back(new OscArgString(paramQuantity->getUnit()));
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
			WARN("Discarding OSC message. Need 2 args: id(int) and value(float). OSC message had address: %s and %i args", msg.getAddress().c_str(), (int) msg.getNumArgs());
			return oscReceived;
		}

		int controllerId = msg.getArgAsInt(0);
		float value = msg.getArgAsFloat(1);
		// Learn
		if (learningId >= 0 && (learnedControllerIdLast != controllerId || lastLearnedAddress != address)) {
			oscControllers[learningId] = OscController::Create(address, controllerId, CONTROLLERMODE::DIRECT, value, ts);
			expLabels[learningId] = string::f("%s-%02d", oscControllers[learningId]->getTypeString(), oscControllers[learningId]->getControllerId());

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
					oscControllers[id]->setCurrentValue(value, ts);
					expValues[id] = value;

					return oscReceived;
				}
			}
		}
		return oscReceived;
	}

	void oscResendFeedback() {
		for (int i = 0; i < MAX_PARAMS; i++) {
			if (oscControllers[i]) {
				oscParam[i].hasChanged =true;
				oscControllers[i]->setValueOut(-1.f);
			}
		}
	}

	void clearMap(int id, bool oscOnly = false) {
		learningId = -1;
		oscParam[id].reset();
		oscControllers[id] = nullptr;
		expValues[id] = 0.0f;
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
			expValues[id] = 0.0f;
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

	void learnParam(int id, int64_t moduleId, int paramId) {
		APP->engine->updateParamHandle(&paramHandles[id], moduleId, paramId, true);
		textLabels[id] = "";
		oscParam[id].reset();
		learnedParam = true;
		commitLearn();
		updateMapLen();
	}

	void learnMapping(int mapId, ModuleMeowMoryParam meowMoryParam) {
		if (meowMoryParam.controllerId >= 0) {
			oscControllers[mapId] = OscController::Create(meowMoryParam.address, meowMoryParam.controllerId, meowMoryParam.controllerMode);
			expLabels[mapId] = string::f("%s-%02d", oscControllers[mapId]->getTypeString(), oscControllers[mapId]->getControllerId());
			if (meowMoryParam.encSensitivity) oscControllers[mapId]->setSensitivity(meowMoryParam.encSensitivity);
		}
		textLabels[mapId] = meowMoryParam.label;
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

	void moduleMeowMorySave(std::string saveKey) {
		ModuleMeowMory meowMory = ModuleMeowMory();
		Module* module = NULL;
		for (int mapIndex = 0; mapIndex < mapLen; mapIndex++) {
			if (paramHandles[mapIndex].moduleId < 0) continue;

			auto paramKey = string::f("%s %s", paramHandles[mapIndex].module->model->plugin->slug.c_str(), paramHandles[mapIndex].module->model->slug.c_str());
			if (paramKey != saveKey) continue;
			module = paramHandles[mapIndex].module;

			ModuleMeowMoryParam meowMoryParam = ModuleMeowMoryParam();
			meowMoryParam.fromMappings(paramHandles[mapIndex], oscControllers[mapIndex], textLabels[mapIndex]);
			meowMory.paramArray.push_back(meowMoryParam);
		}
		meowMory.pluginName = module->model->plugin->name;
		meowMory.moduleName = module->model->name;
		meowMoryStorage[saveKey] = meowMory;
	}

	void moduleMeowMoryDelete(std::string key) { meowMoryStorage.erase(key); }

	void moduleMeowMoryApply(Module* m) {
		if (!m) return;
		auto key = string::f("%s %s", m->model->plugin->slug.c_str(), m->model->slug.c_str());
		auto it = meowMoryStorage.find(key);
		if (it == meowMoryStorage.end()) return;
		ModuleMeowMory meowMory = it->second;

		clearMaps();
		meowMoryModuleId = m->id;
		int mapIndex = 0;
		for (ModuleMeowMoryParam meowMoryParam : meowMory.paramArray) {
			learnParam(mapIndex, m->id, meowMoryParam.paramId);
			learnMapping(mapIndex, meowMoryParam);
			mapIndex++;
		}
		updateMapLen();
	}

	bool moduleMeowMoryTest(Module* m) {
		if (!m) return false;
		auto key = string::f("%s %s", m->model->plugin->slug.c_str(), m->model->slug.c_str());
		auto it = meowMoryStorage.find(key);
		if (it == meowMoryStorage.end()) return false;
		return true;
	}

	void bankMeowMorySave(int index) { meowMoryBankStorage[index] = bankToMeowMory(); }

	void bankMeowMoryDelete(int index) { meowMoryBankStorage[index] = BankMeowMory(); }

	void bankMeowMoryApply(int index) {
		clearMaps();
		meowMoryToBank(meowMoryBankStorage[index]);
	}

	BankMeowMory bankToMeowMory() {
		BankMeowMory meowMory;
		for (int id = 0; id < mapLen; id++) {
			BankMeowMoryParam param;
			param.fromMappings(paramHandles[id], oscControllers[id], textLabels[id]);
			meowMory.bankParamArray.push_back(param);
		}
		return meowMory;
	}

	void meowMoryToBank(BankMeowMory meowMory) {
		int mapIndex = 0;
		for (BankMeowMoryParam meowMoryParam : meowMory.bankParamArray) {
			learnParam(mapIndex, meowMoryParam.moduleId, meowMoryParam.paramId);
			learnMapping(mapIndex, meowMoryParam);
			mapIndex++;
		}
		updateMapLen();
	}

	void setProcessDivision(int d) {
		processDivision = d;
		processDivider.setDivision(d);
		processDivider.reset();
		lightDivider.setDivision(2048);
		lightDivider.reset();
	}

	float* expGetValues() override {
		return expValues;
	}
	
	std::string* expGetLabels() override {
		return expLabels;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		// Settings
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
		json_object_set_new(rootJ, "alwaysSendFullFeedback", json_boolean(alwaysSendFullFeedback));
		json_object_set_new(rootJ, "oscIgnoreDevices", json_boolean(oscIgnoreDevices));
		json_object_set_new(rootJ, "currentBankIndex", json_integer(currentBankIndex));

		// Module MeowMory
		json_t* meowMoryStorageJ = json_array();
		for (auto it : meowMoryStorage) {
			ModuleMeowMory meowMory = it.second;
			json_t* meowMoryJ = meowMory.toJson();
			json_object_set_new(meowMoryJ, "key", json_string(it.first.c_str()));
			json_array_append_new(meowMoryStorageJ, meowMoryJ);
		}
		json_object_set_new(rootJ, "meowMory", meowMoryStorageJ);

		// Bank MeowMory
		bankMeowMorySave(currentBankIndex);
		json_t* meowMoryBankStorageJ = json_array();
		for (int bankIndex = 0; bankIndex < 128; bankIndex++) {
			if (meowMoryBankStorage[bankIndex].bankParamArray.size() == 0) continue;

			json_t* bankJ = meowMoryBankStorage[bankIndex].toJson();
			if (json_object_size(bankJ) > 0) {
				json_object_set_new(bankJ, "bankIndex", json_integer(bankIndex));
				json_array_append_new(meowMoryBankStorageJ, bankJ);
			}
		}

		json_object_set_new(rootJ, "banks", meowMoryBankStorageJ);
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// Settings
		panelTheme = json_integer_value(json_object_get(rootJ, "panelTheme"));
		oscResendPeriodically = json_boolean_value(json_object_get(rootJ, "oscResendPeriodically"));
		alwaysSendFullFeedback = json_boolean_value(json_object_get(rootJ, "alwaysSendFullFeedback"));
		contextLabel = json_string_value(json_object_get(rootJ, "contextLabel"));
		textScrolling = json_boolean_value(json_object_get(rootJ, "textScrolling"));
		mappingIndicatorHidden = json_boolean_value(json_object_get(rootJ, "mappingIndicatorHidden"));
		locked = json_boolean_value(json_object_get(rootJ, "locked"));
		processDivision = json_integer_value(json_object_get(rootJ, "processDivision"));
		clearMapsOnLoad = json_boolean_value(json_object_get(rootJ, "clearMapsOnLoad"));
		if (clearMapsOnLoad) clearMaps();

		// Module MeowMory
		resetMapMemory();
		json_t* meowMoryStorageJ = json_object_get(rootJ, "meowMory");
		size_t i;
		json_t* meowMoryJ;
		json_array_foreach(meowMoryStorageJ, i, meowMoryJ) {
			std::string key = json_string_value(json_object_get(meowMoryJ, "key"));
			ModuleMeowMory meowMory = ModuleMeowMory();
			meowMory.fromJson(meowMoryJ);
			meowMoryStorage[key] = meowMory;
		}

		// Bank MeowMory
		json_t* banksJ = json_object_get(rootJ, "banks");
		json_t* currentBankIndexJ = json_object_get(rootJ, "currentBankIndex");
		currentBankIndex = currentBankIndexJ ? json_integer_value(currentBankIndexJ) : 0;
		if (banksJ) {
			size_t bankArrayIndex;
			json_t* bankObjectJ;
			json_array_foreach(banksJ, bankArrayIndex, bankObjectJ) {
				int bankIndex = json_integer_value(json_object_get(bankObjectJ, "bankIndex"));
				BankMeowMory meowMory;
				meowMory.fromJson(bankObjectJ);
				meowMoryBankStorage[bankIndex] = meowMory;
			}
		}
		bankMeowMoryApply(currentBankIndex);

		// OSC settings
		if (!oscIgnoreDevices) {
			oscIgnoreDevices = json_boolean_value(json_object_get(rootJ, "oscIgnoreDevices"));
			receiving = json_boolean_value(json_object_get(rootJ, "receiving"));
			sending = json_boolean_value(json_object_get(rootJ, "sending"));
			ip = json_string_value(json_object_get(rootJ, "ip"));
			txPort = json_string_value(json_object_get(rootJ, "txPort"));
			rxPort = json_string_value(json_object_get(rootJ, "rxPort"));
			receiverPower();
			senderPower();
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

			Menu* createChildMenu() override {
				Menu* menu = new Menu;

				LabelField* labelField = new LabelField;
				labelField->box.size.x = 60;
				labelField->module = module;
				labelField->text = std::to_string(module->oscControllers[id]->getSensitivity());
				labelField->id = id;
				menu->addChild(labelField);
				menu->addChild(createMenuItem("Reset", "", [=]() { module->oscControllers[id]->setSensitivity(OscController::ENCODER_DEFAULT_SENSITIVITY); }));

				return menu;
			}
		};  // struct EncoderMenuItem

		if (module->oscControllers[id]) {
			menu->addChild(createMenuItem("Clear OSC assignment", "", [=]() { module->clearMap(id, true); }));
			if (strcmp(module->oscControllers[id]->getTypeString(), "ENC") == 0)
				menu->addChild(construct<EncoderMenuItem>(&MenuItem::text, "Encoder Sensitivity", &EncoderMenuItem::module, module, &EncoderMenuItem::id, id));
			else
				menu->addChild(createSubmenuItem("Input mode for Controller", "", [=](Menu* menu) {
					menu->addChild(createCheckMenuItem("Direct", "", [=]() { return module->oscControllers[id]->getControllerMode() == CONTROLLERMODE::DIRECT; }, [=]() { module->oscControllers[id]->setControllerMode(CONTROLLERMODE::DIRECT); }));
					menu->addChild(createCheckMenuItem("Pickup (snap)", "", [=]() { return module->oscControllers[id]->getControllerMode() == CONTROLLERMODE::PICKUP1; }, [=]() { module->oscControllers[id]->setControllerMode(CONTROLLERMODE::PICKUP1); }));
					menu->addChild(createCheckMenuItem("Pickup (jump)", "", [=]() { return module->oscControllers[id]->getControllerMode() == CONTROLLERMODE::PICKUP2; }, [=]() { module->oscControllers[id]->setControllerMode(CONTROLLERMODE::PICKUP2); }));
					menu->addChild(createCheckMenuItem("Toggle", "", [=]() { return module->oscControllers[id]->getControllerMode() == CONTROLLERMODE::TOGGLE; }, [=]() { module->oscControllers[id]->setControllerMode(CONTROLLERMODE::TOGGLE); }));
					menu->addChild(createCheckMenuItem("Toggle + Value", "", [=]() { return module->oscControllers[id]->getControllerMode() == CONTROLLERMODE::TOGGLE_VALUE; }, [=]() { module->oscControllers[id]->setControllerMode(CONTROLLERMODE::TOGGLE_VALUE); }));
				}));
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
	LabelSliderHorizontal* slider;
	
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
		inpPos = mm2px(Vec(27, 109));
		addChild(createParamCentered<PawButton>(inpPos, module, OscelotModule::PARAM_PREV));
		addChild(createLightCentered<PawPrevLight>(inpPos, module, OscelotModule::LIGHT_PREV));

		inpPos = mm2px(Vec(46, 109));
		addChild(createParamCentered<PawButton>(inpPos, module, OscelotModule::PARAM_APPLY));
		addChild(createLightCentered<PawLight>(inpPos, module, OscelotModule::LIGHT_APPLY));

		inpPos = mm2px(Vec(65, 109));
		addChild(createParamCentered<PawButton>(inpPos, module, OscelotModule::PARAM_NEXT));
		addChild(createLightCentered<PawNextLight>(inpPos, module, OscelotModule::LIGHT_NEXT));

		// Banks
		inpPos = mm2px(Vec(46, 122));
		slider = createParamCentered<LabelSliderHorizontal>(inpPos, module, OscelotModule::PARAM_BANK);
		slider->module = module;
		if (module) {
			slider->label->text = std::to_string(module->currentBankIndex + 1);
		}
		addChild(slider);
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

			slider->label->text = std::to_string(module->currentBankIndex + 1);

			if (module->contextLabel != contextLabel) {
				contextLabel = module->contextLabel;
			}
		}

		ParamWidgetContextExtender::step();
	}

	void meowMoryPrevModule() {
		std::list<Widget*> modules = APP->scene->rack->getModuleContainer()->children;
		auto sort = [&](Widget* w1, Widget* w2) {
			auto t1 = std::make_tuple(w1->box.pos.y, w1->box.pos.x);
			auto t2 = std::make_tuple(w2->box.pos.y, w2->box.pos.x);
			return t1 > t2;
		};
		modules.sort(sort);
		meowMoryScanModules(modules);
	}

	void meowMoryNextModule() {
		std::list<Widget*> modules = APP->scene->rack->getModuleContainer()->children;
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
			if (module->moduleMeowMoryTest(m)) {
				module->moduleMeowMoryApply(m);
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
		ParamQuantity* pq = pw->getParamQuantity();
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
				Menu* menu = new Menu;
				if (currentId < 0) {
					menu->addChild(createMenuItem("Learn OSC", "", [=]() {
						int id = module->enableLearn(-1, true);
						if (id >= 0) module->learnParam(id, pq->module->id, pq->paramId);
					}));
				} else {
					menu->addChild(createMenuItem("Learn OSC", "", [=]() { module->enableLearn(currentId, true); }));
				}

				if (module->mapLen > 0) {
					menu->addChild(new MenuSeparator);
					for (int id = 0; id < module->mapLen; id++) {
						if (module->oscControllers[id]) {
							std::string text;
							if (module->textLabels[id] != "") {
								text = module->textLabels[id];
							} else {
								text = string::f("%s-%02d", module->oscControllers[id]->getTypeString(), module->oscControllers[id]->getControllerId());
							}
							menu->addChild(createCheckMenuItem(text, "", [=]() { return id == currentId; }, [=]() { module->learnParam(id, pq->module->id, pq->paramId); }));
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
				module->moduleMeowMoryApply(m);
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
		APP->event->setSelectedWidget(this);
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
		int sampleRate = int(APP->engine->getSampleRate());

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

			Menu* createChildMenu() override {
				Menu* menu = new Menu;

				LabelField* labelField = new LabelField;
				labelField->placeholder = "Name this Cat";
				labelField->box.size.x = 100;
				labelField->module = module;
				labelField->text = module->contextLabel;
				menu->addChild(labelField);
				menu->addChild(createMenuItem("Reset", "", [=]() { module->contextLabel = ""; }));
				return menu;
			}
		};  // struct ContextMenuItem

		menu->addChild(createSubmenuItem("User interface", "", [=](Menu* menu) {
			menu->addChild(construct<ContextMenuItem>(&MenuItem::text, "Set Context Label", &ContextMenuItem::module, module));
			menu->addChild(createBoolPtrMenuItem("Text scrolling", "",  &module->textScrolling));
			menu->addChild(createBoolPtrMenuItem("Hide mapping indicators", "", &module->mappingIndicatorHidden));
			menu->addChild(createBoolPtrMenuItem("Lock mapping slots", "", &module->locked));
		}));
		menu->addChild(new MenuSeparator());

		menu->addChild(createSubmenuItem("Preset load", "", [=](Menu* menu) {
			menu->addChild(createBoolPtrMenuItem("Ignore OSC devices", "", &module->oscIgnoreDevices));
			menu->addChild(createBoolPtrMenuItem("Clear mapping slots", "", &module->clearMapsOnLoad));
		}));

		menu->addChild(createSubmenuItem("Precision", "", [=](Menu* menu) {
			menu->addChild(createCheckMenuItem(string::f("Unnecessary (%i Hz)",sampleRate / 64), "", [=]() {	return module->processDivision == 64;}, [=]() {	module->setProcessDivision(64);}));
			menu->addChild(createCheckMenuItem(string::f("Why not (%i Hz)",sampleRate / 128), "", [=]() {	return module->processDivision == 128;}, [=]() {	module->setProcessDivision(128);}));
			menu->addChild(createCheckMenuItem(string::f("More than Enough (%i Hz)",sampleRate / 256), "", [=]() {	return module->processDivision == 256;}, [=]() {	module->setProcessDivision(256);}));
			menu->addChild(createCheckMenuItem(string::f("Enough (%i Hz)",sampleRate / 512), "", [=]() {	return module->processDivision == 512;}, [=]() {	module->setProcessDivision(512);}));
		}));

		menu->addChild(createSubmenuItem("Mode", "", [=](Menu* menu) {
			menu->addChild(createCheckMenuItem("Operating", "", [=]() {	return module->oscMode == OSCMODE::OSCMODE_DEFAULT;}, [=]() {	module->setMode(OSCMODE::OSCMODE_DEFAULT);}));
			menu->addChild(createCheckMenuItem("Locate and indicate", "", [=]() {	return module->oscMode == OSCMODE::OSCMODE_LOCATE;}, [=]() {	module->setMode(OSCMODE::OSCMODE_LOCATE);}));
		}));

		menu->addChild(createSubmenuItem("Re-send OSC feedback", "", [=](Menu* menu) {
			menu->addChild(createMenuItem("Now", "", [=]() { module->oscResendFeedback(); }));
			menu->addChild(createBoolPtrMenuItem("Periodically", "", &module->oscResendPeriodically ));
			menu->addChild(createBoolPtrMenuItem("Send Full feedback", "", &module->alwaysSendFullFeedback ));
		}));

		menu->addChild(new MenuSeparator());
		menu->addChild(createSubmenuItem("Map module", "", [=](Menu* menu) {
			menu->addChild(createMenuItem("Clear first", RACK_MOD_CTRL_NAME "+" RACK_MOD_SHIFT_NAME "+D", [=]() { enableLearn(LEARN_MODE::BIND_CLEAR); }));
			menu->addChild(createMenuItem("Keep OSC assignments", RACK_MOD_SHIFT_NAME "+D", [=]() { enableLearn(LEARN_MODE::BIND_KEEP); }));
		}));
		menu->addChild(createMenuItem("Clear mappings", "", [=]() { module->clearMaps(); }));

		appendContextMenuMem(menu);
	}

	void appendContextMenuMem(Menu* menu) {
		OscelotModule* module = dynamic_cast<OscelotModule*>(this->module);
		assert(module);
		
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("...........:::MeowMory:::..........."));
		menu->addChild(createSubmenuItem("Available mappings", "", [=](Menu* menu) {
			for (auto it : module->meowMoryStorage) {
					ModuleMeowMory meowMory = it.second;
					std::string text = string::f("%s %s", meowMory.pluginName.c_str(), meowMory.moduleName.c_str());
					std::string key = it.first;
					menu->addChild(createSubmenuItem(text, "", [=](Menu* menu) {
						menu->addChild(createMenuItem("Delete", "", [=]() { module->moduleMeowMoryDelete(key); }));
					}));
				}
		}));
		menu->addChild(createSubmenuItem("Store mapping", "", [=](Menu* menu) {
			std::map<std::string, std::string> modulesToSave;

			for (size_t i = 0; i < MAX_PARAMS; i++) {
				if (module->paramHandles[i].moduleId < 0) continue;
				Module* m = module->paramHandles[i].module;
				if (!m) continue;

				auto saveKey = string::f("%s %s", m->model->plugin->slug.c_str(), m->model->slug.c_str());
				std::string menuName = string::f("%s %s", m->model->plugin->name.c_str(), m->model->name.c_str());
				modulesToSave[menuName] = saveKey;
			}
			for (auto it : modulesToSave) {
				menu->addChild(createMenuItem(it.first, "", [=]() { module->moduleMeowMorySave(it.second); }));
			}
		}));
		menu->addChild(createMenuItem("Apply mapping", RACK_MOD_SHIFT_NAME "+V", [=]() { enableLearn(LEARN_MODE::MEM); }));
	}
};

}  // namespace Oscelot
}  // namespace TheModularMind

Model* modelOSCelot = createModel<TheModularMind::Oscelot::OscelotModule, TheModularMind::Oscelot::OscelotWidget>("OSCelot");