#pragma once
#include <rack.hpp>


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelPath;
extern Model* modelTrace;
extern Model* modelAsset;
extern Model* modelAtlas;
extern Model* modelCeres;
extern Model* modelFuji;
extern Model* modelSena;
extern Model* modelHive;

enum COLORS {
	C_WHITE = 0,
	C_ORANGE = 1,
};

template <int N, int COLOR>
struct VostokNumberLedT : ModuleLightWidget {
	static constexpr float backgroundGrey = 80.f / 255.f;

	// Data for N = 1
	static constexpr float circle_xs_1[] = {2.250263, 2.918860, 3.587449, 2.250263, 2.918860, 3.587449, 2.250263, 2.918860, 3.587449, 2.250263, 2.918860, 3.587449, 2.250263, 2.918860, 3.587449, 2.250263, 2.918860, 3.587449, 0.244472, 0.913086, 2.250263, 2.918860, 3.587449, 0.244472, 0.913086, 1.581675, 2.250263, 2.918860, 3.587449, 0.913086, 1.581675, 2.250263, 2.918860, 3.587449, 1.581675, 2.250263, 2.918860, 3.587449};
	static constexpr float circle_ys_1[] = {6.570861, 6.570861, 6.570861, 5.867958, 5.867958, 5.867958, 5.165054, 5.165054, 5.165054, 4.462066, 4.462066, 4.462066, 3.759163, 3.759163, 3.759163, 3.056174, 3.056174, 3.056174, 2.353271, 2.353271, 2.353271, 2.353271, 2.353271, 1.650283, 1.650283, 1.650283, 1.650283, 1.650283, 1.650283, 0.947379, 0.947379, 0.947379, 0.947379, 0.947379, 0.244476, 0.244476, 0.244476, 0.244476};
	static constexpr int num_circles_1 = 38;
	const Vec size_1 = Vec(45.258903, 80.496002);

	// Data for N = 2
	static constexpr float circle_xs_2[] = {0.244477, 0.913066, 0.244477, 0.913066, 0.913066, 0.244477, 0.913066, 0.244477, 0.913066, 0.244477, 0.913066, 0.913066, 1.624707, 2.293304, 2.961893, 3.630484, 4.299078, 4.967667, 1.624707, 2.293304, 2.961893, 3.630484, 4.299078, 4.967667, 1.624707, 2.293304, 2.961893, 1.624707, 2.293304, 2.961893, 3.630484, 2.293304, 2.961893, 3.630484, 4.299078, 2.961893, 3.630484, 4.299078, 4.967667, 1.624707, 3.630484, 4.299078, 4.967667, 1.624707, 3.630484, 4.299078, 4.967667, 1.624707, 2.293304, 2.961893, 3.630484, 4.299078, 4.967667, 1.624707, 2.293304, 2.961893, 3.630484, 4.299078};
	static constexpr float circle_ys_2[] = {6.575858, 6.575858, 5.872954, 5.872954, 5.169966, 2.358267, 2.358267, 1.655279, 1.655279, 0.952375, 0.952375, 0.249387, 6.570862, 6.570862, 6.570862, 6.570862, 6.570862, 6.570862, 5.867959, 5.867959, 5.867959, 5.867959, 5.867959, 5.867959, 5.165055, 5.165055, 5.165055, 4.462067, 4.462067, 4.462067, 4.462067, 3.759164, 3.759164, 3.759164, 3.759164, 3.056175, 3.056175, 3.056175, 3.056175, 2.353272, 2.353272, 2.353272, 2.353272, 1.650284, 1.650284, 1.650284, 1.650284, 0.947380, 0.947380, 0.947380, 0.947380, 0.947380, 0.947380, 0.244477, 0.244477, 0.244477, 0.244477, 0.244477};
	static constexpr int num_circles_2 = 58;
	const Vec size_2 = Vec(5.212, 6.820);

