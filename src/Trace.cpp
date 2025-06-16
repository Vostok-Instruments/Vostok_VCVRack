#include "plugin.hpp"


struct Trace : Module {
	enum ParamId {
		SCAN_PARAM,
		PLUS_MINUS_PARAM,
		SCAN_CV_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		SCAN_INPUT,
		IN1_INPUT,
		IN2_INPUT,
		IN3_INPUT,
		IN4_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		NUM1_LIGHT,
		NUM2_LIGHT,
		NUM3_LIGHT,
		NUM4_LIGHT,
		LIGHTS_LEN
	};

	Trace() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(SCAN_PARAM, 0.f, 1.f, 0.f, "Scan");
		configSwitch(PLUS_MINUS_PARAM, 0.f, 1.f, 0.f, "CV Attenuator Polarity", {"+", "-"});
		configParam(SCAN_CV_PARAM, 0.f, 1.f, 1.f, "Scan CV", "%", 0.f, 100.f);

		auto scanIn = configInput(SCAN_INPUT, "Scan");
		scanIn->description = "Expects 5V peak-to-peak, combines with SCAN slider";

		configInput(IN1_INPUT, "Ch. 1");
		configInput(IN2_INPUT, "Ch. 2");
		configInput(IN3_INPUT, "Ch. 3");
		configInput(IN4_INPUT, "Ch. 4");

		configOutput(OUT_OUTPUT, "Signal");
	}

	void process(const ProcessArgs& args) override {

		const float scanCv = params[SCAN_CV_PARAM].getValue() * inputs[SCAN_INPUT].getVoltage() / 5.f * (params[PLUS_MINUS_PARAM].getValue() == 0.f ? 1.f : -1.f);
		const float scanValue = clamp(params[SCAN_PARAM].getValue() + scanCv, 0.f, 1.f);

		float inGain1 = gainForChannel(0, scanValue);
		float inGain2 = gainForChannel(1, scanValue);
		float inGain3 = gainForChannel(2, scanValue);
		float inGain4 = gainForChannel(3, scanValue);

		lights[NUM1_LIGHT].setBrightness(inGain1);
		lights[NUM2_LIGHT].setBrightness(inGain2);
		lights[NUM3_LIGHT].setBrightness(inGain3);
		lights[NUM4_LIGHT].setBrightness(inGain4);


		float out = inputs[IN1_INPUT].getVoltage() * inGain1 +
		            inputs[IN2_INPUT].getVoltage() * inGain2 +
		            inputs[IN3_INPUT].getVoltage() * inGain3 +
		            inputs[IN4_INPUT].getVoltage() * inGain4;

		outputs[OUT_OUTPUT].setVoltage(out);
	}

};



struct TraceWidget : ModuleWidget {
	TraceWidget(Trace* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Trace.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<VostokSlider>(mm2px(Vec(6.681, 52.264)), module, Trace::SCAN_PARAM));
		addParam(createParam<CKSSHoriz2>(mm2px(Vec(7.0, 97.063)), module, Trace::PLUS_MINUS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.123, 110.004)), module, Trace::SCAN_CV_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.315, 15.203)), module, Trace::SCAN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.315, 28.16)), module, Trace::IN3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.015, 28.203)), module, Trace::IN4_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.315, 41.203)), module, Trace::IN1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.015, 41.203)), module, Trace::IN2_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.015, 15.203)), module, Trace::OUT_OUTPUT));


		addChild(createLight<VostokOrangeNumberLed<1>>(mm2px(Vec(2.584, 82.545)), module, Trace::NUM1_LIGHT));
		addChild(createLight<VostokOrangeNumberLed<2>>(mm2px(Vec(12.89, 72.488)), module, Trace::NUM2_LIGHT));
		addChild(createLight<VostokOrangeNumberLed<3>>(mm2px(Vec(1.56, 62.668)), module, Trace::NUM3_LIGHT));
		addChild(createLight<VostokOrangeNumberLed<4>>(mm2px(Vec(12.560, 52.488)), module, Trace::NUM4_LIGHT));

	}
};


Model* modelTrace = createModel<Trace, TraceWidget>("Trace");