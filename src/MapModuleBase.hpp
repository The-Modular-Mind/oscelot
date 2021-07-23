#pragma once
#include "plugin.hpp"
#include "settings.hpp"
#include "components/ParamHandleIndicator.hpp"
#include "digital/OscelotParam.hpp"
#include <chrono>


// Abstract modules

namespace TheModularMind {

template< int MAX_CHANNELS >
struct MapModuleBase : Module {
	/** Number of maps */
	int mapLen = 0;
	/** The mapped param handle of each channel */
	ParamHandle paramHandles[MAX_CHANNELS];
	TheModularMind::ParamHandleIndicator paramHandleIndicator[MAX_CHANNELS];

	/** Channel ID of the learning session */
	int learningId;
	/** Whether the param has been set during the learning session */
	bool learnedParam;

	/** [Stored to JSON] */
	bool textScrolling = true;

	NVGcolor mappingIndicatorColor = color::BLACK_TRANSPARENT;
	/** [Stored to JSON] */
	bool mappingIndicatorHidden = false;

	/** The smoothing processor (normalized between 0 and 1) of each channel */
	dsp::ExponentialFilter valueFilters[MAX_CHANNELS];

	dsp::ClockDivider indicatorDivider;

	MapModuleBase() {
		for (int id = 0; id < MAX_CHANNELS; id++) {
			paramHandleIndicator[id].color = mappingIndicatorColor;
			paramHandleIndicator[id].handle = &paramHandles[id];
			APP->engine->addParamHandle(&paramHandles[id]);
		}
		indicatorDivider.setDivision(2048);
	}

	~MapModuleBase() {
		for (int id = 0; id < MAX_CHANNELS; id++) {
			APP->engine->removeParamHandle(&paramHandles[id]);
		}
	}

	void onReset() override {
		learningId = -1;
		learnedParam = false;
		clearMaps();
		mapLen = 0;
	}

	void process(const ProcessArgs& args) override {
		if (indicatorDivider.process()) {
			float t = indicatorDivider.getDivision() * args.sampleTime;
			for (int i = 0; i < MAX_CHANNELS; i++) {
				paramHandleIndicator[i].color = mappingIndicatorHidden ? color::BLACK_TRANSPARENT : mappingIndicatorColor;
				if (paramHandles[i].moduleId >= 0) {
					paramHandleIndicator[i].process(t, learningId == i);
				}
			}
		}
	}

	ParamQuantity* getParamQuantity(int id) {
		// Get Module
		Module* module = paramHandles[id].module;
		if (!module)
			return NULL;
		// Get ParamQuantity
		int paramId = paramHandles[id].paramId;
		ParamQuantity* paramQuantity = module->paramQuantities[paramId];
		if (!paramQuantity)
			return NULL;
		if (!paramQuantity->isBounded())
			return NULL;
		return paramQuantity;
	}

	virtual void clearMap(int id) {
		if (paramHandles[id].moduleId < 0) return;
		learningId = -1;
		APP->engine->updateParamHandle(&paramHandles[id], -1, 0, true);
		valueFilters[id].reset();
		updateMapLen();
	}

	virtual void clearMaps() {
		learningId = -1;
		for (int id = 0; id < MAX_CHANNELS; id++) {
			APP->engine->updateParamHandle(&paramHandles[id], -1, 0, true);
			valueFilters[id].reset();
		}
		mapLen = 0;
	}

	virtual void updateMapLen() {
		// Find last nonempty map
		int id;
		for (id = MAX_CHANNELS - 1; id >= 0; id--) {
			if (paramHandles[id].moduleId >= 0)
				break;
		}
		mapLen = id + 1;
		// Add an empty "Mapping..." slot
		if (mapLen < MAX_CHANNELS)
			mapLen++;
	}

	virtual void commitLearn() {
		if (learningId < 0)
			return;
		if (!learnedParam)
			return;
		// Reset learned state
		learnedParam = false;
		// Find next incomplete map
		while (++learningId < MAX_CHANNELS) {
			if (paramHandles[learningId].moduleId < 0)
				return;
		}
		learningId = -1;
	}