	// Data for N = 3
	static constexpr float circle_xs_3[] = {0.913063, 0.244474, 0.913063, 0.244474, 0.913063, 0.913063, 0.913063, 1.624704, 2.293301, 2.961890, 3.630478, 4.299075, 1.624704, 2.293301, 2.961890, 3.630478, 4.299075, 4.967664, 1.624704, 3.630478, 4.299075, 4.967664, 5.636236, 4.299075, 4.967664, 5.636236, 1.624704, 2.293301, 2.961890, 3.630478, 4.299075, 4.967664, 5.636236, 1.624704, 2.293301, 2.961890, 3.630478, 4.299075, 4.967664, 2.293301, 2.961890, 3.630478, 2.961890, 3.630478, 4.299075, 1.624704, 2.293301, 2.961890, 3.630478, 4.299075, 4.967664, 1.624704, 2.293301, 2.961890, 3.630478, 4.299075, 4.967664};
	static constexpr float circle_ys_3[] = {6.575858, 5.872954, 5.872954, 5.169966, 5.169966, 0.952375, 0.249387, 6.570862, 6.570862, 6.570862, 6.570862, 6.570862, 5.867959, 5.867959, 5.867959, 5.867959, 5.867959, 5.867959, 5.165055, 5.165055, 5.165055, 5.165055, 5.169966, 4.462067, 4.462067, 4.467062, 3.759164, 3.759164, 3.759164, 3.759164, 3.759164, 3.759164, 3.764159, 3.056175, 3.056175, 3.056175, 3.056175, 3.056175, 3.056175, 2.353272, 2.353272, 2.353272, 1.650284, 1.650284, 1.650284, 0.947380, 0.947380, 0.947380, 0.947380, 0.947380, 0.947380, 0.244477, 0.244477, 0.244477, 0.244477, 0.244477, 0.244477};
	static constexpr int num_circles_3 = 57;
	const Vec size_3 = Vec(5.88, 6.828);

	// Data for N = 4
	static constexpr float circle_xs_4[] = {0.244475, 0.913004, 0.244475, 0.913004, 0.244475, 0.913004, 0.244475, 0.913004, 0.913004, 3.630470, 4.299084, 4.967614, 3.630470, 4.299084, 4.967614, 1.624713, 2.293242, 2.961856, 3.630470, 4.299084, 4.967614, 5.636228, 1.624713, 2.293242, 2.961856, 3.630470, 4.299084, 4.967614, 5.636228, 1.624713, 3.630470, 4.299084, 4.967614, 1.624713, 2.293242, 3.630470, 4.299084, 4.967614, 1.624713, 2.293242, 2.961856, 3.630470, 4.299084, 4.967614, 1.624713, 2.293242, 2.961856, 3.630470, 4.299084, 4.967614, 2.293242, 2.961856, 3.630470, 4.299084, 4.967614, 2.961856, 3.630470, 4.299084, 4.967614};
	static constexpr float circle_ys_4[] = {5.169965, 5.169965, 4.467061, 4.467061, 3.764158, 3.764158, 3.061170, 3.061170, 2.358266, 6.570861, 6.570861, 6.570861, 5.867958, 5.867958, 5.867958, 5.165054, 5.165054, 5.165054, 5.165054, 5.165054, 5.165054, 5.169965, 4.462066, 4.462066, 4.462066, 4.462066, 4.462066, 4.462066, 4.467061, 3.759163, 3.759163, 3.759163, 3.759163, 3.056174, 3.056174, 3.056174, 3.056174, 3.056174, 2.353271, 2.353271, 2.353271, 2.353271, 2.353271, 2.353271, 1.650283, 1.650283, 1.650283, 1.650283, 1.650283, 1.650283, 0.947379, 0.947379, 0.947379, 0.947379, 0.947379, 0.244476, 0.244476, 0.244476, 0.244476};
	static constexpr int num_circles_4 = 59;
	const Vec size_4 = Vec(5.881, 6.815);

