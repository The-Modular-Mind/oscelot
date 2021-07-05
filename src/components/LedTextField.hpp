#pragma once
#include "../plugin.hpp"
#include "LedTextDisplay.hpp"

namespace StoermelderPackOne {

struct StoermelderTextField : LedDisplayTextField {
	float textSize = 13.f;
	const static unsigned int defaultMaxTextLength = 5;
	unsigned int maxTextLength;
	NVGcolor bgColor;
	bool isFocused = false;
	bool doubleClick = false;

	StoermelderTextField() {
		maxTextLength = defaultMaxTextLength;
		textOffset = math::Vec(-0.4f, -2.1f);
		color = nvgRGB(0xDA, 0xa5, 0x20);
		bgColor = color::BLACK;
		bgColor.a=0.3;
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
			int begin = std::min(cursor, selection);
			int end = (this == APP->event->selectedWidget) ? std::max(cursor, selection) : -1;
			bndIconLabelCaret(args.vg, textOffset.x, textOffset.y, box.size.x - 2 * textOffset.x,
			                  box.size.y - 2 * textOffset.y, -1, color, 12, text.c_str(), highlightColor, begin, end);

			bndSetFont(APP->window->uiFont->handle);
		}

		nvgResetScissor(args.vg);

		//nvgScissor(args.vg, RECT_ARGS(args.clipBox));

		// if (bgColor.a > 0.0) {
		// 	nvgBeginPath(args.vg);
		// 	nvgRect(args.vg, textOffset.x, 0, box.size.x, box.size.y);
		// 	nvgFillColor(args.vg, bgColor);
		// 	nvgFill(args.vg);
		// }

		// if (text.length() > 0) {
		// 	nvgFillColor(args.vg, color);
		// 	nvgFontFaceId(args.vg, font->handle);
		// 	nvgTextLetterSpacing(args.vg, 0.0);
		// 	nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		// 	nvgFontSize(args.vg, textSize);
		// 	nvgTextBox(args.vg, textOffset.x, box.size.y / 2.f, box.size.x, text.c_str(), NULL);
		// }

		// if (isFocused) {
		// 	NVGcolor highlightColor = color;
		// 	highlightColor.a = 0.5;

		// 	int begin = std::min(cursor, selection);
		// 	int end = std::max(cursor, selection);
		// 	int len = end - begin;

		// 	// hacky way of measuring character width
		// 	NVGglyphPosition glyphs[4];
		// 	nvgTextGlyphPositions(args.vg, 0.f, 0.f, "a", NULL, glyphs, 4);
		// 	float char_width = -2 * glyphs[0].x;

		// 	float ymargin = 2.f;
		// 	nvgBeginPath(args.vg);
		// 	nvgFillColor(args.vg, highlightColor);
		// 	nvgRect(args.vg,
		// 			box.size.x / 2.f + textOffset.x + (begin - 0.5f * TextField::text.size()) * char_width - 1,
		// 			ymargin,
		// 			(len > 0 ? (char_width * len) : 1) + 1,
		// 			box.size.y - 2.f * ymargin);
		// 	nvgFill(args.vg);
		// }

		//nvgResetScissor(args.vg);
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

} // namespace StoermelderPackOne