#pragma once
#include "plugin.hpp"
#include "settings.hpp"

namespace TheModularMind {
namespace Rack {

/** Move the view-port smoothly and center a Widget
 */
struct ViewportCenterSmooth {
	Vec source, target;
	float sourceZoom, targetZoom;
	int framecount = 0;
	int frame = 0;

	void trigger(Widget* w, float zoom, float framerate, float transitionTime = 1.f) {
		Vec target = w->box.pos;
		target = target.plus(w->box.size.mult(0.5f));
		trigger(target, zoom, framerate, transitionTime);
	}

	void trigger(Vec target, float zoom, float framerate, float transitionTime = 1.f) {
		// source is at top-left, translate to center of screen
		Vec source = APP->scene->rackScroll->offset;
		source = source.plus(APP->scene->rackScroll->box.size.mult(0.5f));
		source = source.div(APP->scene->rackScroll->zoomWidget->zoom);

		this->source = source;
		this->target = target;
		this->sourceZoom = rack::settings::zoom;
		this->targetZoom = zoom;
		this->framecount = int(transitionTime * framerate);
		this->frame = 0;
	}

	void trigger(Rect rect, float framerate, float transitionTime = 1.f) {
		// NB: unstable API!
		Vec target = rect.getCenter();
		target = target.mult(APP->scene->rackScroll->zoomWidget->zoom);
		target = target.minus(APP->scene->rackScroll->box.size.mult(0.5f));
		float zx = std::log2(APP->scene->rackScroll->box.size.x / rect.size.x * 0.9f);
		float zy = std::log2(APP->scene->rackScroll->box.size.y / rect.size.y * 0.9f);
		float zoom = std::min(zx, zy);
		trigger(rect.getCenter(), zoom, framerate, transitionTime);
	}

	void reset() {
		frame = framecount = 0;
	}

	void process() {
		if (framecount == frame) return;

		float t = float(frame) / float(framecount - 1);
		// Sigmoid
		t = t * 8.f - 4.f;
		t = 1.f / (1.f + std::exp(-t));
		t = rescale(t, 0.0179f, 0.98201f, 0.f, 1.f);

		// Calculate interpolated view-point and zoom
		Vec p1 = source.mult(1.f - t);
		Vec p2 = target.mult(t);
		Vec p = p1.plus(p2);
		
		// Ignore tiny changes in zoom as they will cause graphical artifacts
		if (std::abs(sourceZoom - targetZoom) > 0.01f) {
			float z = sourceZoom * (1.f - t) + targetZoom * t;
			rack::settings::zoom = z;
		}

		// Move the view
		// NB: unstable API!
		p = p.mult(APP->scene->rackScroll->zoomWidget->zoom);
		p = p.minus(APP->scene->rackScroll->box.size.mult(0.5f));
		APP->scene->rackScroll->offset = p;

		frame++;
	}
};

struct ViewportCenter {
	ViewportCenter(Widget* w, float zoomToWidget = -1.f) {
		// NB: unstable API!
		Vec target = w->box.pos;
		target = target.plus(w->box.size.mult(0.5f));
		target = target.mult(APP->scene->rackScroll->zoomWidget->zoom);
		target = target.minus(APP->scene->rackScroll->box.size.mult(0.5f));
		APP->scene->rackScroll->offset = target;
		if (zoomToWidget > 0.f) {
			rack::settings::zoom = std::log2(APP->scene->rackScroll->box.size.y / w->box.size.y * zoomToWidget);
		}
	}

	ViewportCenter(Vec target) {
		// NB: unstable API!
		target = target.mult(APP->scene->rackScroll->zoomWidget->zoom);
		target = target.minus(APP->scene->rackScroll->box.size.mult(0.5f));
		APP->scene->rackScroll->offset = target;
	}

	ViewportCenter(Rect rect) {
		// NB: unstable API!
		Vec target = rect.getCenter();
		target = target.mult(APP->scene->rackScroll->zoomWidget->zoom);
		target = target.minus(APP->scene->rackScroll->box.size.mult(0.5f));
		APP->scene->rackScroll->offset = target;
		float zx = std::log2(APP->scene->rackScroll->box.size.x / rect.size.x * 0.9f);
		float zy = std::log2(APP->scene->rackScroll->box.size.y / rect.size.y * 0.9f);
		rack::settings::zoom = std::min(zx, zy);
	}
};

} // namespace Rack
} // namespace StoermelderPackOne