	// Data for N = 5
	static constexpr float circle_xs_5[] = {0.244478, 0.244478, 0.956102, 1.624714, 2.293327, 2.961939, 3.630467, 0.956102, 1.624714, 2.293327, 2.961939, 3.630467, 4.299080, 0.956102, 1.624714, 2.961939, 3.630467, 4.299080, 4.967693, 3.630467, 4.299080, 4.967693, 0.956102, 1.624714, 2.961939, 3.630467, 4.299080, 4.967693, 0.956102, 1.624714, 2.293327, 2.961939, 3.630467, 4.299080, 0.956102, 1.624714, 2.293327, 2.961939, 3.630467, 0.956102, 1.624714, 0.956102, 1.624714, 2.293327, 2.961939, 3.630467, 4.299080, 4.967693, 0.956102, 1.624714, 2.293327, 2.961939, 3.630467, 4.299080, 4.967693};
	static constexpr float circle_ys_5[] = {5.872949, 5.169962, 6.570856, 6.570856, 6.570856, 6.570856, 6.570856, 5.867954, 5.867954, 5.867954, 5.867954, 5.867954, 5.867954, 5.165051, 5.165051, 5.165051, 5.165051, 5.165051, 5.169962, 4.462064, 4.462064, 4.467059, 3.759161, 3.759161, 3.759161, 3.759161, 3.759161, 3.764156, 3.056174, 3.056174, 3.056174, 3.056174, 3.056174, 3.056174, 2.353271, 2.353271, 2.353271, 2.353271, 2.353271, 1.650284, 1.650284, 0.947381, 0.947381, 0.947381, 0.947381, 0.947381, 0.947381, 0.952377, 0.244479, 0.244479, 0.244479, 0.244479, 0.244479, 0.244479, 0.249389};
	static constexpr int num_circles_5 = 55;
	const Vec size_5 = Vec(5.121, 6.815);

	// Data for N = 6
	static constexpr float circle_xs_6[] = {0.244478, 0.244478, 0.244478, 0.244478, 0.244478, 1.624714, 2.293327, 2.961855, 3.630467, 0.956101, 1.624714, 2.293327, 2.961855, 3.630467, 4.299080, 0.956101, 1.624714, 2.293327, 2.961855, 3.630467, 4.299080, 4.967692, 0.956101, 1.624714, 3.630467, 4.299080, 4.967692, 0.956101, 1.624714, 3.630467, 4.299080, 4.967692, 0.956101, 1.624714, 2.293327, 2.961855, 3.630467, 4.299080, 4.967692, 0.956101, 1.624714, 2.293327, 2.961855, 3.630467, 4.299080, 0.956101, 1.624714, 2.293327, 1.624714, 2.293327, 2.961855, 2.293327, 2.961855, 3.630467};
	static constexpr float circle_ys_6[] = {5.169963, 4.467060, 3.764157, 3.061170, 2.358268, 6.570857, 6.570857, 6.570857, 6.570857, 5.867955, 5.867955, 5.867955, 5.867955, 5.867955, 5.867955, 5.165052, 5.165052, 5.165052, 5.165052, 5.165052, 5.165052, 5.169963, 4.462065, 4.462065, 4.462065, 4.462065, 4.467060, 3.759162, 3.759162, 3.759162, 3.759162, 3.764157, 3.056175, 3.056175, 3.056175, 3.056175, 3.056175, 3.056175, 3.061170, 2.353272, 2.353272, 2.353272, 2.353272, 2.353272, 2.353272, 1.650285, 1.650285, 1.650285, 0.947382, 0.947382, 0.947382, 0.244480, 0.244480, 0.244480};
	static constexpr int num_circles_6 = 54;
	const Vec size_6 = Vec(5.212, 6.815);

	// Pointers to selected data
	const float* circle_xs;
	const float* circle_ys;
	int num_circles;

	static constexpr float radius = 0.75;
	const float factor = 1.f / (SVG_DPI / MM_PER_IN); // factor to convert from mm to px

	VostokNumberLedT() {
		if constexpr(N == 1) {
			circle_xs = circle_xs_1;
			circle_ys = circle_ys_1;
			num_circles = num_circles_1;
			this->box.size = mm2px(size_1);

		}
		else if constexpr(N == 2) {
			circle_xs = circle_xs_2;
			circle_ys = circle_ys_2;
			num_circles = num_circles_2;
			this->box.size = mm2px(size_2);
		}
		else if constexpr(N == 3) {
			circle_xs = circle_xs_3;
			circle_ys = circle_ys_3;
			num_circles = num_circles_3;
			this->box.size = mm2px(size_3);
		}
		else if constexpr(N == 4) {
			circle_xs = circle_xs_4;
			circle_ys = circle_ys_4;
			num_circles = num_circles_4;
			this->box.size = mm2px(Vec(6.0, 6.0));
		}
		else if constexpr(N == 5) {
			circle_xs = circle_xs_5;
			circle_ys = circle_ys_5;
			num_circles = num_circles_5;
			this->box.size = mm2px(size_5);
		}
		else if constexpr(N == 6) {
			circle_xs = circle_xs_6;
			circle_ys = circle_ys_6;
			num_circles = num_circles_6;
			this->box.size = mm2px(size_6);
		}
		else {
			circle_xs = nullptr;
			circle_ys = nullptr;
			num_circles = 0;
		}
		this->box.pos = mm2px(Vec(0.0, 0.0)); // position of the LED

		if constexpr(COLOR == C_WHITE) {
			this->addBaseColor(SCHEME_WHITE);
		}
		else if constexpr(COLOR == C_ORANGE) {
			this->addBaseColor(SCHEME_ORANGE);
		}
		else {
			this->addBaseColor(SCHEME_WHITE); // default to white if COLOR is not recognized
		}
	}

