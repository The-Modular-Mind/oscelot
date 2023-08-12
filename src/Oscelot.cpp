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

	int panelTheme = rand() % 4;
	float expValues[MAX_PARAMS]={};
	std::string expLabels[MAX_PARAMS]={};
	/** Number of maps */
	int mapLen = 0;
	bool oscIgnoreDevices;
	bool clearMapsOnLoad;
    bool alwaysSendFullFeedback;
    int sendEndMessage;
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
	std::string moduleSlug = "";

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

	/** Stored OSC client state in OSC'elot preset */
  std::string oscClientStoredState = "";

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
		clearMaps(false);
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
		oscClientStoredState = "";

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

	void switchBankTo(int bankIndex) {
		bankMeowMorySave(currentBankIndex);
		currentBankIndex = bankIndex;
		bankMeowMoryApply(currentBankIndex);
	}
	
	void sendOscClientStoredStateMessage(std::string storedState) {
		OscBundle feedbackBundle;
		OscMessage stateMessage;

		stateMessage.setAddress("/state");
		stateMessage.addStringArg(storedState.c_str());

		feedbackBundle.addMessage(stateMessage);
		oscSender.sendBundle(feedbackBundle);
	}

	void process(const ProcessArgs& args) override {
		ts++;
		if (params[PARAM_BANK].getValue() != currentBankIndex) {
			switchBankTo(params[PARAM_BANK].getValue());
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
					if (oscControllers[id]->getValueOut() != paramQuantity->getDisplayValueString()) {
						if (controllerId >= 0 && oscControllers[id]->getControllerMode() == CONTROLLERMODE::DIRECT) oscControllers[id]->setValueIn(currentParamValue);

						oscControllers[id]->setCurrentValue(currentParamValue, 0);
						expValues[id]=currentParamValue;
						oscControllers[id]->setValueOut(paramQuantity->getDisplayValueString());
						if (sending) {
							sendOscFeedback(id);
							oscSent = true;
								// Send end of mappings message, when switching between saved mappings.
							if (sendEndMessage > 0 && mapLen - 2 == id) {
								OscMessage endMessage;
								if (sendEndMessage == 1)
									endMessage.setAddress(OSCMSG_MODULE_END);
								else
									endMessage.setAddress(OSCMSG_BANK_END);

								endMessage.addIntArg(mapLen - 1);
								oscSender.sendMessage(endMessage);
								sendEndMessage = 0;
							}
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
		if (address == OSCMSG_NEXT_MODULE) {
			oscTriggerNext = true;
			if (msg.getNumArgs() > 0) {
				moduleSlug = msg.getArgAsString(0);
			}
			return oscReceived;
		} else if (address == OSCMSG_PREV_MODULE) {
			oscTriggerPrev = true;
			if (msg.getNumArgs() > 0) {
				moduleSlug = msg.getArgAsString(0);
			}
			return oscReceived;
		} else if (address == OSCMSG_BANK_SELECT) {
			int bankIndex = msg.getArgAsInt(0);
			if (bankIndex >= 0 && bankIndex < 128) {
				params[PARAM_BANK].setValue(bankIndex);
				switchBankTo(bankIndex);
				return oscReceived;
			}
		} else if (address == OSCMSG_LIST_MODULES) {
			sendMappedModuleList();
      return oscReceived;
		} else if (address == OSCMSG_STORE_CLIENT_STATE) {
			oscClientStoredState = msg.getArgAsString(0);
			return oscReceived;
		} else if (address == OSCMSG_GET_CLIENT_STATE) {
			sendOscClientStoredStateMessage(oscClientStoredState);
			return oscReceived;
		// } else if (address != ADDRESS_FADER || address != ADDRESS_ENCODER || address != ADDRESS_BUTTON) {
		} else if (msg.getNumArgs()!=2) {
			WARN("Discarding OSC message. Need 2 args: id(int) and value(float). OSC message had address: %s and %i args", msg.getAddress().c_str(), (int)msg.getNumArgs());
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
				oscControllers[i]->setValueOut("-1");
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

	void clearMaps(bool Lock = true) {
		learningId = -1;
		for (int id = 0; id < MAX_PARAMS; id++) {
			textLabels[id] = "";
			oscParam[id].reset();
			oscControllers[id] = nullptr;
			expValues[id] = 0.0f;
			if(Lock){
				APP->engine->updateParamHandle(&paramHandles[id], -1, 0, true);
			} else {
				APP->engine->updateParamHandle_NoLock(&paramHandles[id], -1, 0, true);
			}
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

	void learnParam(int id, int64_t moduleId, int paramId, bool Lock = true) {
		if(Lock){
			APP->engine->updateParamHandle(&paramHandles[id], moduleId, paramId, true);
		} else {
			APP->engine->updateParamHandle_NoLock(&paramHandles[id], moduleId, paramId, true);
		}
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
		
		OscMessage startMessage;
		startMessage.setAddress(OSCMSG_MODULE_START);
		startMessage.addStringArg(m->model->name.c_str());
		startMessage.addStringArg(m->model->getFullName());
		startMessage.addStringArg(m->model->description);
		startMessage.addIntArg(meowMory.paramArray.size());
		oscSender.sendMessage(startMessage);
		sendEndMessage=1;

		clearMaps();
		meowMoryModuleId = m->id;
		int mapIndex = 0;
		for (ModuleMeowMoryParam meowMoryParam : meowMory.paramArray) {
			learnParam(mapIndex, m->id, meowMoryParam.paramId);
			learnMapping(mapIndex, meowMoryParam);
			mapIndex++;
		}
	}

	bool moduleMeowMoryTest(Module* m) {
		if (!m) return false;
		auto key = string::f("%s %s", m->model->plugin->slug.c_str(), m->model->slug.c_str());
		auto it = meowMoryStorage.find(key);
		if (it == meowMoryStorage.end()) return false;
		return true;
	}

	void bankMeowMorySave(int index) { 
		BankMeowMory meowMory;
		for (int id = 0; id < mapLen; id++) {
			BankMeowMoryParam param;
			param.fromMappings(paramHandles[id], oscControllers[id], textLabels[id]);
			meowMory.bankParamArray.push_back(param);
		}
		meowMoryBankStorage[index] = meowMory;
	}

	void bankMeowMoryDelete(int index) { meowMoryBankStorage[index] = BankMeowMory(); }

	void bankMeowMoryApply(int index) {
		clearMaps(false);
				
		OscMessage startMessage;
		startMessage.setAddress(OSCMSG_BANK_START);
		startMessage.addIntArg(meowMoryBankStorage[index].bankParamArray.size()-1);
		oscSender.sendMessage(startMessage);
		sendEndMessage=2;

		int mapIndex = 0;
		for (BankMeowMoryParam meowMoryParam : meowMoryBankStorage[index].bankParamArray) {
			learnParam(mapIndex, meowMoryParam.moduleId, meowMoryParam.paramId, false);
			learnMapping(mapIndex, meowMoryParam);
			mapIndex++;
		}
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

		json_object_set_new(rootJ, "oscClientStoredState", json_string(oscClientStoredState.c_str()));

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
		if (clearMapsOnLoad) clearMaps(false);

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

		json_t* oscClientStoredStateJ = json_object_get(rootJ, "oscClientStoredState");
		oscClientStoredState = oscClientStoredStateJ ? json_string_value(oscClientStoredStateJ) : "";
	}

	void sendMappedModuleList() {
		OscBundle moduleListBundle;
		OscMessage moduleListMessage;
    moduleListMessage.setAddress(OSCMSG_MODULE_LIST);

    // Build mapped module list to send to OSC clients in /oscelot/moduleList message
		std::list<Widget*> modules = APP->scene->rack->getModuleContainer()->children;
		auto sort = [&](Widget* w1, Widget* w2) {
			auto t1 = std::make_tuple(w1->box.pos.y, w1->box.pos.x);
			auto t2 = std::make_tuple(w2->box.pos.y, w2->box.pos.x);
			return t1 < t2;
		};
		modules.sort(sort);
		std::list<Widget*>::iterator it = modules.begin();
		
		// Scan over all rack modules, determine if each is mapped in OSC'elot
		for (; it != modules.end(); it++) {
			ModuleWidget* mw = dynamic_cast<ModuleWidget*>(*it);
			Module* m = mw->module;
			if (moduleMeowMoryTest(m)) {
        // Add module to mapped module list
        // If there more than one instance of  mapped module in the rack, it will appear in the list multiple times
        auto key = string::f("%s %s", m->model->plugin->slug.c_str(), m->model->slug.c_str());
        moduleListMessage.addStringArg(key);
 	      moduleListMessage.addStringArg(m->model->name);
			}
		}
    moduleListBundle.addMessage(moduleListMessage);
    oscSender.sendBundle(moduleListBundle);
	}

};

}  // namespace Oscelot
}  // namespace TheModularMind