#include "plugin.hpp"


struct Sena : Module {

	static const int NUM_CHANNELS = 4;

	enum ParamId {
		ENUMS(FREQ1_PARAM, NUM_CHANNELS),
		ENUMS(MOD1_PARAM, NUM_CHANNELS),
		ENUMS(VCO_LFO_MODE1_PARAM, NUM_CHANNELS),
		ENUMS(VOCT_FM1_PARAM, NUM_CHANNELS),
		ENUMS(FINE1_PARAM, NUM_CHANNELS),
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(VOCT1_INPUT, NUM_CHANNELS),
		ENUMS(MOD1_INPUT, NUM_CHANNELS),
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(OUT1_OUTPUT, NUM_CHANNELS),
		WHITE_OUTPUT,
		PINK_OUTPUT,
		BLUE_OUTPUT,
		BROWN_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(NUM1_LIGHT, NUM_CHANNELS),
		LIGHTS_LEN
	};

	const std::string modeNames[4] = { "Fold", "Shape", "Phase", "PWM" };
	const std::string channelNames[4] = { "Sine", "Triangle", "Saw", "Square" };

	Sena() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < NUM_CHANNELS; ++i) {
			configParam(FREQ1_PARAM + i, 0.f, 1.f, 0.f, "Frequency");
			configParam(MOD1_PARAM + i, 0.f, 1.f, 0.f, modeNames[i]);
			configSwitch(VCO_LFO_MODE1_PARAM + i, 0.f, 1.f, 1.f, "Rate Mode", {"LFO", "VCO"});
			configSwitch(VOCT_FM1_PARAM + i, 0.f, 1.f, 0.f, "FM Type", {"V/OCT", "FM"});
			configSwitch(FINE1_PARAM + i, 0.f, 1.f, 0.f, "Fine Tune", {"Coarse", "Fine"});
			configInput(VOCT1_INPUT + i, string::f("%s v/oct", channelNames[i]));
			configInput(MOD1_INPUT + i, string::f("%s CV", modeNames[i]));
			configOutput(OUT1_OUTPUT + i, string::f("%s", channelNames[i]));

		}

		configOutput(WHITE_OUTPUT, "");
		configOutput(PINK_OUTPUT, "");
		configOutput(BLUE_OUTPUT, "");
		configOutput(BROWN_OUTPUT, "");
	}

	void process(const ProcessArgs& args) override {
	}
};


struct SenaWidget : ModuleWidget {
	SenaWidget(Sena* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Sena.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const float first_y = 15.269;
		const float last_y = 85.422;

		const float step_y = (last_y - first_y) / (Sena::NUM_CHANNELS - 1);
		for (int i = 0; i < Sena::NUM_CHANNELS; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(41.096, 15.269 + step_y * i)), module, Sena::FREQ1_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(58.808, 15.269 + step_y * i)), module, Sena::MOD1_PARAM + i));
			addParam(createParam<CKSSNarrow>(mm2px(Vec(48.339, 21.438 + step_y * i)), module, Sena::VCO_LFO_MODE1_PARAM + i));
			addParam(createParam<CKSSHoriz2>(mm2px(Vec(8.802, 23.292 + step_y * i)), module, Sena::VOCT_FM1_PARAM + i));
			addParam(createParam<CKSSHoriz2>(mm2px(Vec(19.579, 23.292 + step_y * i)), module, Sena::FINE1_PARAM + i));

			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.264, 15.269 + step_y * i)), module, Sena::VOCT1_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.433, 15.269 + step_y * i)), module, Sena::MOD1_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(26.601, 15.269 + step_y * i)), module, Sena::OUT1_OUTPUT + i));

		}

		// noise
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(21.43, 112.652)), module, Sena::WHITE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(30.647, 112.652)), module, Sena::PINK_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.863, 112.652)), module, Sena::BLUE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(49.079, 112.652)), module, Sena::BROWN_OUTPUT));

		// leds
	}
};


Model* modelSena = createModel<Sena, SenaWidget>("Sena");