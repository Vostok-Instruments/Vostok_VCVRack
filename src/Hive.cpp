#include "plugin.hpp"


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
		ENUMS(NUM_LIGHT, NUM_CHANNELS),
		LEFT_LIGHT,
		RIGHT_LIGHT,
		LIGHTS_LEN
	};

	Hive() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		for (int i = 0; i < NUM_CHANNELS; ++i) {
			configParam(GAIN_PARAM + i, 0.f, 1.f, 0.f, string::f("Ch. %d Gain", i + 1));
			configParam(PAN_PARAM + i, -1.f, 1.f, 0.f, string::f("Ch. %d Pan", i + 1));
			configInput(LEFT_INPUT + i, string::f("Ch. %d Left", i + 1));
			configInput(RIGHT_INPUT + i, string::f("Ch. %d Right", i + 1));
			configInput(PAN_INPUT + i, "Pan CV " + std::to_string(i + 1));
		}

		configParam(MASTER_PARAM, 0.f, 1.f, 0.f, "Master Gain");

		configOutput(LEFT_OUTPUT, "Left");
		configOutput(RIGHT_OUTPUT, "Right");
	}

	void process(const ProcessArgs& args) override {

		for (int i = 0; i < NUM_CHANNELS; ++i) {
			float left = inputs[LEFT_INPUT + i].getVoltage() * params[GAIN_PARAM + i].getValue();
			float right = inputs[RIGHT_INPUT + i].getVoltage() * params[GAIN_PARAM + i].getValue();

			lights[NUM_LIGHT + i].setBrightness((left + right) / 2.f);
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
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(27.502, 15.303 + gap*i)),module, Hive::GAIN_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(42.002, 15.303 + gap*i)),  module, Hive::PAN_PARAM + i));
		}

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.615, 96.403)), module, Hive::PAN_INPUT + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.815, 96.403)), module, Hive::PAN_INPUT + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.615, 111.403)), module, Hive::PAN_INPUT + 2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.815, 111.403)), module, Hive::PAN_INPUT + 3));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(27.615, 96.403)), module, Hive::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(27.615, 111.403)), module, Hive::RIGHT_OUTPUT));

		addChild(createLight<VostokWhiteNumberLed<1>>(mm2px(Vec(31.994, 19.259)), module, Hive::NUM_LIGHT + 0));
		addChild(createLight<VostokWhiteNumberLed<2>>(mm2px(Vec(31.994, 37.665)), module, Hive::NUM_LIGHT + 1));
		addChild(createLight<VostokWhiteNumberLed<3>>(mm2px(Vec(31.607, 56.040)), module, Hive::NUM_LIGHT + 2));
		addChild(createLight<VostokWhiteNumberLed<4>>(mm2px(Vec(31.607, 74.537)), module, Hive::NUM_LIGHT + 3));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(34.31, 100.043)), module, Hive::LEFT_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(34.31, 107.578)), module, Hive::RIGHT_LIGHT));
	}
};


Model* modelHive = createModel<Hive, HiveWidget>("Hive");