	virtual void enableLearn(int id) {
		if (learningId != id) {
			learningId = id;
			learnedParam = false;
		}
	}

	virtual void disableLearn(int id) {
		if (learningId == id) {
			learningId = -1;
		}
	}

	virtual void learnParam(int id, int moduleId, int paramId) {
		APP->engine->updateParamHandle(&paramHandles[id], moduleId, paramId, true);
		learnedParam = true;
		commitLearn();
		updateMapLen();
	}

 
	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "textScrolling", json_boolean(textScrolling));
		json_object_set_new(rootJ, "mappingIndicatorHidden", json_boolean(mappingIndicatorHidden));

		json_t* mapsJ = json_array();
		for (int id = 0; id < mapLen; id++) {
			json_t* mapJ = json_object();
			json_object_set_new(mapJ, "moduleId", json_integer(paramHandles[id].moduleId));
			json_object_set_new(mapJ, "paramId", json_integer(paramHandles[id].paramId));
			dataToJsonMap(mapJ, id);
			json_array_append_new(mapsJ, mapJ);
		}
		json_object_set_new(rootJ, "maps", mapsJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		clearMaps();

		json_t* textScrollingJ = json_object_get(rootJ, "textScrolling");
		textScrolling = json_boolean_value(textScrollingJ);
		json_t* mappingIndicatorHiddenJ = json_object_get(rootJ, "mappingIndicatorHidden");
		mappingIndicatorHidden = json_boolean_value(mappingIndicatorHiddenJ);

		json_t* mapsJ = json_object_get(rootJ, "maps");
		if (mapsJ) {
			json_t* mapJ;
			size_t mapIndex;
			json_array_foreach(mapsJ, mapIndex, mapJ) {
				json_t* moduleIdJ = json_object_get(mapJ, "moduleId");
				json_t* paramIdJ = json_object_get(mapJ, "paramId");
				if (!(moduleIdJ && paramIdJ))
					continue;
				if (mapIndex >= MAX_CHANNELS)
					continue;
				int moduleId = json_integer_value(moduleIdJ);
				int paramId = json_integer_value(paramIdJ);
				APP->engine->updateParamHandle(&paramHandles[mapIndex], moduleId, paramId, false);
				dataFromJsonMap(mapJ, mapIndex);
			}
		}
		updateMapLen();
	}

	virtual void dataToJsonMap(json_t* mapJ, int index) {}
	virtual void dataFromJsonMap(json_t* mapJ, int index) {}
};

// Widgets

template< int MAX_CHANNELS, typename MODULE >
struct MapModuleChoice : LedDisplayChoice {
	MODULE* module = NULL;
	bool processEvents = true;
	int id;

	std::chrono::time_point<std::chrono::system_clock> hscrollUpdate = std::chrono::system_clock::now();
	int hscrollCharOffset = 0;

	MapModuleChoice() {
		box.size = mm2px(Vec(0, 7.5));
		textOffset = Vec(6, 14.7);
		color = nvgRGB(0xf0, 0xf0, 0xf0);
	}

	~MapModuleChoice() {
		if (module && module->learningId == id) {
			glfwSetCursor(APP->window->win, NULL);
		}
	}

	void setModule(MODULE* module) {
		this->module = module;
	}

	void onButton(const event::Button& e) override {
		e.stopPropagating();
		if (!module) return;
		if (module->locked) return;

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			e.consume(this);
		}

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			e.consume(this);