	void draw(const DrawArgs& args) override {

		NVGcolor c = nvgRGBf(backgroundGrey, backgroundGrey, backgroundGrey);
		nvgFillColor(args.vg, c);

		for (int i = 0; i < num_circles; i++) {
			// draw the circle
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, circle_xs[i] / factor, circle_ys[i] / factor, radius);

			nvgFill(args.vg); // this is not needed as we set the fill color above
		}

	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {

			if (module && !module->isBypassed()) {

				// main RGB color
				nvgFillColor(args.vg, color);
				for (int i = 0; i < num_circles; i++) {

					// draw the circle
					nvgBeginPath(args.vg);
					nvgCircle(args.vg, circle_xs[i] / factor, circle_ys[i] / factor, radius);
					nvgFill(args.vg); // this is not needed as we set the fill color above
				}
			}
		}

		Widget::drawLayer(args, layer);
	}
};

template <int N>
using VostokWhiteNumberLed = VostokNumberLedT<N, C_WHITE>;

template <int N>
using VostokOrangeNumberLed = VostokNumberLedT<N, C_ORANGE>;

// derived from VCVSlider
struct VostokSlider : app::SvgSlider {
	VostokSlider() {
		setBackgroundSvg(Svg::load(asset::plugin(pluginInstance, "res/components/VCVSlider.svg")));
		setHandleSvg(Svg::load(asset::plugin(pluginInstance, "res/components/VCVSliderHandle.svg")));
		setHandlePosCentered(
		  //math::Vec(19.84260/2, 136.535 - 11.74218/2),
		  math::Vec(19.84260 / 2, 109.545 - 11.74218 / 2),
		  math::Vec(19.84260 / 2, 0.0 + 11.74218 / 2)
		);
	}
};

// derived from VCVSlider
struct VostokSliderHoriz : app::SvgSlider {
	VostokSliderHoriz() {
		setBackgroundSvg(Svg::load(asset::plugin(pluginInstance, "res/components/VCVSliderHoriz.svg")));
		setHandleSvg(Svg::load(asset::plugin(pluginInstance, "res/components/VCVSliderHandleHoriz.svg")));
		setHandlePosCentered(
		  math::Vec(0.0    + 11.74218 / 2, 19.84260 / 2),
		  math::Vec(81.545 - 11.74218 / 2, 19.84260 / 2)
		);
		horizontal = true;
	}
};




struct CKSSHoriz2 : app::SvgSwitch {
	CKSSHoriz2() {
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SwitchNarrowHoriz_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SwitchNarrowHoriz_2.svg")));
	}
};

struct CKSSHoriz3 : app::SvgSwitch {
	CKSSHoriz3() {
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SwitchNarrowHoriz_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SwitchNarrowHoriz_1.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SwitchNarrowHoriz_2.svg")));
	}
};

struct CKSSNarrow : app::SvgSwitch {
	CKSSNarrow() {
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SwitchNarrow_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SwitchNarrow_2.svg")));
	}
};

// const float centres[4] = {0.0, 1.45, 2.9, 4.35};  	// measured values
static constexpr float centres[4] = {0.5f, 0.5f + 4.0f / 3.0f, 0.5f + 2.f * 4.0f / 3.f, 4.5};  	// values adjusted for symmetry etc
static float gainForChannel(int channel, float routeValue) {
	float routeValueForChannel = (routeValue - centres[channel] / 5.f);
	if (channel == 0) {
		routeValueForChannel = std::max(0.f, routeValueForChannel);
	}
	else if (channel == 3) {
		routeValueForChannel = std::min(0.f, routeValueForChannel);
	}
	routeValueForChannel = std::abs(routeValueForChannel);

	float gain = (channel == 0 || channel == 3) ? 0.89f : 0.865f;
	return gain * std::exp2f(-routeValueForChannel * routeValueForChannel * routeValueForChannel * 290.f);
}