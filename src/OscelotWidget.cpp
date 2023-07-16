#include "OscelotWidget.hpp"
#include "MapModuleBase.hpp"
#include "plugin.hpp"

namespace TheModularMind {
namespace Oscelot {


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
				meowMorySwitchModule(module->moduleSlug, true);
				module->moduleSlug = "";
			}

			if (module->oscTriggerNext || meowMoryNextTrigger.process(module->params[OscelotModule::PARAM_NEXT].getValue())) {
				module->oscTriggerNext = false;
				meowMorySwitchModule(module->moduleSlug, false);
				module->moduleSlug = "";
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

	void meowMorySwitchModule(std::string moduleSlugName, bool prev) {
		std::list<Widget*> modules = APP->scene->rack->getModuleContainer()->children;
		auto sort = [&](Widget* w1, Widget* w2) {
			auto t1 = std::make_tuple(w1->box.pos.y, w1->box.pos.x);
			auto t2 = std::make_tuple(w2->box.pos.y, w2->box.pos.x);
			if (prev)
				return t1 > t2;
			else
				return t1 < t2;
		};
		modules.sort(sort);
		meowMoryScanModules(modules, moduleSlugName);
	}

	void meowMoryScanModules(std::list<Widget*>& modules, std::string moduleSlugName) {
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
				// Scan for module with name moduleSlug
				if (moduleSlugName != "") {
					if (m->model->slug.c_str() == moduleSlugName) {
						module->moduleMeowMoryApply(m);
						return;
					}
					// if no name, get next saved mapping
				} else {
					module->moduleMeowMoryApply(m);
					return;
				}
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