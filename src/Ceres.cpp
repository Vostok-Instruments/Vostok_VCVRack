#include "plugin.hpp"


struct Ceres : Module {

	static const int NUM_CHANNELS = 6;


	enum ParamId {
		ENUMS(LEVEL1_PARAM, NUM_CHANNELS),
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(IN1_INPUT, NUM_CHANNELS),
		ENUMS(CV1_INPUT, NUM_CHANNELS),
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


	Ceres() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < NUM_CHANNELS; i++) {
			configParam(LEVEL1_PARAM + i, 0.f, 1.f, 0.f, string::f("Ch. %d level", i + 1));
			configInput(IN1_INPUT + i, string::f("Ch. %d", i + 1));
			auto cv = configInput(CV1_INPUT + i, string::f("Ch. %d level CV", i + 1));
			cv->description = "CV is clipped to the range 0-5V, fully open at 5V.";
			auto out = configOutput(OUT1_OUTPUT + i, string::f("Ch. %d", i + 1));
			if (i == NUM_CHANNELS - 1) {
				out->description = "Mix of channels 1-6";
			}
		}
	}

	void process(const ProcessArgs& args) override {
		float normalVoltage = 0.f;
		float mix = 0.f;
		for (int i = 0; i < NUM_CHANNELS; i++) {
			const float level = params[LEVEL1_PARAM + i].getValue() * clamp(inputs[CV1_INPUT + i].getNormalVoltage(5.f) / 5.f, 0.f, 1.f);
			const float in = inputs[IN1_INPUT + i].getNormalVoltage(normalVoltage);
			normalVoltage = in;
			const float out = in * level;
			mix += out;

			outputs[OUT1_OUTPUT + i].setVoltage(out);
			lights[NUM1_LIGHT + i].setBrightness(level);
		}

		// channel 6 is always the mix output
		outputs[OUT1_OUTPUT + 5].setVoltage(mix);
	}
};


struct CeresWidget : ModuleWidget {

	CeresWidget(Ceres* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Ceres.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		constexpr std::array<int, 6> arr{0, 1, 2, 3, 4, 5};

		const float offset_y = 18.343;
		for (const int i : arr) {
			const float y =  15.31 + i * offset_y;
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(41.04, y)), module, Ceres::LEVEL1_PARAM + i));

			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.25, y)), module, Ceres::IN1_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.47, y)), module, Ceres::CV1_INPUT + i));

			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(26.74, y)), module, Ceres::OUT1_OUTPUT + i));
		}

		addChild(createLight<VostokNumberLed<1>>(mm2px(Vec(30.847, 19.259)), module, Ceres::NUM1_LIGHT + 0));
		addChild(createLight<VostokNumberLed<2>>(mm2px(Vec(30.847, 37.665)), module, Ceres::NUM1_LIGHT + 1));
		addChild(createLight<VostokNumberLed<3>>(mm2px(Vec(30.460, 56.040)), module, Ceres::NUM1_LIGHT + 2));
		addChild(createLight<VostokNumberLed<4>>(mm2px(Vec(30.460, 74.537)), module, Ceres::NUM1_LIGHT + 3));
		addChild(createLight<VostokNumberLed<5>>(mm2px(Vec(30.794, 92.853)), module, Ceres::NUM1_LIGHT + 4));
		addChild(createLight<VostokNumberLed<6>>(mm2px(Vec(30.794, 111.296)), module, Ceres::NUM1_LIGHT + 5));

	}
};


Model* modelCeres = createModel<Ceres, CeresWidget>("Ceres");