			if (module->paramHandles[id].moduleId >= 0) {
				createContextMenu();
			} 
			else {
				module->clearMap(id);
			}
		}
	}

	void createContextMenu() {
		struct UnmapItem : MenuItem {
			MODULE* module;
			int id;
			void onAction(const event::Action& e) override {
				module->clearMap(id);
			}
		};

		struct IndicateItem : MenuItem {
			MODULE* module;
			int id;
			void onAction(const event::Action& e) override {
				ParamHandle* paramHandle = &module->paramHandles[id];
				ModuleWidget* mw = APP->scene->rack->getModule(paramHandle->moduleId);
				module->paramHandleIndicator[id].indicate(mw);
			}
		};

		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel("Parameter \"" + getParamName() + "\""));
		menu->addChild(construct<IndicateItem>(&MenuItem::text, "Locate and indicate", &IndicateItem::module, module, &IndicateItem::id, id));
		menu->addChild(construct<UnmapItem>(&MenuItem::text, "Unmap", &UnmapItem::module, module, &UnmapItem::id, id));
		appendContextMenu(menu);
	}

	virtual void appendContextMenu(Menu* menu) { }

	void onSelect(const event::Select& e) override {
		if (!module) return;
		if (module->locked) return;

		ScrollWidget *scroll = getAncestorOfType<ScrollWidget>();
		scroll->scrollTo(box);

		// Reset touchedParam, unstable API
		APP->scene->rack->touchedParam = NULL;
		module->enableLearn(id);

		GLFWcursor* cursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
		glfwSetCursor(APP->window->win, cursor);
	}

	void onDeselect(const event::Deselect& e) override {
		if (!module) return;
		if (!processEvents) return;

		// Check if a ParamWidget was touched, unstable API
		ParamWidget *touchedParam = APP->scene->rack->touchedParam;
		if (touchedParam && touchedParam->paramQuantity->module != module) {
			APP->scene->rack->touchedParam = NULL;
			int moduleId = touchedParam->paramQuantity->module->id;
			int paramId = touchedParam->paramQuantity->paramId;
			module->learnParam(id, moduleId, paramId);
			hscrollCharOffset = 0;
		} 
		else {
			module->disableLearn(id);
		}
		glfwSetCursor(APP->window->win, NULL);
	}

	void step() override {
		if (!module)
			return;
		if(module->panelTheme==1) {
			color = nvgRGB(0xfe, 0xff, 0xe0);
		}
		else{
			color = nvgRGB(0xDA, 0xa5, 0x20);
		}
		// Set bgColor and selected state
		if (module->learningId == id) {
			bgColor = color;
			bgColor.a = 0.15;
			if (APP->event->getSelectedWidget() != this)
				APP->event->setSelected(this);
		} 
		else {
			bgColor = nvgRGBA(0, 0, 0, 0);
			if (APP->event->getSelectedWidget() == this)
				APP->event->setSelected(NULL);
		}

		// Set text
		if (module->paramHandles[id].moduleId >= 0 && module->learningId != id) {
			std::string prefix = "";
			std::string label = getSlotLabel();
			if (label == "") {
				prefix = getSlotPrefix();
				label = getParamName();
				if (label == "") {
					module->clearMap(id);
					return;
				}
			}

			size_t hscrollMaxLength = ceil(box.size.x / 6.2f);
			if (module->textScrolling && label.length() + prefix.length() > hscrollMaxLength) {
				// Scroll the parameter-name horizontically
				text = prefix + label.substr(hscrollCharOffset > (int)label.length() ? 0 : hscrollCharOffset);
				auto now = std::chrono::system_clock::now();
				if (now - hscrollUpdate > std::chrono::milliseconds{100}) {
					hscrollCharOffset = (hscrollCharOffset + 1) % (label.length() + hscrollMaxLength);
					hscrollUpdate = now;
				}
			} 
			else {
				text = prefix + label;
			}
		} 
		else {
			if (module->learningId == id) {
				text = getSlotPrefix() + "Mapping...";
			} else {
				text = getSlotPrefix() + "Unmapped";
			}
		}

		// Set text color
		if (module->paramHandles[id].moduleId >= 0 || module->learningId == id) {
			color.a = 0.9;
		} 
		else {
			color.a = 0.5;
		}
	}

	virtual std::string getSlotLabel() {
		return "";
	}

	virtual std::string getSlotPrefix() {
		return MAX_CHANNELS > 1 ? string::f("%02d ", id + 1) : "";
	}

	ParamQuantity* getParamQuantity() {
		if (!module)
			return NULL;
		if (id >= module->mapLen)
			return NULL;
		ParamHandle* paramHandle = &module->paramHandles[id];
		if (paramHandle->moduleId < 0)
			return NULL;
		ModuleWidget *mw = APP->scene->rack->getModule(paramHandle->moduleId);
		if (!mw)
			return NULL;
		// Get the Module from the ModuleWidget instead of the ParamHandle.
		// I think this is more elegant since this method is called in the app world instead of the engine world.
		Module* m = mw->module;
		if (!m)
			return NULL;
		int paramId = paramHandle->paramId;
		if (paramId >= (int) m->params.size())
			return NULL;
		ParamQuantity* paramQuantity = m->paramQuantities[paramId];
		return paramQuantity;
	}

	std::string getParamName() {
		if (!module)
			return "";
		if (id >= module->mapLen)
			return "";
		ParamHandle* paramHandle = &module->paramHandles[id];
		if (paramHandle->moduleId < 0)
			return "";
		ModuleWidget *mw = APP->scene->rack->getModule(paramHandle->moduleId);
		if (!mw)
			return "";
		// Get the Module from the ModuleWidget instead of the ParamHandle.
		// I think this is more elegant since this method is called in the app world instead of the engine world.
		Module* m = mw->module;
		if (!m)
			return "";
		int paramId = paramHandle->paramId;
		if (paramId >= (int) m->params.size())
			return "";
		ParamQuantity* paramQuantity = m->paramQuantities[paramId];
		std::string s;
		s += mw->model->name;
		s += " ";
		s += paramQuantity->label;
		return s;
	}

	void draw(const DrawArgs& args) override {
		if (bgColor.a > 0.0) {
			nvgScissor(args.vg, RECT_ARGS(args.clipBox));
			nvgBeginPath(args.vg);
			nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
			nvgFillColor(args.vg, bgColor);
			nvgFill(args.vg);
			nvgResetScissor(args.vg);
		}

		if (font->handle >= 0) {
			Rect r = Rect(textOffset.x, 0.f, box.size.x - textOffset.x * 2, box.size.y).intersect(args.clipBox);
			nvgScissor(args.vg, RECT_ARGS(r));
			nvgFillColor(args.vg, color);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, 0.0);
			nvgFontSize(args.vg, 12);
			nvgText(args.vg, textOffset.x, textOffset.y, text.c_str(), NULL);
			nvgResetScissor(args.vg);
		}
	}
};

