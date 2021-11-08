#pragma once
#include "plugin.hpp"
#include "settings.hpp"

namespace TheModularMind {
namespace Rack {

/** Move the view-port smoothly and center a Widget
 */
struct ViewportCenter {
	ViewportCenter(Widget* w, float zoomToWidget = -1.f) {
		// NB: unstable API!
		Vec target = w->box.pos;
		target = target.plus(w->box.size.mult(0.5f));
		target = target.mult(APP->scene->rackScroll->zoomWidget->zoom);
		target = target.minus(APP->scene->rackScroll->box.size.mult(0.5f));
		APP->scene->rackScroll->offset = target;
		if (zoomToWidget > 0.f) {
			APP->scene->rackScroll->setZoom(std::log2(APP->scene->rackScroll->box.size.y / w->box.size.y * zoomToWidget));
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
		APP->scene->rackScroll->setZoom(std::min(zx, zy));
	}
};

} // namespace Rack
} // namespace TheModularMind