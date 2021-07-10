#include "plugin.hpp"
#include "MidiCat.hpp"
#include "MapModuleBase.hpp"
#include "StripIdFixModule.hpp"
#include "digital/ScaledMapParam.hpp"
#include "components/MenuLabelEx.hpp"
#include "components/SubMenuSlider.hpp"
#include "components/MidiWidget.hpp"
#include "components/MatrixButton.hpp"
#include "osc/vcvOsc.h"
#include "ui/ParamWidgetContextExtender.hpp"
#include "ui/OverlayMessageWidget.hpp"
#include <osdialog.h>

namespace StoermelderPackOne {
namespace MidiCat {

static const char PRESET_FILTERS[] = "VCV Rack module preset (.vcvm):vcvm";


enum MIDIMODE {
	MIDIMODE_DEFAULT = 0,
	MIDIMODE_LOCATE = 1
};


struct MidiCatParam : ScaledMapParam<float> {
	vcvOscController* oscController=nullptr;
};


struct MidiCatModule : Module, StripIdFixModule {

	enum ParamIds {
		PARAM_CONNECT,
		PARAM_PREV,
		PARAM_NEXT,
		PARAM_APPLY,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(LIGHT_CONNECT, 2),
		LIGHT_APPLY,
		NUM_LIGHTS
	};

	/** [Stored to Json] */
	// midi::InputQueue midiInput;
	vcvOscReceiver oscReceiver;
	/** [Stored to Json] */
	OscCatOutput midiOutput;
	// std::string ip="127.127.127.127";
	std::string ip="localhost";
	std::string rxPort = "7009";
	std::string txPort = "7002";

	/** [Stored to JSON] */
	int panelTheme = 0;

	/** Number of maps */
	int mapLen = 0;
	/** [Stored to Json] The mapped CC number of each channel */
	int midiOptions[MAX_CHANNELS];
	/** [Stored to JSON] */
	bool midiIgnoreDevices;
	/** [Stored to JSON] */
	bool clearMapsOnLoad;

	/** [Stored to Json] The mapped param handle of each channel */
	ParamHandle paramHandles[MAX_CHANNELS];
	ParamHandleIndicator paramHandleIndicator[MAX_CHANNELS];

	/** Channel ID of the learning session */
	int learningId;
	/** Wether multiple slots or just one slot should be learned */
	bool learnSingleSlot = false;
	/** Whether the CC has been set during the learning session */
	bool learnedCc;
	int learnedCcLast = -1;
	std::string lastLearnedAddress="";
	/** Whether the param has been set during the learning session */
	bool learnedParam;

	/** [Stored to Json] */
	bool textScrolling = true;
	/** [Stored to Json] */
	std::string textLabel[MAX_CHANNELS];
	/** [Stored to Json] */
	bool locked;

	NVGcolor mappingIndicatorColor = nvgRGB(0xff, 0xff, 0x40);
	/** [Stored to Json] */
	bool mappingIndicatorHidden = false;

	uint32_t ts = 0;

	MIDIMODE midiMode = MIDIMODE::MIDIMODE_DEFAULT;

	/** Track last values */
	float lastValueIn[MAX_CHANNELS];
	float lastValueInIndicate[MAX_CHANNELS];
	float lastValueOut[MAX_CHANNELS];

	dsp::RingBuffer<int, 8> overlayQueue;
	/** [Stored to Json] */
	bool overlayEnabled;

	/** [Stored to Json] */
	MidiCatParam midiParam[MAX_CHANNELS];
	/** [Stored to Json] */
	bool midiResendPeriodically;
	dsp::ClockDivider midiResendDivider;

	dsp::ClockDivider processDivider;
	dsp::ClockDivider lightDivider;
	/** [Stored to Json] */
	int processDivision;
	dsp::ClockDivider indicatorDivider;

	// Pointer of the MEM-expander's attribute
	std::map<std::pair<std::string, std::string>, MemModule*> expMemStorage;
	// Module* expMem = NULL;
	int expMemModuleId = -1;

	Module* expCtx = NULL;
	
	bool state;
	bool oscReceived = false;

	dsp::BooleanTrigger connectTrigger;
	// BufferedTriggerParamQuantity expMemPrevQuantity;
	dsp::SchmittTrigger expMemPrevTrigger;
	// BufferedTriggerParamQuantity expMemNextQuantity;
	dsp::SchmittTrigger expMemNextTrigger;
	// BufferedTriggerParamQuantity expMemParamQuantity;
	dsp::SchmittTrigger expMemParamTrigger;

	MidiCatModule() {
		panelTheme = pluginSettings.panelThemeDefault;
		INFO("panelTheme: %i", panelTheme);
		// config(0, 0, 0, 0);
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PARAM_CONNECT, 0.0f, 1.0f, 0.0f, "Enable");
		configParam(PARAM_PREV, 0.f, 1.f, 0.f, "Scan for previous module mapping");
		configParam(PARAM_NEXT, 0.f, 1.f, 0.f, "Scan for next module mapping");
		configParam(PARAM_APPLY, 0.f, 1.f, 0.f, "Apply mapping");

		for (int id = 0; id < MAX_CHANNELS; id++) {
			paramHandleIndicator[id].color = mappingIndicatorColor;
			paramHandleIndicator[id].handle = &paramHandles[id];
			APP->engine->addParamHandle(&paramHandles[id]);
			midiParam[id].setLimits(0.0f, 1.0f, -1.0f);
		}
		indicatorDivider.setDivision(2048);
		lightDivider.setDivision(2048);
		midiResendDivider.setDivision(APP->engine->getSampleRate() / 2);
		onReset();
	}

	~MidiCatModule() {
		for (int id = 0; id < MAX_CHANNELS; id++) {
			APP->engine->removeParamHandle(&paramHandles[id]);
		}
	}

	void resetMapMemory() {
		for (auto it : expMemStorage) {
			delete it.second;
		}
		expMemStorage.clear();
	}

	void onReset() override {
		state=false;
		learningId = -1;
		learnedCc = false;
		learnedParam = false;
		clearMaps();
		mapLen = 1;
		for (int i = 0; i < MAX_CHANNELS; i++) {
			lastValueIn[i] = -1;
			lastValueOut[i] = -1;
			textLabel[i] = "";
			midiOptions[i] = 0;
			midiParam[i].reset();
		}
		locked = false;
		// midiInput.reset();
		midiOutput.reset();
		// midiOutput.midi::Output::reset();
		midiIgnoreDevices = false;
		midiResendPeriodically = false;
		midiResendDivider.reset();
		processDivision = 64;
		processDivider.setDivision(processDivision);
		processDivider.reset();
		// lightDivider.setDivision(processDivision*16);
		// lightDivider.reset();
		overlayEnabled = true;
		clearMapsOnLoad = false;
	}

	void onSampleRateChange() override {
		midiResendDivider.setDivision(APP->engine->getSampleRate() / 2);
	}

	void power() {
		if (state) {
			bool o = midiOutput.setup(ip, std::stoi(txPort));
			bool r = oscReceiver.setup(std::stoi(rxPort));
			state = o && r;
		}
		else{
			midiOutput.stop();
			oscReceiver.stop();
		}
	}
	
