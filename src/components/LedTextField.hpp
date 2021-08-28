#pragma once
#include "../plugin.hpp"

namespace TheModularMind {

struct OscelotTextField : LedDisplayTextField {
	float textSize = 14.f;
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
			                  box.size.y - 2 * textOffset.y, -1, color, textSize, text.c_str(), highlightColor, begin, end);

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

struct OscelotTextLabel : ui::Label {
	float textSize = 8.f;
	NVGcolor color;
	NVGcolor bgColor;
	std::shared_ptr<Font> font;

	OscelotTextLabel() {
		color = nvgRGB(0xfe, 0xff, 0xe0);
		bgColor = color::WHITE;
		color.a = 0.9;
		bgColor.a = 0.05; 
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/NovaMono-Regular.ttf"));
	}

	void drawLabel(const DrawArgs& args, float x0, float y0, float w, const char* label, int size) {
		float constexpr padMargin = 3;
		nvgBeginPath(args.vg);

		nvgFontSize(args.vg, size);
		nvgTextAlign(args.vg, NVG_ALIGN_TOP | NVG_ALIGN_CENTER);
		nvgFillColor(args.vg, color);
		nvgText(args.vg, x0 + w / 2, y0, label, NULL);

		float bounds[4];
		nvgTextBounds(args.vg, x0 + w / 2, y0, label, NULL, bounds);

		// Background
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 2 * padMargin, bounds[1] - (padMargin / 2.0), w - 4 * padMargin, 3 * (bounds[3] - bounds[1] + padMargin), 5.0);
		nvgFillColor(args.vg, bgColor);
		nvgFill(args.vg);
	}

	void draw(const DrawArgs& args) override { drawLabel(args, 0.f, box.size.y, box.size.x, text.c_str(), textSize); }
};

} // namespace TheModularMind