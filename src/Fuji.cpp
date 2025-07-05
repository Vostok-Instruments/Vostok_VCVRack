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

	enum Mode {
		AD,  // Attack-Decay
		HOLD // Hold
	};
	enum Loop {
		ONE_SHOT, // One Shot
		LOOP      // Loop
	};


	Fuji() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < NUM_CHANNELS; i++) {
			configParam(ATTACK1_PARAM + i, -3.f, 0.f, 0.f, string::f("Attack Ch. %d", i + 1), " s", 10.f);
			configParam(DECAY1_PARAM + i, -3.f, 0.f, 0.f, string::f("Decay Ch. %d", i + 1), " s", 10.f);
			configSwitch(MODE1_PARAM + i, 0.f, 1.f, 0.f, string::f("Mode Ch. %d", i + 1), {"AD Mode", "Hold Mode"});
			configSwitch(LOOP1_PARAM + i, 0.f, 1.f, 0.f, string::f("Loop Ch. %d", i + 1), {"One Shot", "Loop"});
			configInput(GATE1_INPUT + i, string::f("Gate Ch. %d", i + 1));
			configOutput(OUT1_OUTPUT + i, string::f("Ch. %d", i + 1));
		}
	}

	dsp::ExponentialSlewLimiter attackSlew[NUM_CHANNELS];
	dsp::ExponentialSlewLimiter decaySlew[NUM_CHANNELS];
	dsp::SchmittTrigger gateTrigger[NUM_CHANNELS];
	dsp::PulseGenerator pulseGen[NUM_CHANNELS];

	void process(const ProcessArgs& args) override {

		for (int i = 0; i < NUM_CHANNELS; i++) {

			float attackTime = std::pow(10, params[ATTACK1_PARAM + i].getValue()); 	// range 1ms to 1s
			float decayTime = std::pow(10, params[DECAY1_PARAM + i].getValue()); 	// range 1ms to 1s
			const Mode mode = static_cast<Mode>(params[MODE1_PARAM + i].getValue());
			const Loop loop = static_cast<Loop>(params[LOOP1_PARAM + i].getValue());

			if (gateTrigger[i].process(inputs[GATE1_INPUT + i].getVoltage())) {
				pulseGen[i].trigger(1.2 * attackTime);
			}
						
			// Gate is high, start attack phase
			attackSlew[i].setRiseFallTau(attackTime, decayTime);

			float slewSignal = pulseGen[i].process(args.sampleTime);
			if (mode == HOLD) {
				slewSignal = std::max(slewSignal, gateTrigger[i].isHigh() ? 1.f : 0.f);
			}

			float envelope = attackSlew[i].process(args.sampleTime, slewSignal);

			outputs[OUT1_OUTPUT + i].setVoltage(envelope * 10.f); // Scale to 10V
			
			lights[NUM1_LIGHT + i].setBrightness(envelope);
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