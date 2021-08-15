#pragma once
#include "plugin.hpp"
#include "settings.hpp"
#include "components/ParamHandleIndicator.hpp"
#include "components/OscelotParam.hpp"
#include <chrono>

namespace TheModularMind {

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
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/NovaMono-Regular.ttf"));
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

		struct LabelMenuItem : MenuItem {
			MODULE* module;
			int id;
			std::string tempLabel;

			LabelMenuItem() {
				rightText = RIGHT_ARROW;
			}

			struct LabelField : ui::TextField {
				MODULE* module;
				int id;
				void onSelectKey(const event::SelectKey& e) override {
					if (e.action == GLFW_PRESS && e.key == GLFW_KEY_ENTER) {
						module->textLabels[id] = text;

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
				MODULE* module;
				int id;
				void onAction(const event::Action& e) override { module->textLabels[id] = ""; }
			};

			Menu* createChildMenu() override {
				Menu* menu = new Menu;

				LabelField* labelField = new LabelField;
				labelField->placeholder = "Label";
				labelField->box.size.x = 220;
				labelField->module = module;
				labelField->id = id;
				labelField->text = module->textLabels[id];
				if(labelField->text==""){
					labelField->text=tempLabel;
				}
				menu->addChild(labelField);

				ResetItem* resetItem = new ResetItem;
				resetItem->text = "Reset";
				resetItem->module = module;
				resetItem->id = id;
				menu->addChild(resetItem);

				return menu;
			}
		}; // struct LabelMenuItem


		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel("Parameter \"" + getParamName() + "\""));
		menu->addChild(construct<LabelMenuItem>(
		    &MenuItem::text, "Custom label", &LabelMenuItem::module, module, &LabelMenuItem::id, id,
		    &LabelMenuItem::tempLabel, getSlotPrefix() == ".... " ? getParamName() : getSlotPrefix() + getParamName()));
		menu->addChild(construct<IndicateItem>(&MenuItem::text, "Locate and indicate", &IndicateItem::module, module, &IndicateItem::id, id));
		
		menu->addChild(new MenuSeparator());
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
		s += " > ";
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
			nvgFontSize(args.vg, 14);
			nvgText(args.vg, textOffset.x, textOffset.y, text.c_str(), NULL);
			nvgResetScissor(args.vg);
		}
	}
};

struct oscelotScrollWidget : ScrollWidget {
	void draw(const DrawArgs& args) override {
	    NVGcolor color = color::BLACK;
		nvgScissor(args.vg, RECT_ARGS(args.clipBox));
		Widget::draw(args);
		nvgResetScissor(args.vg);

		if(verticalScrollBar->visible){
			color.a = 0.5;
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, verticalScrollBar->box.pos.x, verticalScrollBar->box.pos.y, verticalScrollBar->box.size.x, verticalScrollBar->box.size.y, 3.0);
			nvgFillColor(args.vg, color);
			nvgFill(args.vg);

			color.a = 0.4;
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, verticalScrollBar->box.pos.x+1, verticalScrollBar->box.pos.y+1, verticalScrollBar->box.size.x - 2, verticalScrollBar->box.size.y - 2, 3.0);
			nvgFillColor(args.vg, color);
			nvgFill(args.vg);
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

		scroll = new oscelotScrollWidget();
		scroll->box.size.x = box.size.x;
		scroll->box.size.y = box.size.y - scroll->box.pos.y;
		scroll->verticalScrollBar->box.size.x = 8.0f;

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
		NVGcolor bgColor = color::BLACK;
		bgColor.a=0.0;
		// LedDisplay::draw(args);

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 5.0);
		nvgFillColor(args.vg, bgColor);
		nvgFill(args.vg);

		nvgScissor(args.vg, RECT_ARGS(args.clipBox));
		Widget::draw(args);
		nvgResetScissor(args.vg);

		if (module && module->locked) {
			NVGcolor fColor = color::WHITE;
			fColor.a=0.10;
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, -2, -5, box.size.x + 8, box.size.y + 7, 25.0);
			nvgFillColor(args.vg, fColor);
			nvgFill(args.vg);
		}
	}

	void onHoverScroll(const event::HoverScroll& e) override {
		if (module && module->locked) {
			e.stopPropagating();
		}
		LedDisplay::onHoverScroll(e);
	}
};


} // namespace TheModularMind