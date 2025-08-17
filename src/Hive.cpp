#include "plugin.hpp"
#include "ChowDSP.hpp"

using simd::float_4;
using simd::Vector;


struct Hive : Module {

	static const int NUM_CHANNELS = 4;

	enum ParamId {
		ENUMS(GAIN_PARAM, NUM_CHANNELS),
		ENUMS(PAN_PARAM, NUM_CHANNELS),
		MASTER_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(LEFT_INPUT, NUM_CHANNELS),
		ENUMS(RIGHT_INPUT, NUM_CHANNELS),
		ENUMS(PAN_INPUT, NUM_CHANNELS),
		INPUTS_LEN
	};
	enum OutputId {
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(NUM_LIGHT, NUM_CHANNELS * 2),
		LEFT_LIGHT,
		RIGHT_LIGHT,
		LIGHTS_LEN
	};
	enum StereoSide {
		LEFT = 0,
		RIGHT = 1,
		NUM_SIDES = 2
	};

	chowdsp::TBiquadFilter<float_4> dcBlockFilter[NUM_SIDES];
	bool clipOutput = true;
	bool acCoupling = true;
	dsp::ClockDivider lightDivider;

	dsp::VuMeter2 leftMeter, rightMeter;

	Hive() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		for (int i = 0; i < NUM_CHANNELS; ++i) {
			configParam(GAIN_PARAM + i, 0.f, 2.818382f, 1.f, string::f("Ch. %d Gain", i + 1), " dB", -10, 20);
			configParam(PAN_PARAM + i, -1.f, 1.f, 0.f, string::f("Ch. %d Pan", i + 1));
			configInput(LEFT_INPUT + i, string::f("Ch. %d Left", i + 1));
			configInput(RIGHT_INPUT + i, string::f("Ch. %d Right", i + 1));
			auto panCV = configInput(PAN_INPUT + i, "Pan CV " + std::to_string(i + 1));
			panCV->description = "5Vp-p bipolar CV for panning, sums with the pan knob";
		}

		configParam(MASTER_PARAM, 0.f, 1.f, 0.f, "Master Gain", " dB", -10, 20);

		configOutput(LEFT_OUTPUT, "Left");
		configOutput(RIGHT_OUTPUT, "Right");

		lightDivider.setDivision(lightUpdateRate);
	}

	void onSampleRateChange() override {
		float sampleRate = APP->engine->getSampleRate();

		// this doesn't work with floats below ~0.0004
		const float fc = std::max(0.0004, (30. / sampleRate));
		dcBlockFilter[0].setParameters(chowdsp::TBiquadFilter<float_4>::HIGHPASS, fc, 0.707, 1.0f);
		dcBlockFilter[1].setParameters(chowdsp::TBiquadFilter<float_4>::HIGHPASS, fc, 0.707, 1.0f);

		dcBlockFilter[0].reset();
		dcBlockFilter[1].reset();
	}

	void process(const ProcessArgs& args) override {

		float_4 leftIns = float_4(
		                    inputs[LEFT_INPUT + 0].getVoltage(),
		                    inputs[LEFT_INPUT + 1].getVoltage(),
		                    inputs[LEFT_INPUT + 2].getVoltage(),
		                    inputs[LEFT_INPUT + 3].getVoltage());

		float_4 rightIns = float_4(
		                     inputs[RIGHT_INPUT + 0].getNormalVoltage(inputs[LEFT_INPUT + 0].getVoltage()),
		                     inputs[RIGHT_INPUT + 1].getNormalVoltage(inputs[LEFT_INPUT + 1].getVoltage()),
		                     inputs[RIGHT_INPUT + 2].getNormalVoltage(inputs[LEFT_INPUT + 2].getVoltage()),
		                     inputs[RIGHT_INPUT + 3].getNormalVoltage(inputs[LEFT_INPUT + 3].getVoltage()));

		float_4 panCvIns = float_4(
		                     inputs[PAN_INPUT + 0].getVoltage(),
		                     inputs[PAN_INPUT + 1].getVoltage(),
		                     inputs[PAN_INPUT + 2].getVoltage(),
		                     inputs[PAN_INPUT + 3].getVoltage());
		float_4 panParams = float_4(
		                      params[PAN_PARAM + 0].getValue(),
		                      params[PAN_PARAM + 1].getValue(),
		                      params[PAN_PARAM + 2].getValue(),
		                      params[PAN_PARAM + 3].getValue());
		float_4 pan = simd::clamp(panParams + panCvIns / 2.5f, -1.f, 1.f);

		float_4 gains = float_4(
		                  params[GAIN_PARAM + 0].getValue(),
		                  params[GAIN_PARAM + 1].getValue(),
		                  params[GAIN_PARAM + 2].getValue(),
		                  params[GAIN_PARAM + 3].getValue());


		// mixer is AC coupled (by default)
		if (acCoupling) {
			leftIns = dcBlockFilter[LEFT].process(leftIns);
			rightIns = dcBlockFilter[RIGHT].process(rightIns);
		}

		const float_4 panLeft = simd::clamp(1 - pan, 0.f, 1.f);
		const float_4 panRight = simd::clamp(1 + pan, 0.f, 1.f);

		// custom pan law, with  -1.5dB of center attenuation
		leftIns *= gains * simd::sqrt(panLeft) * panLeft;
		rightIns *= gains * simd::sqrt(panRight) * panRight;

		// outputs
		float masterGain = std::pow(params[MASTER_PARAM].getValue(), 2);
		float leftSum = masterGain * (leftIns[0] + leftIns[1] + leftIns[2] + leftIns[3]);
		float rightSum = masterGain * (rightIns[0] + rightIns[1] + rightIns[2] + rightIns[3]);

		if (clipOutput) {
			leftSum = clamp(leftSum, -10.f, 10.f);
			rightSum = clamp(rightSum, -10.f, 10.f);
		}

		outputs[LEFT_OUTPUT].setVoltage(leftSum);
		outputs[RIGHT_OUTPUT].setVoltage(rightSum);

		leftMeter.process(args.sampleTime, std::abs(leftSum / 6.f));
		rightMeter.process(args.sampleTime, std::abs(rightSum / 6.f));

		float_4 leftsForLights = simd::abs(leftIns / 12.f);
		float_4 rightsForLights = simd::abs(rightIns / 12.f);
		for (int i = 0; i < NUM_CHANNELS; ++i) {
			lights[NUM_LIGHT + i * 2].setBrightnessSmooth(leftsForLights[i], args.sampleTime);
			lights[NUM_LIGHT + i * 2 + 1].setBrightnessSmooth(rightsForLights[i], args.sampleTime);
		}

		if (lightDivider.process()) {
			const float sampleTime = args.sampleTime * lightUpdateRate;
			lights[LEFT_LIGHT].setBrightnessSmooth(leftMeter.getBrightness(-3.0f, 0.f), sampleTime);
			lights[RIGHT_LIGHT].setBrightnessSmooth(rightMeter.getBrightness(-3.0f, 0.f), sampleTime);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "clipOutput", json_boolean(clipOutput));
		json_object_set_new(rootJ, "acCoupling", json_boolean(acCoupling));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* clipOutputJ = json_object_get(rootJ, "clipOutput");
		if (clipOutputJ) {
			clipOutput = json_is_true(clipOutputJ);
		}

		json_t* acCouplingJ = json_object_get(rootJ, "acCoupling");
		if (acCouplingJ) {
			acCoupling = json_is_true(acCouplingJ);
		}
	}
};


