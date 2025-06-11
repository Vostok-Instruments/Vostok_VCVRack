#include "plugin.hpp"


struct Path : Module {
	enum ParamId {
		ROUTE_PARAM,
		PLUS_MINUS_PARAM,
		ROUTE_CV_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		IN_INPUT,
		ROUTE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		OUT3_OUTPUT,
		OUT4_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		NUM1_LIGHT,
		NUM2_LIGHT,
		NUM3_LIGHT,
		NUM4_LIGHT,
		LIGHTS_LEN
	};

	Path() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(ROUTE_PARAM, 0.f, 1.f, 0.f, "Route");
		configSwitch(PLUS_MINUS_PARAM, 0.f, 1.f, 0.f, "CV Attenuator Polarity", {"+", "-"});
		configParam(ROUTE_CV_PARAM, 0.f, 1.f, 1.f, "Route CV");
		auto in = configInput(IN_INPUT, "Signal");
		in->description = "Normalized to 10V";
		auto routeIn = configInput(ROUTE_INPUT, "Route CV");
		routeIn->description = "Expects 5V peak-to-peak, combines with ROUTE slider";

		configOutput(OUT1_OUTPUT, "Ch. 1");
		configOutput(OUT2_OUTPUT, "Ch. 2");
		configOutput(OUT3_OUTPUT, "Ch. 3");
		configOutput(OUT4_OUTPUT, "Ch. 4");
	}

	void process(const ProcessArgs& args) override {

		const float routeCv = params[ROUTE_CV_PARAM].getValue() * inputs[ROUTE_INPUT].getVoltage() / 5.f * (params[PLUS_MINUS_PARAM].getValue() == 0.f ? 1.f : -1.f);
		const float routeValue = clamp(params[ROUTE_PARAM].getValue() + routeCv, 0.f, 1.f);

		float outGain1 = gainForChannel(0, routeValue);
		float outGain2 = gainForChannel(1, routeValue);
		float outGain3 = gainForChannel(2, routeValue);
		float outGain4 = gainForChannel(3, routeValue);

		float in = inputs[IN_INPUT].getNormalVoltage(10.f);
		outputs[OUT1_OUTPUT].setVoltage(in * outGain1);
		outputs[OUT2_OUTPUT].setVoltage(in * outGain2);
		outputs[OUT3_OUTPUT].setVoltage(in * outGain3);
		outputs[OUT4_OUTPUT].setVoltage(in * outGain4);

		// LEDs
		lights[NUM1_LIGHT].setBrightness(outGain1);
		lights[NUM2_LIGHT].setBrightness(outGain2);
		lights[NUM3_LIGHT].setBrightness(outGain3);
		lights[NUM4_LIGHT].setBrightness(outGain4);
	}

	// centres: 0.0, 1.45, 2.9, 4.35V
	const float centres[4] = {0.0, 1.45, 2.9, 4.35};
	float gainForChannel(int channel, float routeValue) {
		float routeValueForChannel = (routeValue - centres[channel] / 5.f);
				if (channel == 0) { 
			routeValueForChannel = std::max(0.f, routeValueForChannel);
		} else if (channel == 3) {
			routeValueForChannel = std::min(0.f, routeValueForChannel);
		}
		routeValueForChannel = std::abs(routeValueForChannel);
		
		float gain = (channel == 0 || channel == 3) ? 0.89f : 0.865f;
		return gain * std::exp2f(-routeValueForChannel * routeValueForChannel * routeValueForChannel * 290.f);
	}
};


struct PathWidget : ModuleWidget {
	PathWidget(Path* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Path.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<VostokSlider>(mm2px(Vec(6.681, 52.264)), module, Path::ROUTE_PARAM));
		addParam(createParam<CKSSHoriz2>(mm2px(Vec(7.0, 97.063)), module, Path::PLUS_MINUS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.123, 110.004)), module, Path::ROUTE_CV_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.315, 15.203)), module, Path::IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.015, 15.203)), module, Path::ROUTE_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(5.315, 28.16)), module, Path::OUT3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.015, 28.203)), module, Path::OUT4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(5.315, 41.203)), module, Path::OUT1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.015, 41.203)), module, Path::OUT2_OUTPUT));

		addChild(createLight<VostokNumberLed<1>>(mm2px(Vec(2.584, 82.545)), module, Path::NUM1_LIGHT));
		addChild(createLight<VostokNumberLed<2>>(mm2px(Vec(12.89, 72.488)), module, Path::NUM2_LIGHT));
		addChild(createLight<VostokNumberLed<3>>(mm2px(Vec(1.56, 62.668)), module, Path::NUM3_LIGHT));
		addChild(createLight<VostokNumberLed<4>>(mm2px(Vec(12.560, 52.488)), module, Path::NUM4_LIGHT));

	}
};


Model* modelPath = createModel<Path, PathWidget>("Path");