	void process(const ProcessArgs &args) override {
		ts++;
		vcvOscMessage rxMessage;
		while (oscReceiver.shift(&rxMessage)) {
			bool r = oscCc(rxMessage);
			oscReceived = oscReceived || r;
		}

		// Process trigger
		if (lightDivider.process() || oscReceived) {
			if (oscReceived) {
				// Orange
				lights[LIGHT_CONNECT].setBrightness(0.1f);
				lights[LIGHT_CONNECT + 1].setBrightness(1.0f);
			} else if (state) {
				// Green
				lights[LIGHT_CONNECT].setBrightness(1.0f);
				lights[LIGHT_CONNECT + 1].setBrightness(0.0f);
			} else {
				// Red
				lights[LIGHT_CONNECT].setBrightness(0.0f);
				lights[LIGHT_CONNECT + 1].setBrightness(1.0f);
			}  
		}

		// Only step channels when some midi event has been received. Additionally
		// step channels for parameter changes made manually every 128th loop. Notice
		// that midi allows about 1000 messages per second, so checking for changes more often
		// won't lead to higher precision on midi output.
		if (processDivider.process() || oscReceived) {
			// Step channels
			for (int id = 0; id < mapLen; id++) {
				int cc = -1;
				if (midiParam[id].oscController!=nullptr)
					cc = midiParam[id].oscController->getControllerId();

				if (cc < 0)
					continue;

				// Get Module
				Module* module = paramHandles[id].module;
				if (!module)
					continue;

				// Get ParamQuantity
				int paramId = paramHandles[id].paramId;
				ParamQuantity* paramQuantity = module->paramQuantities[paramId];
				if (!paramQuantity)
					continue;

				if (!paramQuantity->isBounded())
					continue;

				switch (midiMode) {
					case MIDIMODE::MIDIMODE_DEFAULT: {
						midiParam[id].paramQuantity = paramQuantity;
						float t = -1.0f;

						// Check if CC value has been set and changed
						if (cc >= 0 && oscReceived) {
							switch (midiParam[id].oscController->getCCMode()) {
								case CCMODE::DIRECT:
									if (lastValueIn[id] != midiParam[id].oscController->getValue()) {
										INFO("lastValueIn[%i] = %f, midiParam[%i].oscController->getValue() = %f", id, lastValueIn[id], id, midiParam[id].oscController->getValue());
										lastValueIn[id] = midiParam[id].oscController->getValue();
										t = midiParam[id].oscController->getValue();
									}
									break;
								case CCMODE::PICKUP1:
									// if (lastValueIn[id] != ccs[id].getValue()) {
									// 	if (midiParam[id].isNear(lastValueIn[id])) {
									// 		midiParam[id].resetFilter();
									// 		t = ccs[id].getValue();
									// 	}
									// 	lastValueIn[id] = ccs[id].getValue();
									// }
									break;
								case CCMODE::PICKUP2:
									// if (lastValueIn[id] != ccs[id].getValue()) {
									// 	if (midiParam[id].isNear(lastValueIn[id], ccs[id].getValue())) {
									// 		midiParam[id].resetFilter();
									// 		t = ccs[id].getValue();
									// 	}
									// 	lastValueIn[id] = ccs[id].getValue();
									// }
									break;
								case CCMODE::TOGGLE:
									// if (ccs[id].getValue() > 0 && (lastValueIn[id] == -1 || lastValueIn[id] >= 0)) {
									// 	t = midiParam[id].getLimitMax();
									// 	lastValueIn[id] = -2;
									// } 
									// else if (ccs[id].getValue() == 0 && lastValueIn[id] == -2) {
									// 	t = midiParam[id].getLimitMax();
									// 	lastValueIn[id] = -3;
									// }
									// else if (ccs[id].getValue() > 0 && lastValueIn[id] == -3) {
									// 	t = midiParam[id].getLimitMin();
									// 	lastValueIn[id] = -4;
									// }
									// else if (ccs[id].getValue() == 0 && lastValueIn[id] == -4) {
									// 	t = midiParam[id].getLimitMin();
									// 	lastValueIn[id] = -1;
									// }
									break;
								case CCMODE::TOGGLE_VALUE:
									// if (ccs[id].getValue() > 0 && (lastValueIn[id] == -1 || lastValueIn[id] >= 0)) {
									// 	t = ccs[id].getValue();
									// 	lastValueIn[id] = -2;
									// } 
									// else if (ccs[id].getValue() == 0 && lastValueIn[id] == -2) {
									// 	t = midiParam[id].getValue();
									// 	lastValueIn[id] = -3;
									// }
									// else if (ccs[id].getValue() > 0 && lastValueIn[id] == -3) {
									// 	t = midiParam[id].getLimitMin();
									// 	lastValueIn[id] = -4;
									// }
									// else if (ccs[id].getValue() == 0 && lastValueIn[id] == -4) {
									// 	t = midiParam[id].getLimitMin();
									// 	lastValueIn[id] = -1;
									// }
									break;
							}
						}

						// Set a new value for the mapped parameter
						if (t >= 0.f) {
							INFO("midiParam[%i].setValue(%f)", id, t);
							midiParam[id].setValue(t);
							if (overlayEnabled && overlayQueue.capacity() > 0) {
								overlayQueue.push(id);
								}
						}

						// Apply value on the mapped parameter (respecting slew and scale)
						midiParam[id].process(args.sampleTime * float(processDivision));

						// Retrieve the current value of the parameter (ignoring slew and scale)
						float v = midiParam[id].getValue();

						// Midi feedback
						if (lastValueOut[id] != v) {
							if (cc >= 0 && midiParam[id].oscController->getCCMode() == CCMODE::DIRECT)
								lastValueIn[id] = v;
							this->midiOutput.sendOscFeedback(midiParam[id].oscController->getAddress(), midiParam[id].oscController->getControllerId(), v);
							midiParam[id].oscController->setValue(v, 0);
							lastValueOut[id] = v;
						}
					} break;

					case MIDIMODE::MIDIMODE_LOCATE: {
						bool indicate = false;
						if ((cc >= 0 && midiParam[id].oscController->getValue() >= 0) && lastValueInIndicate[id] != midiParam[id].oscController->getValue()) {
							lastValueInIndicate[id] = midiParam[id].oscController->getValue();
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

		if (midiResendPeriodically && midiResendDivider.process()) {
			midiResendFeedback();
		}

		// Expanders
		bool expCtxFound = false;
		Module* exp = rightExpander.module;
		for (int i = 0; i < 1; i++) {
			if (!exp) break;
			if (exp->model == modelMidiCatCtx && !expCtxFound) {
				expCtx = exp;
				expCtxFound = true;
				exp = exp->rightExpander.module;
				continue;
			}
			break;
		}
		if (!expCtxFound) {
			expCtx = NULL;
		}
	}

	void setMode(MIDIMODE midiMode) {
		if (this->midiMode == midiMode)
			return;
		this->midiMode = midiMode;
		switch (midiMode) {
			case MIDIMODE::MIDIMODE_LOCATE:
				for (int i = 0; i < MAX_CHANNELS; i++) 
					lastValueInIndicate[i] = std::fmax(0, lastValueIn[i]);
				break;
			default:
				break;
		}
	}

	bool oscCc(vcvOscMessage msg) {
		uint8_t controllerId = msg.getArgAsInt(0);
		float value = msg.getArgAsFloat(1);
		std::string address = msg.getAddress();
		bool midiReceived =false;
		// Learn
		if (learningId >= 0 && (learnedCcLast != controllerId || lastLearnedAddress != address)) {
			INFO("learningId %i, controllerId %i, value %f address %s", learningId, controllerId, value, address.c_str());
			midiParam[learningId].oscController=vcvOscController::Create(address, controllerId, value, ts);
			midiParam[learningId].oscController->setCCMode(CCMODE::DIRECT);
			learnedCc = true;
			lastLearnedAddress = address;
			learnedCcLast = controllerId;
			commitLearn();
			updateMapLen();
			refreshParamHandleText(learningId);
		}
		else 
		{
			INFO("%s %i: value %f", address.c_str(), controllerId, value);
			for (int id=0; id < mapLen; id++)
			{
				if (midiParam[id].oscController != nullptr &&
					(midiParam[id].oscController->getControllerId() == controllerId && midiParam[id].oscController->getAddress() == address))
				{
					INFO("midiParam[%i].oscController->setValue(%f, %i, %f)", id, value,ts,midiParam[id].oscController->getValue());
					midiReceived = midiParam[id].oscController->setValue(value, ts);
					return midiReceived;
				}
			}
		}
		if(!midiReceived) oscReceiver.onMessage(msg);
		return midiReceived;
	}

	void midiResendFeedback() {
		for (int i = 0; i < MAX_CHANNELS; i++) {
			lastValueOut[i] = -1;
		}
	}

	void clearMap(int id, bool midiOnly = false) {
		learningId = -1;
		midiParam[id].oscController=nullptr;
		midiOptions[id] = 0;
		midiParam[id].reset();
		if (!midiOnly) {
			textLabel[id] = "";
			APP->engine->updateParamHandle(&paramHandles[id], -1, 0, true);
			updateMapLen();
			refreshParamHandleText(id);
		}
	}

	void clearMaps() {
		learningId = -1;
		for (int id = 0; id < MAX_CHANNELS; id++) {
			textLabel[id] = "";
			midiOptions[id] = 0;
			midiParam[id].reset();
			midiParam[id].oscController=nullptr;
			APP->engine->updateParamHandle(&paramHandles[id], -1, 0, true);
			refreshParamHandleText(id);
		}
		mapLen = 1;
		expMemModuleId = -1;
	}

	void updateMapLen() {
		// Find last nonempty map
		int id;
		for (id = MAX_CHANNELS - 1; id >= 0; id--) {
			if (paramHandles[id].moduleId >= 0)
				break;
		}
		mapLen = id + 1;
		// Add an empty "Mapping..." slot
		if (mapLen < MAX_CHANNELS) {
			mapLen++;
		}
	}

	void commitLearn() {
		if (learningId < 0)
			return;
		if (!learnedCc)
			return;
		if (!learnedParam && paramHandles[learningId].moduleId < 0)
			return;
		// Reset learned state
		learnedCc = false;
		learnedParam = false;
		// Copy modes from the previous slot
		if (learningId > 0) {
			// midiParam[learningId].oscController->setCCMode(midiParam[learningId - 1].oscController->getCCMode());
			midiOptions[learningId] = midiOptions[learningId - 1];
			midiParam[learningId].setSlew(midiParam[learningId - 1].getSlew());
			midiParam[learningId].setMin(midiParam[learningId - 1].getMin());
			midiParam[learningId].setMax(midiParam[learningId - 1].getMax());
		}
		textLabel[learningId] = "";

		// Find next incomplete map
		while (!learnSingleSlot && ++learningId < MAX_CHANNELS) {
			if ((midiParam[learningId].oscController==nullptr) || paramHandles[learningId].moduleId < 0)
				return;
		}
		learningId = -1;
	}

	int enableLearn(int id, bool learnSingle = false) {
		if (id == -1) {
			// Find next incomplete map
			while (++id < MAX_CHANNELS) {
				if (midiParam[id].oscController==nullptr && paramHandles[id].moduleId < 0)
					break;
			}
			if (id == MAX_CHANNELS) {
				return -1;
			}
		}

		if (id == mapLen) {
			disableLearn();
			return -1;
		}
		if (learningId != id) {
			learningId = id;
			learnedCc = false;
			learnedCcLast = -1;
			lastLearnedAddress = "";
			learnedParam = false;
			learnSingleSlot = learnSingle;
		}
		return id;
	}

	void disableLearn() {
		learningId = -1;
	}

	void disableLearn(int id) {
		if (learningId == id) {
			learningId = -1;
		}
	}

	void learnParam(int id, int moduleId, int paramId, bool resetMidiSettings = true) {
		APP->engine->updateParamHandle(&paramHandles[id], moduleId, paramId, true);
		midiParam[id].reset(resetMidiSettings);
		learnedParam = true;
		commitLearn();
		updateMapLen();
	}

	void moduleBind(Module* m, bool keepCcAndNote) {
		if (!m) return;
		if (!keepCcAndNote) {
			clearMaps();
		}
		else {
			// Clean up some additional mappings on the end
			for (int i = int(m->params.size()); i < mapLen; i++) {
				APP->engine->updateParamHandle(&paramHandles[i], -1, -1, true);
			}
		}
		for (size_t i = 0; i < m->params.size() && i < MAX_CHANNELS; i++) {
			learnParam(int(i), m->id, int(i));
		}

		updateMapLen();
	}

	void moduleBindExpander(bool keepCcAndNote) {
		Module::Expander* exp = &leftExpander;
		if (exp->moduleId < 0) return;
		Module* m = exp->module;
		if (!m) return;
		moduleBind(m, keepCcAndNote);
	}

	void refreshParamHandleText(int id) {
		std::string text = "MIDI-CAT";
		if (id >=0 && midiParam[id].oscController!=nullptr) {
			text += string::f(" cc%02d", midiParam[id].oscController->getControllerId());
		}
		paramHandles[id].text = text;
	}

	void expMemSave(std::string pluginSlug, std::string moduleSlug) {
		MemModule* m = new MemModule;
		Module* module = NULL;
		for (size_t i = 0; i < MAX_CHANNELS; i++) {
			if (paramHandles[i].moduleId < 0) continue;
			if (paramHandles[i].module->model->plugin->slug != pluginSlug && paramHandles[i].module->model->slug == moduleSlug) continue;
			module = paramHandles[i].module;

			MemParam* p = new MemParam;
			p->paramId = paramHandles[i].paramId;
			p->cc = midiParam[i].oscController ? midiParam[i].oscController->getControllerId() : -1;
			p->address = midiParam[i].oscController ? midiParam[i].oscController->getAddress() : "";
			p->ccMode = midiParam[i].oscController ? midiParam[i].oscController->getCCMode() : CCMODE::DIRECT;
			p->label = textLabel[i];
			p->midiOptions = midiOptions[i];
			m->paramMap.push_back(p);
		}
		m->pluginName = module->model->plugin->name;
		m->moduleName = module->model->name;

		auto p = std::pair<std::string, std::string>(pluginSlug, moduleSlug);
		auto it = expMemStorage.find(p);
		if (it != expMemStorage.end()) {
			delete it->second;
		}

		expMemStorage[p] = m;
	}

	void expMemDelete(std::string pluginSlug, std::string moduleSlug) {
		auto p = std::pair<std::string, std::string>(pluginSlug, moduleSlug);
		auto it = expMemStorage.find(p);
		delete it->second;
		expMemStorage.erase(p);
	}

	void expMemApply(Module* m) {
		if (!m) return;
		auto p = std::pair<std::string, std::string>(m->model->plugin->slug, m->model->slug);
		auto it = expMemStorage.find(p);
		if (it == expMemStorage.end()) return;
		MemModule* map = it->second;

		clearMaps();
		expMemModuleId = m->id;
		int i = 0;
		for (MemParam* it : map->paramMap) {
			learnParam(i, m->id, it->paramId);
			if(it->address!="") {
				midiParam[i].oscController=vcvOscController::Create(it->address, it->cc);
				midiParam[i].oscController->setCCMode(it->ccMode);
			}
			textLabel[i] = it->label;
			midiOptions[i] = it->midiOptions;
			i++;
		}
		updateMapLen();
	}

	bool expMemTest(Module* m) {
		if (!m) return false;
		auto p = std::pair<std::string, std::string>(m->model->plugin->slug, m->model->slug);
		auto it = expMemStorage.find(p);
		if (it == expMemStorage.end()) return false;
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
		json_object_set_new(rootJ, "state", json_boolean(state));
		json_object_set_new(rootJ, "ip", json_string(ip.c_str()));
		json_object_set_new(rootJ, "txPort", json_string(txPort.c_str()));
		json_object_set_new(rootJ, "rxPort", json_string(rxPort.c_str()));

		json_t* expMemStorageJ = json_array();
		for (auto it : expMemStorage) {
			json_t* expMemStorageJJ = json_object();
			json_object_set_new(expMemStorageJJ, "pluginSlug", json_string(it.first.first.c_str()));
			json_object_set_new(expMemStorageJJ, "moduleSlug", json_string(it.first.second.c_str()));

			auto a = it.second;
			json_object_set_new(expMemStorageJJ, "pluginName", json_string(a->pluginName.c_str()));
			json_object_set_new(expMemStorageJJ, "moduleName", json_string(a->moduleName.c_str()));
			json_t* paramMapJ = json_array();
			for (auto p : a->paramMap) {
				json_t* paramMapJJ = json_object();
				json_object_set_new(paramMapJJ, "paramId", json_integer(p->paramId));
				json_object_set_new(paramMapJJ, "cc", json_integer(p->cc));
				json_object_set_new(paramMapJJ, "address", json_string(p->address.c_str()));
				json_object_set_new(paramMapJJ, "ccMode", json_integer((int)p->ccMode));
				json_object_set_new(paramMapJJ, "label", json_string(p->label.c_str()));
				json_object_set_new(paramMapJJ, "midiOptions", json_integer(p->midiOptions));
				json_array_append_new(paramMapJ, paramMapJJ);
			}
			json_object_set_new(expMemStorageJJ, "paramMap", paramMapJ);

			json_array_append_new(expMemStorageJ, expMemStorageJJ);
		}
		json_object_set_new(rootJ, "expMemStorage", expMemStorageJ);

		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		json_object_set_new(rootJ, "textScrolling", json_boolean(textScrolling));
		json_object_set_new(rootJ, "mappingIndicatorHidden", json_boolean(mappingIndicatorHidden));
		json_object_set_new(rootJ, "locked", json_boolean(locked));
		json_object_set_new(rootJ, "processDivision", json_integer(processDivision));
		json_object_set_new(rootJ, "overlayEnabled", json_boolean(overlayEnabled));
		json_object_set_new(rootJ, "clearMapsOnLoad", json_boolean(clearMapsOnLoad));

		json_t* mapsJ = json_array();
		for (int id = 0; id < mapLen; id++) {
			json_t* mapJ = json_object();
			json_object_set_new(mapJ, "moduleId", json_integer(paramHandles[id].moduleId));
			json_object_set_new(mapJ, "paramId", json_integer(paramHandles[id].paramId));
			json_object_set_new(mapJ, "label", json_string(textLabel[id].c_str()));
			json_object_set_new(mapJ, "midiOptions", json_integer(midiOptions[id]));
			json_array_append_new(mapsJ, mapJ);
			if (id >= 0 && midiParam[id].oscController!=nullptr) {
				json_object_set_new(mapJ, "cc", json_integer(midiParam[id].oscController->getControllerId()));
				json_object_set_new(mapJ, "ccMode", json_integer((int)midiParam[id].oscController->getCCMode()));
				json_object_set_new(mapJ, "address", json_string(midiParam[id].oscController->getAddress().c_str()));
			}
		}
		json_object_set_new(rootJ, "maps", mapsJ);

		json_object_set_new(rootJ, "midiResendPeriodically", json_boolean(midiResendPeriodically));
		json_object_set_new(rootJ, "midiIgnoreDevices", json_boolean(midiIgnoreDevices));
		// json_object_set_new(rootJ, "midiInput", midiInput.toJson());
		// json_object_set_new(rootJ, "midiOutput", midiOutput.toJson());
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* stateJ = json_object_get(rootJ, "state");
		if (stateJ) state = json_boolean_value(stateJ);
		json_t* ipJ = json_object_get(rootJ, "ip");
		if (ipJ) ip = json_string_value(ipJ);
		json_t* txPortJ = json_object_get(rootJ, "txPort");
		if (txPortJ) txPort = json_string_value(txPortJ);
		json_t* rxPortJ = json_object_get(rootJ, "rxPort");
		if (rxPortJ) rxPort = json_string_value(rxPortJ);

		resetMapMemory();
		json_t* expMemStorageJ = json_object_get(rootJ, "expMemStorage");
		size_t i;
		json_t* expMemStorageJJ;
		json_array_foreach(expMemStorageJ, i, expMemStorageJJ) {
			std::string pluginSlug = json_string_value(json_object_get(expMemStorageJJ, "pluginSlug"));
			std::string moduleSlug = json_string_value(json_object_get(expMemStorageJJ, "moduleSlug"));

			MemModule* a = new MemModule;
			a->pluginName = json_string_value(json_object_get(expMemStorageJJ, "pluginName"));
			a->moduleName = json_string_value(json_object_get(expMemStorageJJ, "moduleName"));
			json_t* paramMapJ = json_object_get(expMemStorageJJ, "paramMap");
			size_t j;
			json_t* paramMapJJ;
			json_array_foreach(paramMapJ, j, paramMapJJ) {
				MemParam* p = new MemParam;
				p->paramId = json_integer_value(json_object_get(paramMapJJ, "paramId"));
				p->cc = json_integer_value(json_object_get(paramMapJJ, "cc"));
				p->address = json_string_value(json_object_get(paramMapJJ, "address"));
				p->ccMode = (CCMODE)json_integer_value(json_object_get(paramMapJJ, "ccMode"));
				p->label = json_string_value(json_object_get(paramMapJJ, "label"));
				p->midiOptions = json_integer_value(json_object_get(paramMapJJ, "midiOptions"));
				a->paramMap.push_back(p);
			}
			expMemStorage[std::pair<std::string, std::string>(pluginSlug, moduleSlug)] = a;
		}

		json_t* panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ) panelTheme = json_integer_value(panelThemeJ);

		json_t* textScrollingJ = json_object_get(rootJ, "textScrolling");
		if (textScrollingJ) textScrolling = json_boolean_value(textScrollingJ);
		json_t* mappingIndicatorHiddenJ = json_object_get(rootJ, "mappingIndicatorHidden");
		if (mappingIndicatorHiddenJ) mappingIndicatorHidden = json_boolean_value(mappingIndicatorHiddenJ);
		json_t* lockedJ = json_object_get(rootJ, "locked");
		if (lockedJ) locked = json_boolean_value(lockedJ);
		json_t* processDivisionJ = json_object_get(rootJ, "processDivision");
		if (processDivisionJ) processDivision = json_integer_value(processDivisionJ);
		json_t* overlayEnabledJ = json_object_get(rootJ, "overlayEnabled");
		if (overlayEnabledJ) overlayEnabled = json_boolean_value(overlayEnabledJ);
		json_t* clearMapsOnLoadJ = json_object_get(rootJ, "clearMapsOnLoad");
		if (clearMapsOnLoadJ) clearMapsOnLoad = json_boolean_value(clearMapsOnLoadJ);

		if (clearMapsOnLoad) {
			clearMaps();
		}

		json_t* mapsJ = json_object_get(rootJ, "maps");
		if (mapsJ) {
			json_t* mapJ;
			size_t mapIndex;
			json_array_foreach(mapsJ, mapIndex, mapJ) {
				if (mapIndex >= MAX_CHANNELS) {
					continue;
				}

				json_t* ccJ = json_object_get(mapJ, "cc");
				json_t* addressJ = json_object_get(mapJ, "address");
				json_t* ccModeJ = json_object_get(mapJ, "ccMode");
				json_t* moduleIdJ = json_object_get(mapJ, "moduleId");
				json_t* paramIdJ = json_object_get(mapJ, "paramId");
				json_t* labelJ = json_object_get(mapJ, "label");
				json_t* midiOptionsJ = json_object_get(mapJ, "midiOptions");

				if (!(moduleIdJ || paramIdJ)) {
					APP->engine->updateParamHandle(&paramHandles[mapIndex], -1, 0, true);
				}
				if(json_integer_value(ccJ)>0){
					midiParam[mapIndex].oscController=vcvOscController::Create(json_string_value(addressJ),
																   json_integer_value(ccJ));
					midiParam[mapIndex].oscController->setCCMode((CCMODE)json_integer_value(ccModeJ));
				}

				midiOptions[mapIndex] = json_integer_value(midiOptionsJ);
				int moduleId = moduleIdJ ? json_integer_value(moduleIdJ) : -1;
				int paramId = paramIdJ ? json_integer_value(paramIdJ) : 0;
				if (moduleId >= 0) {
					moduleId = idFix(moduleId);
					if (moduleId != paramHandles[mapIndex].moduleId || paramId != paramHandles[mapIndex].paramId) {
						APP->engine->updateParamHandle(&paramHandles[mapIndex], moduleId, paramId, false);
						refreshParamHandleText(mapIndex);
					}
				}
				if (labelJ) textLabel[mapIndex] = json_string_value(labelJ);
			}
		}

		updateMapLen();
		idFixClearMap();
		
		json_t* midiResendPeriodicallyJ = json_object_get(rootJ, "midiResendPeriodically");
		if (midiResendPeriodicallyJ) midiResendPeriodically = json_boolean_value(midiResendPeriodicallyJ);

		if (!midiIgnoreDevices) {
			json_t* midiIgnoreDevicesJ = json_object_get(rootJ, "midiIgnoreDevices");
			if (midiIgnoreDevicesJ)	midiIgnoreDevices = json_boolean_value(midiIgnoreDevicesJ);
			// json_t* midiInputJ = json_object_get(rootJ, "midiInput");
			// if (midiInputJ) midiInput.fromJson(midiInputJ);
			// json_t* midiOutputJ = json_object_get(rootJ, "midiOutput");
			// if (midiOutputJ) midiOutput.fromJson(midiOutputJ);
			power();
		}
	}
};


struct MidiCatChoice : MapModuleChoice<MAX_CHANNELS, MidiCatModule> {
	MidiCatChoice() {
		textOffset = Vec(6.f, 14.7f);
		color = nvgRGB(0xDA, 0xa5, 0x20);
	}

	std::string getSlotPrefix() override {
		if (module->midiParam[id].oscController!=nullptr) {
			return string::f("cc%02d ", module->midiParam[id].oscController->getControllerId());
		}
		else if (module->paramHandles[id].moduleId >= 0) {
			return ".... ";
		}
		else {
			return "";
		}
	}

	std::string getSlotLabel() override {
		return module->textLabel[id];
	}

	void appendContextMenu(Menu* menu) override {
		struct UnmapMidiItem : MenuItem {
			MidiCatModule* module;
			int id;
			void onAction(const event::Action& e) override {
				module->clearMap(id, true);
			}
		}; // struct UnmapMidiItem

		struct CcModeMenuItem : MenuItem {
			MidiCatModule* module;
			int id;

			CcModeMenuItem() {
				rightText = RIGHT_ARROW;
			}

			struct CcModeItem : MenuItem {
				MidiCatModule* module;
				int id;
				CCMODE ccMode;

				void onAction(const event::Action& e) override {
					module->midiParam[id].oscController->setCCMode(ccMode);
				}
				void step() override {
					rightText = module->midiParam[id].oscController->getCCMode() == ccMode ? "✔" : "";
					MenuItem::step();
				}
			};

			Menu* createChildMenu() override {
				Menu* menu = new Menu;
				menu->addChild(construct<CcModeItem>(&MenuItem::text, "Direct", &CcModeItem::module, module, &CcModeItem::id, id, &CcModeItem::ccMode, CCMODE::DIRECT));
				menu->addChild(construct<CcModeItem>(&MenuItem::text, "Pickup (snap)", &CcModeItem::module, module, &CcModeItem::id, id, &CcModeItem::ccMode, CCMODE::PICKUP1));
				menu->addChild(construct<CcModeItem>(&MenuItem::text, "Pickup (jump)", &CcModeItem::module, module, &CcModeItem::id, id, &CcModeItem::ccMode, CCMODE::PICKUP2));
				menu->addChild(construct<CcModeItem>(&MenuItem::text, "Toggle", &CcModeItem::module, module, &CcModeItem::id, id, &CcModeItem::ccMode, CCMODE::TOGGLE));
				menu->addChild(construct<CcModeItem>(&MenuItem::text, "Toggle + Value", &CcModeItem::module, module, &CcModeItem::id, id, &CcModeItem::ccMode, CCMODE::TOGGLE_VALUE));
				return menu;
			}
		}; // struct CcModeMenuItem
		
		if (module->midiParam[id].oscController!=nullptr) {
			menu->addChild(construct<UnmapMidiItem>(&MenuItem::text, "Clear MIDI assignment", &UnmapMidiItem::module, module, &UnmapMidiItem::id, id));
			menu->addChild(new MenuSeparator());
			menu->addChild(construct<CcModeMenuItem>(&MenuItem::text, "Input mode for CC", &CcModeMenuItem::module, module, &CcModeMenuItem::id, id));
		}

		struct LabelMenuItem : MenuItem {
			MidiCatModule* module;
			int id;

			LabelMenuItem() {
				rightText = RIGHT_ARROW;
			}

			struct LabelField : ui::TextField {
				MidiCatModule* module;
				int id;
				void onSelectKey(const event::SelectKey& e) override {
					if (e.action == GLFW_PRESS && e.key == GLFW_KEY_ENTER) {
						module->textLabel[id] = text;

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
				MidiCatModule* module;
				int id;
				void onAction(const event::Action& e) override {
					module->textLabel[id] = "";
				}
			};

			Menu* createChildMenu() override {
				Menu* menu = new Menu;

				LabelField* labelField = new LabelField;
				labelField->placeholder = "Label";
				labelField->text = module->textLabel[id];
				labelField->box.size.x = 180;
				labelField->module = module;
				labelField->id = id;
				menu->addChild(labelField);

				ResetItem* resetItem = new ResetItem;
				resetItem->text = "Reset";
				resetItem->module = module;
				resetItem->id = id;
				menu->addChild(resetItem);

				return menu;
			}
		}; // struct LabelMenuItem

		menu->addChild(construct<LabelMenuItem>(&MenuItem::text, "Custom label", &LabelMenuItem::module, module, &LabelMenuItem::id, id));
	}
};

struct OscWidget : widget::OpaqueWidget {
	MidiCatModule* module;
	StoermelderTextField* ip;
	StoermelderTextField* txPort;
	StoermelderTextField* rxPort;
	NVGcolor color = nvgRGB(0xDA, 0xa5, 0x20);

	void step() override {
		if (!module) return;

		if(module->panelTheme==1) {
			ip->color = txPort->color = rxPort->color = color::WHITE;
		}
		else{
			ip->color = txPort->color = rxPort->color = color;
		}

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

	void setMidiPort(std::string ipT, std::string rPort, std::string tPort) {
		clearChildren();
		math::Vec pos;

		StoermelderTextField* ip = createWidget<StoermelderTextField>(pos);
		ip->box.size = Vec(100.0f, 15.15f);
		ip->maxTextLength=15;
		ip->text = ipT;
		addChild(ip);
		pos = ip->box.getTopRight();
		pos.x=pos.x+1;
		this->ip = ip;

		StoermelderTextField* txPort = createWidget<StoermelderTextField>(pos);
		txPort->box.size = Vec(37.0f, 15.15f);
		txPort->text = tPort;
		addChild(txPort);
		pos = txPort->box.getTopRight();
		pos.x=pos.x+1;
		this->txPort = txPort;

		StoermelderTextField* rxPort = createWidget<StoermelderTextField>(pos);
		rxPort->box.size = Vec(37.0f, 15.15f);
		rxPort->text = rPort;
		addChild(rxPort);
		this->rxPort = rxPort;
	}
};

struct MidiCatDisplay : MapModuleDisplay<MAX_CHANNELS, MidiCatModule, MidiCatChoice>, OverlayMessageProvider {
	void step() override {
		if (module) {
			int mapLen = module->mapLen;
			for (int id = 0; id < MAX_CHANNELS; id++) {
				choices[id]->visible = (id < mapLen);
			}
		}
		MapModuleDisplay<MAX_CHANNELS, MidiCatModule, MidiCatChoice>::step();
	}

	int nextOverlayMessageId() override {
		if (module->overlayQueue.empty())
			return -1;
		return module->overlayQueue.shift();
	}

	void getOverlayMessage(int id, Message& m) override {
		ParamQuantity* paramQuantity = choices[id]->getParamQuantity();
		if (!paramQuantity) return;

		std::string label = choices[id]->getSlotLabel();
		if (label == "") label = paramQuantity->label;

		m.title = paramQuantity->getDisplayValueString() + paramQuantity->getUnit();
		m.subtitle[0] = paramQuantity->module->model->name;
		m.subtitle[1] = label;
	}
};

struct MidiCatWidget : ThemedModuleWidget<MidiCatModule>, ParamWidgetContextExtender {
	MidiCatModule* module;
	MidiCatDisplay* mapWidget;

	// Module* expMem;
	BufferedTriggerParamQuantity expMemPrevQuantity;
	dsp::BooleanTrigger connectTrigger;
	dsp::SchmittTrigger expMemPrevTrigger;
	BufferedTriggerParamQuantity expMemNextQuantity;
	dsp::SchmittTrigger expMemNextTrigger;
	BufferedTriggerParamQuantity expMemParamQuantity;
	dsp::SchmittTrigger expMemParamTrigger;

	MidiCatCtxBase* expCtx;
	BufferedTriggerParamQuantity* expCtxMapQuantity;
	dsp::SchmittTrigger expCtxMapTrigger;

	enum class LEARN_MODE {
		OFF = 0,
		BIND_CLEAR = 1,
		BIND_KEEP = 2,
		MEM = 3
	};

	LEARN_MODE learnMode = LEARN_MODE::OFF;

	MidiCatWidget(MidiCatModule* module) : ThemedModuleWidget<MidiCatModule>(module, "MidiCat") {
		setModule(module);
		this->module = module;

		addChild(createWidget<StoermelderBlackScrew>(Vec(0, 0)));
		addChild(createWidget<StoermelderBlackScrew>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<StoermelderBlackScrew>(Vec(0, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<StoermelderBlackScrew>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		mapWidget = createWidget<MidiCatDisplay>(Vec(19.5f, 74.9f));
		mapWidget->box.size = Vec(185.8f, 221.5f);
		mapWidget->setModule(module);
		addChild(mapWidget);
		math::Vec inpPos = mapWidget->box.getBottomLeft();
		inpPos.x-=5.0f;
		inpPos.y+=14;

		OscWidget* oscConfigWidget = createWidget<OscWidget>(inpPos);
		oscConfigWidget->box.size = Vec(185.0f, 36.0f);
		oscConfigWidget->module = module;
		if (module) {
			oscConfigWidget->setMidiPort(module ? module->ip : NULL,
											module ? module->rxPort : NULL,
											module ? module->txPort : NULL);
		}
		addChild(oscConfigWidget);

		inpPos.x+=185.0f;
		inpPos.y+=7.8f;

		// Power switch
		addChild(createParamCentered<TL1105>(inpPos, module, MidiCatModule::PARAM_CONNECT));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(inpPos, module, MidiCatModule::LIGHT_CONNECT));

		// Eyes
		addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(35.8f, 32.8f), module, MidiCatModule::LIGHT_CONNECT));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(56.1f, 35.5f), module, MidiCatModule::LIGHT_CONNECT));
		
		// Memory
		inpPos = oscConfigWidget->box.getBottomLeft();
		inpPos.x+=65.0f;
		addChild(createParamCentered<MatrixBackButton>(inpPos, module, MidiCatModule::PARAM_PREV));
		
		inpPos.x+=31.0f;
		addChild(createParamCentered<TL1105>(inpPos, module, MidiCatModule::PARAM_APPLY));
		addChild(createLightCentered<SmallLight<WhiteLight>>(inpPos, module, MidiCatModule::LIGHT_APPLY));
		
		inpPos.x+=31.0f;
		addChild(createParamCentered<MatrixButton>(inpPos, module, MidiCatModule::PARAM_NEXT));
		

		if (module) {
			OverlayMessageWidget::registerProvider(mapWidget);
		}
	}

	~MidiCatWidget() {
		if (learnMode != LEARN_MODE::OFF) {
			glfwSetCursor(APP->window->win, NULL);
		}

		if (module) {
			OverlayMessageWidget::unregisterProvider(mapWidget);
		}
	}

	void step() override {
		ThemedModuleWidget<MidiCatModule>::step();
		if (module) {
			if (connectTrigger.process(module->params[MidiCatModule::PARAM_CONNECT].getValue() > 0.0f)) {
				INFO("IP: %s,%s,%s", module->ip.c_str(), module->rxPort.c_str(), module->txPort.c_str());
				module->state ^= true;
				module->power();
			}

			if (expMemPrevTrigger.process(module->params[MidiCatModule::PARAM_PREV].getValue())) {
				// expMemPrevQuantity.resetBuffer();
				INFO("PREV");
				expMemPrevModule();
			}
			if (expMemNextTrigger.process(module->params[MidiCatModule::PARAM_NEXT].getValue())) {
				// expMemNextQuantity.resetBuffer();
				INFO("NEXT");
				expMemNextModule();
			}
			if (expMemParamTrigger.process(module->params[MidiCatModule::PARAM_APPLY].getValue())) {
				// expMemParamQuantity.resetBuffer();
				INFO("APPLY");
				enableLearn(LEARN_MODE::MEM);
			}

			module->lights[MidiCatModule::LIGHT_APPLY].setBrightness(learnMode == LEARN_MODE::MEM);
			// }

			// CTX-expander
			if (module->expCtx != (Module*)expCtx) {
				expCtx = dynamic_cast<MidiCatCtxBase*>(module->expCtx);
				if (expCtx) {
					expCtxMapQuantity = dynamic_cast<BufferedTriggerParamQuantity*>(expCtx->paramQuantities[0]);
					expCtxMapQuantity->resetBuffer();
				}
			}
			if (expCtx) {
				if (expCtxMapTrigger.process(expCtxMapQuantity->buffer)) {
					expCtxMapQuantity->resetBuffer();
					module->enableLearn(-1, true);
				}
			}
		}

		ParamWidgetContextExtender::step();
	}

	void expMemPrevModule() {
		std::list<Widget*> modules = APP->scene->rack->moduleContainer->children;
		auto sort = [&](Widget* w1, Widget* w2) {
			auto t1 = std::make_tuple(w1->box.pos.y, w1->box.pos.x);
			auto t2 = std::make_tuple(w2->box.pos.y, w2->box.pos.x);
			return t1 > t2;
		};
		modules.sort(sort);
		expMemScanModules(modules);
	}

	void expMemNextModule() {
		std::list<Widget*> modules = APP->scene->rack->moduleContainer->children;
		auto sort = [&](Widget* w1, Widget* w2) {
			auto t1 = std::make_tuple(w1->box.pos.y, w1->box.pos.x);
			auto t2 = std::make_tuple(w2->box.pos.y, w2->box.pos.x);
			return t1 < t2;
		};
		modules.sort(sort);
		expMemScanModules(modules);
	}

	void expMemScanModules(std::list<Widget*>& modules) {
		f:
		std::list<Widget*>::iterator it = modules.begin();
		// Scan for current module in the list
		if (module->expMemModuleId != -1) {
			for (; it != modules.end(); it++) {
				ModuleWidget* mw = dynamic_cast<ModuleWidget*>(*it);
				Module* m = mw->module;
				if (m->id == module->expMemModuleId) {
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
			if (module->expMemTest(m)) {
				module->expMemApply(m);
				return;
			}
		}
		// No module found yet -> retry from the beginning
		if (module->expMemModuleId != -1) {
			module->expMemModuleId = -1;
			goto f;
		}
	}

	void extendParamWidgetContextMenu(ParamWidget* pw, Menu* menu) override {
		if (!module) return;
		if (module->learningId >= 0) return;
		ParamQuantity* pq = pw->paramQuantity;
		if (!pq) return;
		
		struct MidiCatBeginItem : MenuLabel {
			MidiCatBeginItem() {
				text = "MIDI-CAT";
			}
		};

		struct MidiCatEndItem : MenuEntry {
			MidiCatEndItem() {
				box.size = Vec();
			}
		};

		struct MapMenuItem : MenuItem {
			MidiCatModule* module;
			ParamQuantity* pq;
			int currentId = -1;

			MapMenuItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				struct MapItem : MenuItem {
					MidiCatModule* module;
					int currentId;
					void onAction(const event::Action& e) override {
						module->enableLearn(currentId, true);
					}
				};

				struct MapEmptyItem : MenuItem {
					MidiCatModule* module;
					ParamQuantity* pq;
					void onAction(const event::Action& e) override {
						int id = module->enableLearn(-1, true);
						if (id >= 0) module->learnParam(id, pq->module->id, pq->paramId);
					}
				};

				struct RemapItem : MenuItem {
					MidiCatModule* module;
					ParamQuantity* pq;
					int id;
					int currentId;
					void onAction(const event::Action& e) override {
						module->learnParam(id, pq->module->id, pq->paramId, false);
					}
					void step() override {
						rightText = CHECKMARK(id == currentId);
						MenuItem::step();
					}
				};

				Menu* menu = new Menu;
				if (currentId < 0) {
					menu->addChild(construct<MapEmptyItem>(&MenuItem::text, "Learn MIDI", &MapEmptyItem::module, module, &MapEmptyItem::pq, pq));
				}
				else {
					menu->addChild(construct<MapItem>(&MenuItem::text, "Learn MIDI", &MapItem::module, module, &MapItem::currentId, currentId));
				}

				if (module->mapLen > 0) {
					menu->addChild(new MenuSeparator);
					for (int i = 0; i < module->mapLen; i++) {
						if (module->midiParam[i].oscController!=nullptr) {
							std::string text;
							if (module->textLabel[i] != "") {
								text = module->textLabel[i];
							}
							else if (module->midiParam[i].oscController->getControllerId() >= 0) {
								text = string::f("MIDI CC %02d", module->midiParam[i].oscController->getControllerId());
							}
							menu->addChild(construct<RemapItem>(&MenuItem::text, text, &RemapItem::module, module, &RemapItem::pq, pq, &RemapItem::id, i, &RemapItem::currentId, currentId));
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
				MidiCatBeginItem* ml = dynamic_cast<MidiCatBeginItem*>(*it);
				if (ml) { itCvBegin = it; continue; }
			}
			else {
				MidiCatEndItem* ml = dynamic_cast<MidiCatEndItem*>(*it);
				if (ml) { itCvEnd = it; break; }
			}
		}

		for (int id = 0; id < module->mapLen; id++) {
			if (module->paramHandles[id].moduleId == pq->module->id && module->paramHandles[id].paramId == pq->paramId) {
				std::string midiCatId = expCtx ? "on \"" + expCtx->getMidiCatId() + "\"" : "";
				std::list<Widget*> w;
				w.push_back(construct<MapMenuItem>(&MenuItem::text, string::f("Re-map %s", midiCatId.c_str()), &MapMenuItem::module, module, &MapMenuItem::pq, pq, &MapMenuItem::currentId, id));
				w.push_back(construct<CenterModuleItem>(&MenuItem::text, "Go to mapping module", &CenterModuleItem::mw, this));
				w.push_back(new MidiCatEndItem);

				if (itCvBegin == end) {
					menu->addChild(new MenuSeparator);
					menu->addChild(construct<MidiCatBeginItem>());
					for (Widget* wm : w) {
						menu->addChild(wm);
					}
				}
				else {
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

		if (expCtx) {
			std::string midiCatId = expCtx->getMidiCatId();
			if (midiCatId != "") {
				MenuItem* mapMenuItem = construct<MapMenuItem>(&MenuItem::text, string::f("Map on \"%s\"", midiCatId.c_str()), &MapMenuItem::module, module, &MapMenuItem::pq, pq);
				if (itCvBegin == end) {
					menu->addChild(new MenuSeparator);
					menu->addChild(construct<MidiCatBeginItem>());
					menu->addChild(mapMenuItem);
				}
				else {
					menu->addChild(mapMenuItem);
					auto it = std::find(beg, end, mapMenuItem);
					menu->children.splice(std::next(itCvEnd == end ? itCvBegin : itCvEnd), menu->children, it);
				}
			}
		}
	}

	void onDeselect(const event::Deselect& e) override {
		ModuleWidget::onDeselect(e);
		if (learnMode != LEARN_MODE::OFF) {
			DEFER({
				disableLearn();
			});

			// Learn module
			Widget* w = APP->event->getDraggedWidget();
			if (!w) return;
			ModuleWidget* mw = dynamic_cast<ModuleWidget*>(w);
			if (!mw) mw = w->getAncestorOfType<ModuleWidget>();
			if (!mw || mw == this) return;
			Module* m = mw->module;
			if (!m) return;

			MidiCatModule* module = dynamic_cast<MidiCatModule*>(this->module);
			switch (learnMode) {
				case LEARN_MODE::BIND_CLEAR:
					module->moduleBind(m, false); break;
				case LEARN_MODE::BIND_KEEP:
					module->moduleBind(m, true); break;
				case LEARN_MODE::MEM:
					module->expMemApply(m); break;
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
				case GLFW_KEY_E: {
					if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
						MidiCatModule* module = dynamic_cast<MidiCatModule*>(this->module);
						module->moduleBindExpander(true);
					}
					if ((e.mods & RACK_MOD_MASK) == (GLFW_MOD_SHIFT | RACK_MOD_CTRL)) {
						MidiCatModule* module = dynamic_cast<MidiCatModule*>(this->module);
						module->moduleBindExpander(false);
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
					MidiCatModule* module = dynamic_cast<MidiCatModule*>(this->module);
					disableLearn();
					module->disableLearn();
					e.consume(this);
					break;
				}
				case GLFW_KEY_SPACE: {
					if (module->learningId >= 0) {
						MidiCatModule* module = dynamic_cast<MidiCatModule*>(this->module);
						module->enableLearn(module->learningId + 1);
						if (module->learningId == -1) disableLearn();
						e.consume(this);
					}
					break;
				}
			}
		}
		ThemedModuleWidget<MidiCatModule>::onHoverKey(e);
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
		ThemedModuleWidget<MidiCatModule>::appendContextMenu(menu);
		assert(module);

		struct ResendMidiOutItem : MenuItem {
			MidiCatModule* module;
			Menu* createChildMenu() override {
				struct NowItem : MenuItem {
					MidiCatModule* module;
					void onAction(const event::Action& e) override {
						module->midiResendFeedback();
					}
				};

				struct PeriodicallyItem : MenuItem {
					MidiCatModule* module;
					void onAction(const event::Action& e) override {
						module->midiResendPeriodically ^= true;
					}
					void step() override {
						rightText = CHECKMARK(module->midiResendPeriodically);
						MenuItem::step();
					}
				};

				Menu* menu = new Menu;
				menu->addChild(construct<NowItem>(&MenuItem::text, "Now", &NowItem::module, module));
				menu->addChild(construct<PeriodicallyItem>(&MenuItem::text, "Periodically", &PeriodicallyItem::module, module));
				return menu;
			}
		}; // struct ResendMidiOutItem

		struct PresetLoadMenuItem : MenuItem {
			struct IgnoreMidiDevicesItem : MenuItem {
				MidiCatModule* module;
				void onAction(const event::Action& e) override {
					module->midiIgnoreDevices ^= true;
				}
				void step() override {
					rightText = CHECKMARK(module->midiIgnoreDevices);
					MenuItem::step();
				}
			}; // struct IgnoreMidiDevicesItem

			struct ClearMapsOnLoadItem : MenuItem {
				MidiCatModule* module;
				void onAction(const event::Action& e) override {
					module->clearMapsOnLoad ^= true;
				}
				void step() override {
					rightText = CHECKMARK(module->clearMapsOnLoad);
					MenuItem::step();
				}
			}; // struct ClearMapsOnLoadItem

			MidiCatModule* module;
			PresetLoadMenuItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				Menu* menu = new Menu;
				menu->addChild(construct<IgnoreMidiDevicesItem>(&MenuItem::text, "Ignore MIDI devices", &IgnoreMidiDevicesItem::module, module));
				menu->addChild(construct<ClearMapsOnLoadItem>(&MenuItem::text, "Clear mapping slots", &ClearMapsOnLoadItem::module, module));
				return menu;
			}
		};

		struct PrecisionMenuItem : MenuItem {
			struct PrecisionItem : MenuItem {
				MidiCatModule* module;
				int sampleRate;
				int division;
				std::string text;
				PrecisionItem() {
					sampleRate = int(APP->engine->getSampleRate());
				}
				void onAction(const event::Action& e) override {
					module->setProcessDivision(division);
				}
				void step() override {
					MenuItem::text = string::f("%s (%i Hz)", text.c_str(), sampleRate / division);
					rightText = module->processDivision == division ? "✔" : "";
					MenuItem::step();
				}
			};

			MidiCatModule* module;
			PrecisionMenuItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				Menu* menu = new Menu;
				menu->addChild(construct<PrecisionItem>(&PrecisionItem::text, "Audio rate", &PrecisionItem::module, module, &PrecisionItem::division, 1));
				menu->addChild(construct<PrecisionItem>(&PrecisionItem::text, "Higher CPU", &PrecisionItem::module, module, &PrecisionItem::division, 8));
				menu->addChild(construct<PrecisionItem>(&PrecisionItem::text, "Moderate CPU", &PrecisionItem::module, module, &PrecisionItem::division, 64));
				menu->addChild(construct<PrecisionItem>(&PrecisionItem::text, "Lowest CPU", &PrecisionItem::module, module, &PrecisionItem::division, 256));
				return menu;
			}
		}; // struct PrecisionMenuItem

		struct MidiModeMenuItem : MenuItem {
			MidiModeMenuItem() {
				rightText = RIGHT_ARROW;
			}

			struct MidiModeItem : MenuItem {
				MidiCatModule* module;
				MIDIMODE midiMode;

				void onAction(const event::Action &e) override {
					module->setMode(midiMode);
				}
				void step() override {
					rightText = module->midiMode == midiMode ? "✔" : "";
					MenuItem::step();
				}
			};

			MidiCatModule* module;
			Menu* createChildMenu() override {
				Menu* menu = new Menu;
				menu->addChild(construct<MidiModeItem>(&MenuItem::text, "Operating", &MidiModeItem::module, module, &MidiModeItem::midiMode, MIDIMODE::MIDIMODE_DEFAULT));
				menu->addChild(construct<MidiModeItem>(&MenuItem::text, "Locate and indicate", &MidiModeItem::module, module, &MidiModeItem::midiMode, MIDIMODE::MIDIMODE_LOCATE));
				return menu;
			}
		}; // struct MidiModeMenuItem

		menu->addChild(new MenuSeparator());
		menu->addChild(construct<PresetLoadMenuItem>(&MenuItem::text, "Preset load", &PresetLoadMenuItem::module, module));
		menu->addChild(construct<PrecisionMenuItem>(&MenuItem::text, "Precision", &PrecisionMenuItem::module, module));
		menu->addChild(construct<MidiModeMenuItem>(&MenuItem::text, "Mode", &MidiModeMenuItem::module, module));
		menu->addChild(construct<ResendMidiOutItem>(&MenuItem::text, "Re-send MIDI feedback", &MenuItem::rightText, RIGHT_ARROW, &ResendMidiOutItem::module, module));

		struct UiMenuItem : MenuItem {
			MidiCatModule* module;
			UiMenuItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				struct TextScrollItem : MenuItem {
					MidiCatModule* module;
					void onAction(const event::Action& e) override {
						module->textScrolling ^= true;
					}
					void step() override {
						rightText = module->textScrolling ? "✔" : "";
						MenuItem::step();
					}
				}; // struct TextScrollItem

				struct MappingIndicatorHiddenItem : MenuItem {
					MidiCatModule* module;
					void onAction(const event::Action& e) override {
						module->mappingIndicatorHidden ^= true;
					}
					void step() override {
						rightText = module->mappingIndicatorHidden ? "✔" : "";
						MenuItem::step();
					}
				}; // struct MappingIndicatorHiddenItem

				struct LockedItem : MenuItem {
					MidiCatModule* module;
					void onAction(const event::Action& e) override {
						module->locked ^= true;
					}
					void step() override {
						rightText = module->locked ? "✔" : "";
						MenuItem::step();
					}
				}; // struct LockedItem

				Menu* menu = new Menu;
				menu->addChild(construct<TextScrollItem>(&MenuItem::text, "Text scrolling", &TextScrollItem::module, module));
				menu->addChild(construct<MappingIndicatorHiddenItem>(&MenuItem::text, "Hide mapping indicators", &MappingIndicatorHiddenItem::module, module));
				menu->addChild(construct<LockedItem>(&MenuItem::text, "Lock mapping slots", &LockedItem::module, module));
				return menu;
			}
		}; // struct UiMenuItem

		struct OverlayEnabledItem : MenuItem {
			MidiCatModule* module;
			void onAction(const event::Action& e) override {
				module->overlayEnabled ^= true;
			}
			void step() override {
				rightText = module->overlayEnabled ? "✔" : "";
				MenuItem::step();
			}
		}; // struct OverlayEnabledItem

		struct ClearMapsItem : MenuItem {
			MidiCatModule* module;
			void onAction(const event::Action& e) override {
				module->clearMaps();
			}
		}; // struct ClearMapsItem

		struct ModuleLearnExpanderMenuItem : MenuItem {
			MidiCatModule* module;
			ModuleLearnExpanderMenuItem() {
				rightText = RIGHT_ARROW;
			}
			Menu* createChildMenu() override {
				struct ModuleLearnExpanderItem : MenuItem {
					MidiCatModule* module;
					bool keepCcAndNote;
					void onAction(const event::Action& e) override {
						module->moduleBindExpander(keepCcAndNote);
					}
				};

				Menu* menu = new Menu;
				menu->addChild(construct<ModuleLearnExpanderItem>(&MenuItem::text, "Clear first", &MenuItem::rightText, RACK_MOD_CTRL_NAME "+" RACK_MOD_SHIFT_NAME "+E", &ModuleLearnExpanderItem::module, module, &ModuleLearnExpanderItem::keepCcAndNote, false));
				menu->addChild(construct<ModuleLearnExpanderItem>(&MenuItem::text, "Keep MIDI assignments", &MenuItem::rightText, RACK_MOD_SHIFT_NAME "+E", &ModuleLearnExpanderItem::module, module, &ModuleLearnExpanderItem::keepCcAndNote, true));
				return menu;
			}
		}; // struct ModuleLearnExpanderMenuItem

		struct ModuleLearnSelectMenuItem : MenuItem {
			MidiCatWidget* mw;
			ModuleLearnSelectMenuItem() {
				rightText = RIGHT_ARROW;
			}
			Menu* createChildMenu() override {
				struct ModuleLearnSelectItem : MenuItem {
					MidiCatWidget* mw;
					LEARN_MODE mode;
					void onAction(const event::Action& e) override {
						mw->enableLearn(mode);
					}
				};

				Menu* menu = new Menu;
				menu->addChild(construct<ModuleLearnSelectItem>(&MenuItem::text, "Clear first", &MenuItem::rightText, RACK_MOD_CTRL_NAME "+" RACK_MOD_SHIFT_NAME "+D", &ModuleLearnSelectItem::mw, mw, &ModuleLearnSelectItem::mode, LEARN_MODE::BIND_CLEAR));
				menu->addChild(construct<ModuleLearnSelectItem>(&MenuItem::text, "Keep MIDI assignments", &MenuItem::rightText, RACK_MOD_SHIFT_NAME "+D", &ModuleLearnSelectItem::mw, mw, &ModuleLearnSelectItem::mode, LEARN_MODE::BIND_KEEP));
				return menu;
			}
		}; // struct ModuleLearnSelectMenuItem

		menu->addChild(new MenuSeparator());
		menu->addChild(construct<UiMenuItem>(&MenuItem::text, "User interface", &UiMenuItem::module, module));
		menu->addChild(construct<OverlayEnabledItem>(&MenuItem::text, "Status overlay", &OverlayEnabledItem::module, module));
		menu->addChild(new MenuSeparator());
		menu->addChild(construct<ClearMapsItem>(&MenuItem::text, "Clear mappings", &ClearMapsItem::module, module));
		menu->addChild(construct<ModuleLearnExpanderMenuItem>(&MenuItem::text, "Map module (left)", &ModuleLearnExpanderMenuItem::module, module));
		menu->addChild(construct<ModuleLearnSelectMenuItem>(&MenuItem::text, "Map module (select)", &ModuleLearnSelectMenuItem::mw, this));

		appendContextMenuMem(menu);
	}

	void appendContextMenuMem(Menu* menu) {
		MidiCatModule* module = dynamic_cast<MidiCatModule*>(this->module);
		assert(module);

		struct MapMenuItem : MenuItem {
			MidiCatModule* module;
			MapMenuItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				struct MidimapModuleItem : MenuItem {
					MidiCatModule* module;
					std::string pluginSlug;
					std::string moduleSlug;
					MemModule* midimapModule;
					MidimapModuleItem() {
						rightText = RIGHT_ARROW;
					}
					Menu* createChildMenu() override {
						struct DeleteItem : MenuItem {
							MidiCatModule* module;
							std::string pluginSlug;
							std::string moduleSlug;
							void onAction(const event::Action& e) override {
								module->expMemDelete(pluginSlug, moduleSlug);
							}
						}; // DeleteItem

						Menu* menu = new Menu;
						menu->addChild(construct<DeleteItem>(&MenuItem::text, "Delete", &DeleteItem::module, module, &DeleteItem::pluginSlug, pluginSlug, &DeleteItem::moduleSlug, moduleSlug));
						return menu;
					}
				}; // MidimapModuleItem

				std::list<std::pair<std::string, MidimapModuleItem*>> l; 
				for (auto it : module->expMemStorage) {
					MemModule* a = it.second;
					MidimapModuleItem* midimapModuleItem = new MidimapModuleItem;
					midimapModuleItem->text = string::f("%s %s", a->pluginName.c_str(), a->moduleName.c_str());
					midimapModuleItem->module = module;
					midimapModuleItem->midimapModule = a;
					midimapModuleItem->pluginSlug = it.first.first;
					midimapModuleItem->moduleSlug = it.first.second;
					l.push_back(std::pair<std::string, MidimapModuleItem*>(midimapModuleItem->text, midimapModuleItem));
				}

				l.sort();
				Menu* menu = new Menu;
				for (auto it : l) {
					menu->addChild(it.second);
				}
				return menu;
			}
		}; // MapMenuItem

		struct SaveMenuItem : MenuItem {
			MidiCatModule* module;
			SaveMenuItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				struct SaveItem : MenuItem {
					MidiCatModule* module;
					std::string pluginSlug;
					std::string moduleSlug;
					void onAction(const event::Action& e) override {
						module->expMemSave(pluginSlug, moduleSlug);
					}
				}; // SaveItem

				typedef std::pair<std::string, std::string> ppair;
				std::list<std::pair<std::string, ppair>> list;
				std::set<ppair> s;
				for (size_t i = 0; i < MAX_CHANNELS; i++) {
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
					menu->addChild(construct<SaveItem>(&MenuItem::text, it.first, &SaveItem::module, module, &SaveItem::pluginSlug, it.second.first, &SaveItem::moduleSlug, it.second.second));
				}
				return menu;
			}
		}; // SaveMenuItem

		struct ApplyItem : MenuItem {
			MidiCatWidget* mw;
			void onAction(const event::Action& e) override {
				mw->enableLearn(LEARN_MODE::MEM);
			}
		}; // ApplyItem

		menu->addChild(new MenuSeparator());
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "MEM-expander"));
		menu->addChild(construct<MapMenuItem>(&MenuItem::text, "Available mappings", &MapMenuItem::module, module));
		menu->addChild(construct<SaveMenuItem>(&MenuItem::text, "Store mapping", &SaveMenuItem::module, module));
		menu->addChild(construct<ApplyItem>(&MenuItem::text, "Apply mapping", &MenuItem::rightText, RACK_MOD_SHIFT_NAME "+V", &ApplyItem::mw, this));
	}
};

} // namespace MidiCat
} // namespace StoermelderPackOne

Model* modelOSCelot = createModel<StoermelderPackOne::MidiCat::MidiCatModule, StoermelderPackOne::MidiCat::MidiCatWidget>("OSCelot");