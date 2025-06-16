#include "plugin.hpp"


struct Fuji : Module {
	static const int NUM_CHANNELS = 6;
	enum ParamId {
		ENUMS(ATTACK1_PARAM, NUM_CHANNELS),
		ENUMS(DECAY1_PARAM, NUM_CHANNELS),
		ENUMS(MODE1_PARAM, NUM_CHANNELS),
		ENUMS(LOOP1_PARAM, NUM_CHANNELS),
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(GATE1_INPUT, NUM_CHANNELS),
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(OUT1_OUTPUT, NUM_CHANNELS),
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(NUM1_LIGHT, NUM_CHANNELS),
		LIGHTS_LEN
	};

	Fuji() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < NUM_CHANNELS; i++) {
			configParam(ATTACK1_PARAM + i, 0.f, 1.f, 0.f, string::f("Attack Ch. %d", i + 1));
			configParam(DECAY1_PARAM + i, 0.f, 1.f, 0.f, string::f("Decay Ch. %d", i + 1));
			configSwitch(MODE1_PARAM + i, 0.f, 1.f, 0.f, string::f("Mode Ch. %d", i + 1), {"AD Mode", "Hold Mode"});
			configSwitch(LOOP1_PARAM + i, 0.f, 1.f, 0.f, string::f("Loop Ch. %d", i + 1), {"One Shot", "Loop"});
			configInput(GATE1_INPUT + i, string::f("Gate Ch. %d", i + 1));
			configOutput(OUT1_OUTPUT + i, string::f("Ch. %d", i + 1));
		}
	}

	void process(const ProcessArgs& args) override {

		for (int i = 0; i < NUM_CHANNELS; i++) {
			lights[NUM1_LIGHT + i].setBrightness(params[ATTACK1_PARAM + i].getValue());
		}

	}
};


struct FujiWidget : ModuleWidget {
	FujiWidget(Fuji* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Fuji.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const float first_y = 13.918;
		const float last_y = 105.830;
		const float step_y = (last_y - first_y) / (Fuji::NUM_CHANNELS - 1);


		for (int i = 0; i < Fuji::NUM_CHANNELS; ++i) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(27.502, 15.303 + step_y * i)), module, Fuji::ATTACK1_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(42.002, 15.303 + step_y * i)), module, Fuji::DECAY1_PARAM + i));

			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.661, 15.303 + step_y * i)), module, Fuji::GATE1_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.815, 15.303 + step_y * i)), module, Fuji::OUT1_OUTPUT + i));

			addParam(createParam<CKSSHoriz2>(mm2px(Vec(3.435, 23.155 + step_y * i)), module, Fuji::MODE1_PARAM + i));
			addParam(createParam<CKSSHoriz2>(mm2px(Vec(13.72, 23.155 + step_y * i)), module, Fuji::LOOP1_PARAM + i));
		}

		addChild(createLight<VostokWhiteNumberLed<1>>(mm2px(Vec(31.994, 19.259)), module, Fuji::NUM1_LIGHT + 0));
		addChild(createLight<VostokWhiteNumberLed<2>>(mm2px(Vec(31.994, 37.665)), module, Fuji::NUM1_LIGHT + 1));
		addChild(createLight<VostokWhiteNumberLed<3>>(mm2px(Vec(31.607, 56.040)), module, Fuji::NUM1_LIGHT + 2));
		addChild(createLight<VostokWhiteNumberLed<4>>(mm2px(Vec(31.607, 74.537)), module, Fuji::NUM1_LIGHT + 3));
		addChild(createLight<VostokWhiteNumberLed<5>>(mm2px(Vec(31.941, 92.853)), module, Fuji::NUM1_LIGHT + 4));
		addChild(createLight<VostokWhiteNumberLed<6>>(mm2px(Vec(31.941, 111.296)), module, Fuji::NUM1_LIGHT + 5));


	}
};


Model* modelFuji = createModel<Fuji, FujiWidget>("Fuji");