template< int MAX_CHANNELS, typename MODULE, typename CHOICE = MapModuleChoice<MAX_CHANNELS, MODULE> >
struct MapModuleDisplay : LedDisplay {
	MODULE* module;
	ScrollWidget* scroll;
	CHOICE* choices[MAX_CHANNELS];

	~MapModuleDisplay() {
		for (int id = 0; id < MAX_CHANNELS; id++) {
			choices[id]->processEvents = false;
		}
	}

	void setModule(MODULE* module) {
		this->module = module;

		scroll = new ScrollWidget();
		scroll->box.size.x = box.size.x;
		scroll->box.size.y = box.size.y - scroll->box.pos.y;
		scroll->verticalScrollBar->box.size.x =8.0f;
		addChild(scroll);

		Vec pos;
		for (int id = 0; id < MAX_CHANNELS; id++) {
			CHOICE* choice = createWidget<CHOICE>(pos);
			choice->box.size.x = box.size.x;
			choice->id = id;
			choice->setModule(module);
			scroll->container->addChild(choice);
			choices[id] = choice;

			pos = choice->box.getBottomLeft();
		}
	}

	void draw(const DrawArgs& args) override {
		// LedDisplay::draw(args);
		NVGcolor bgColor = color::BLACK;
		bgColor.a=0.0;

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 5.0);
		nvgFillColor(args.vg, bgColor);
		nvgFill(args.vg);

		nvgScissor(args.vg, RECT_ARGS(args.clipBox));
		Widget::draw(args);
		nvgResetScissor(args.vg);

		if (module && module->locked) {
			float stroke = 2.f;
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, stroke / 2, stroke / 2, box.size.x - stroke, box.size.y - stroke, 5.0);
			nvgStrokeWidth(args.vg, stroke);
			nvgStrokeColor(args.vg, color::mult(color::WHITE, 0.5f));
			nvgStroke(args.vg);
		}
	}

	void onHoverScroll(const event::HoverScroll& e) override {
		if (module && module->locked) {
			e.stopPropagating();
		}
		LedDisplay::onHoverScroll(e);
	}
};


} // namespace StoermelderPackOne