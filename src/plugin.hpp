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

template <typename TBase = GrayModuleLightWidget>
struct TOrangeLight : GrayModuleLightWidget {
	TOrangeLight() {
		this->addBaseColor(SCHEME_ORANGE);
	}
};
using OrangeLight = TOrangeLight<>;


struct VostokLed : TSvgLight<WhiteLight> {
	static constexpr float backgroundGrey = 80.f / 255.f;
	VostokLed() {
	}

	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {

			if (!sw->svg)
				return;

			if (module && !module->isBypassed()) {

				// when LED is off, draw the background (grey value #4d4d4d), but if on then progressively blend away to zero
				float backgroundFactor = std::max(0.f, 1.f - color.a) * backgroundGrey;
				if (backgroundFactor > 0.f) {
					NVGcolor c = nvgRGBf(backgroundFactor, backgroundFactor, backgroundFactor);

					for (auto s = sw->svg->handle->shapes; s; s = s->next) {
						s->fill.color = ((int)(c.a * 255) << 24) + (((int)(c.b * 255)) << 16) + (((int)(c.g * 255)) << 8) + (int)(c.r * 255);
						s->fill.type = NSVG_PAINT_COLOR;
					}

					nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
					svgDraw(args.vg, sw->svg->handle);
				}

				// main RGB color
				const int fillColor = ((int)(color.a * 255) << 24) + (((int)(color.b * 255)) << 16) + (((int)(color.g * 255)) << 8) + (int)(color.r * 255);
				for (auto s = sw->svg->handle->shapes; s; s = s->next) {
					s->fill.color = fillColor;
					s->fill.type = NSVG_PAINT_COLOR;
				}

				nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
				svgDraw(args.vg, sw->svg->handle);
				// drawHalo(args);
			}
		}
		Widget::drawLayer(args, layer);
	}
};


struct VostokLedOrange : TSvgLight<OrangeLight> {
	static constexpr float backgroundGrey = 80.f / 255.f;
	VostokLedOrange() {
	}

	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {

			if (!sw->svg)
				return;

			if (module && !module->isBypassed()) {

				// when LED is off, draw the background (grey value #4d4d4d), but if on then progressively blend away to zero
				float backgroundFactor = std::max(0.f, 1.f - color.a) * backgroundGrey;
				if (backgroundFactor > 0.f) {
					NVGcolor c = nvgRGBf(backgroundFactor, backgroundFactor, backgroundFactor);

					for (auto s = sw->svg->handle->shapes; s; s = s->next) {
						s->fill.color = ((int)(c.a * 255) << 24) + (((int)(c.b * 255)) << 16) + (((int)(c.g * 255)) << 8) + (int)(c.r * 255);
						s->fill.type = NSVG_PAINT_COLOR;
					}

					nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
					svgDraw(args.vg, sw->svg->handle);
				}

				// main RGB color
				const int fillColor = ((int)(color.a * 255) << 24) + (((int)(color.b * 255)) << 16) + (((int)(color.g * 255)) << 8) + (int)(color.r * 255);
				for (auto s = sw->svg->handle->shapes; s; s = s->next) {
					s->fill.color = fillColor;
					s->fill.type = NSVG_PAINT_COLOR;
				}

				nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
				svgDraw(args.vg, sw->svg->handle);
				// drawHalo(args);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

template <int N>
struct VostokNumberLed : VostokLed {
	VostokNumberLed() {
		this->setSvg(Svg::load(asset::plugin(pluginInstance, string::f("res/components/no%d.svg", N))));
	}
};

template <int N>
struct VostokNumberOrangeLed : VostokLedOrange {
	VostokNumberOrangeLed() {
		this->setSvg(Svg::load(asset::plugin(pluginInstance, string::f("res/components/no%d.svg", N))));
	}
};


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
template <typename TLightBase = WhiteLight>
struct VostokLightSlider : LightSlider<VostokSlider, VCVSliderLight<TLightBase>> {
	VostokLightSlider() {}
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
template <typename TBase>
struct VCVSliderLightHoriz : RectangleLight<TSvgLight<TBase>> {
	VCVSliderLightHoriz() {
		this->setSvg(Svg::load(asset::plugin(pluginInstance, "res/ComponentLibrary/VCVSliderLightHoriz.svg")));
	}
};

template <typename TLightBase = WhiteLight>
struct VostokLightSliderHoriz : LightSlider<VostokSliderHoriz, VCVSliderLightHoriz<TLightBase>> {
	VostokLightSliderHoriz() {}
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