struct HiveWidget : ModuleWidget {
	HiveWidget(Hive* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Hive.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(42.002, 103.803)), module, Hive::MASTER_PARAM));

		const float gap = 33.752 - 15.396;
		for (int i = 0; i < Hive::NUM_CHANNELS; ++i) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.615, 15.396 + gap * i)), module, Hive::LEFT_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.815, 15.403 + gap * i)), module, Hive::RIGHT_INPUT + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(27.502, 15.303 + gap * i)), module, Hive::GAIN_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(42.002, 15.303 + gap * i)),  module, Hive::PAN_PARAM + i));
		}

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.615, 96.403)), module, Hive::PAN_INPUT + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.815, 96.403)), module, Hive::PAN_INPUT + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.615, 111.403)), module, Hive::PAN_INPUT + 2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.815, 111.403)), module, Hive::PAN_INPUT + 3));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(27.615, 96.403)), module, Hive::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(27.615, 111.403)), module, Hive::RIGHT_OUTPUT));

		addChild(createLight<VostokUpperWhiteNumberLed<1>>(mm2px(Vec(31.994, 19.259)), module, Hive::NUM_LIGHT + 2 * 0 + 0)); 	// white top
		addChild(createLight<VostokLowerOrangeNumberLed<1>>(mm2px(Vec(31.994, 19.259)), module, Hive::NUM_LIGHT + 2 * 0 + 1)); 	// orange bottom
		addChild(createLight<VostokUpperWhiteNumberLed<2>>(mm2px(Vec(31.994, 37.665)), module, Hive::NUM_LIGHT + 2 * 1 + 0));
		addChild(createLight<VostokLowerOrangeNumberLed<2>>(mm2px(Vec(31.994, 37.665)), module, Hive::NUM_LIGHT + 2 * 1 + 1));
		addChild(createLight<VostokUpperWhiteNumberLed<3>>(mm2px(Vec(31.607, 56.040)), module, Hive::NUM_LIGHT + 2 * 2 + 0));
		addChild(createLight<VostokLowerOrangeNumberLed<3>>(mm2px(Vec(31.607, 56.040)), module, Hive::NUM_LIGHT + 2 * 2 + 1));
		addChild(createLight<VostokUpperWhiteNumberLed<4>>(mm2px(Vec(31.607, 74.537)), module, Hive::NUM_LIGHT + 2 * 3 + 0));
		addChild(createLight<VostokLowerOrangeNumberLed<4>>(mm2px(Vec(31.607, 74.537)), module, Hive::NUM_LIGHT + 2 * 3 + 1));

		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(34.31, 100.043)), module, Hive::LEFT_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(34.31, 107.578)), module, Hive::RIGHT_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		Hive* hive = dynamic_cast<Hive*>(module);
		assert(hive);

		menu->addChild(new MenuSeparator());

		menu->addChild(createBoolPtrMenuItem("AC coupling", "", &hive->acCoupling));
		menu->addChild(createBoolPtrMenuItem("Clip Output ±10V", "", &hive->clipOutput));
	}
};


Model* modelHive = createModel<Hive, HiveWidget>("Hive");