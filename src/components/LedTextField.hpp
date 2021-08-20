#pragma once
#include "../plugin.hpp"

namespace TheModularMind {

struct OscelotTextField : LedDisplayTextField {
	float textSize = 13.f;
	const static unsigned int defaultMaxTextLength = 5;
	unsigned int maxTextLength;
	NVGcolor bgColor;
	bool isFocused = false;
	bool doubleClick = false;

	OscelotTextField() {
		maxTextLength = defaultMaxTextLength;
		textOffset = math::Vec(-0.4f, -2.1f);
		color = nvgRGB(0xfe, 0xff, 0xe0);
		bgColor = color::BLACK;
		bgColor.a=0.3;
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/NovaMono-Regular.ttf"));
	}

	void draw(const DrawArgs& args) override {
		nvgScissor(args.vg, RECT_ARGS(args.clipBox));

		// Background
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 5.0);
		nvgFillColor(args.vg, bgColor);
		nvgFill(args.vg);

		// Text
		if (font->handle >= 0) {
			bndSetFont(font->handle);

			NVGcolor highlightColor = color;
			highlightColor.a = 0.5;
			color.a = 0.9;
			int begin = std::min(cursor, selection);
			int end = (this == APP->event->selectedWidget) ? std::max(cursor, selection) : -1;
			bndIconLabelCaret(args.vg, textOffset.x, textOffset.y, box.size.x - 2 * textOffset.x,
			                  box.size.y - 2 * textOffset.y, -1, color, 14, text.c_str(), highlightColor, begin, end);

			bndSetFont(APP->window->uiFont->handle);
		}
		nvgResetScissor(args.vg);
	}

	void onSelect(const event::Select& e) override {
		isFocused = true;
		e.consume(this);
	}

	void onDeselect(const event::Deselect& e) override {
		isFocused = false;
		LedDisplayTextField::setText(TextField::text);
		e.consume(NULL);
	}

	void onAction(const event::Action& e) override {
		// this gets fired when the user types 'enter'
		event::Deselect eDeselect;
		onDeselect(eDeselect);
		APP->event->selectedWidget = NULL;
		e.consume(NULL);
	}

	void onDoubleClick(const event::DoubleClick& e) override {
		doubleClick = true;
	}

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_RELEASE) {
			if (doubleClick) {
				doubleClick = false;
				selectAll();
			}
		}
		LedDisplayTextField::onButton(e);
	}

	void onSelectText(const event::SelectText& e) override {
		if (TextField::text.size() < maxTextLength || cursor != selection) {
			LedDisplayTextField::onSelectText(e);
		} else {
			e.consume(NULL);
		}
	}
};

} // namespace TheModularMind