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

	enum GateMode {
		AD,  // Attack-Decay
		HOLD // Hold
	};
	enum LoopMode {
		ONE_SHOT, // One Shot
		LOOP      // Loop
	};


	Fuji() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < NUM_CHANNELS; i++) {
			configParam(ATTACK1_PARAM + i, -3.f, 0.f, -1.5f, string::f("Attack Ch. %d", i + 1), " s", 10.f);
			configParam(DECAY1_PARAM + i, -3.f, 0.f, -1.5f, string::f("Decay Ch. %d", i + 1), " s", 10.f);
			configSwitch(MODE1_PARAM + i, 0.f, 1.f, 0.f, string::f("Mode Ch. %d", i + 1), {"AD Mode", "Hold Mode"});
			configSwitch(LOOP1_PARAM + i, 0.f, 1.f, 0.f, string::f("Loop Ch. %d", i + 1), {"One Shot", "Loop"});
			auto gateInput = configInput(GATE1_INPUT + i, string::f("Gate Ch. %d", i + 1));
			if (i > 0) {
				gateInput->description = "Normalled to gate input for channel " + string::f("%d", i);
			}
			configOutput(OUT1_OUTPUT + i, string::f("Ch. %d", i + 1));
		}

		lightDivider.setDivision(lightUpdateRate);
	}

	dsp::ExponentialSlewLimiter attackSlew[NUM_CHANNELS];
	dsp::ExponentialSlewLimiter decaySlew[NUM_CHANNELS];
	dsp::SchmittTrigger gateTrigger[NUM_CHANNELS];
	dsp::SchmittTrigger loopTrigger[NUM_CHANNELS];
	dsp::PulseGenerator pulseGen[NUM_CHANNELS];
	dsp::ClockDivider lightDivider;
	bool rising[NUM_CHANNELS] = { false, false, false, false, false, false };
	bool triggerChannels[NUM_CHANNELS] = { true, true, true, true, true, true };

	void process(const ProcessArgs& args) override {
		const bool updateLEDs = lightDivider.process();

		float gateNormalVoltage = 0.f;
		for (int i = 0; i < NUM_CHANNELS; i++) {

			float attackTime = std::pow(10, params[ATTACK1_PARAM + i].getValue()); 	// range 1ms to 1s
			float decayTime = std::pow(10, params[DECAY1_PARAM + i].getValue()); 	// range 1ms to 1s
			const GateMode gateMode = static_cast<GateMode>(params[MODE1_PARAM + i].getValue());
			const LoopMode loopMode = static_cast<LoopMode>(params[LOOP1_PARAM + i].getValue());

			const float gateInput = inputs[GATE1_INPUT + i].getNormalVoltage(gateNormalVoltage);
			const bool gateTriggered = gateTrigger[i].process(gateInput);
			gateNormalVoltage = gateInput;

			// if we're not in loop mode
			if (loopMode == ONE_SHOT) {
				// rising edge to gate input can trigger attack phase
				if (gateTriggered) {
					rising[i] = true;
				}
				// or suppress the disabling of the rising edge by later code if in hold mode
				if (gateMode == HOLD && gateTrigger[i].isHigh()) {
					rising[i] = true; // Continue rising edge if gate is held
				}
			}
			else if (loopMode == LOOP) {
				// loop mode ignores the gate input but resets in AD mode
				if (triggerChannels[i] || gateTriggered) {
					rising[i] = true; // Start rising edge
					triggerChannels[i] = false; // Reset trigger state for this channel
				}
			}

			// Gate is high, start attack phase
			attackSlew[i].setRiseFallTau(attackTime, decayTime);

			float slewSignal = rising[i];
			if (gateMode == HOLD && loopMode == ONE_SHOT) {
				slewSignal = std::max(slewSignal, gateTrigger[i].isHigh() ? 1.f : 0.f);
			}
			if (loopMode == LOOP) {
				slewSignal = 2.f * slewSignal - 1.f; // Convert to bipolar signal for loop mode
			}

			attackSlew[i].process(args.sampleTime, slewSignal);
			attackSlew[i].out = clamp(attackSlew[i].out, -0.8f, 0.8f);
			float envelope = attackSlew[i].out;

			float out = 0.f;
			if (loopMode == LOOP) {
				if (envelope < -0.799f) {
					triggerChannels[i] = true; // Set trigger state for this channel
				}
				else if (envelope > 0.799) {
					rising[i] = false; // End rising edge
				}

				// envelope is in the range [-0.8, 0.8], so scale to [-8, 8]
				out = 10.f * envelope;
			}
			else {
				if (envelope > 0.799) {
					rising[i] = false; // End rising edge
				}
				out = clamp(10.f * envelope, 0.f, 8.f);
			}

			outputs[OUT1_OUTPUT + i].setVoltage(out);

			if (updateLEDs) {
				const float sampleTime = args.sampleTime * lightUpdateRate;
				lights[NUM1_LIGHT + i].setBrightnessSmooth(envelope, sampleTime, lambda);
			}
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

		addChild(createLight<VostokOrangeNumberLed<1>>(mm2px(Vec(31.994, 19.259)), module, Fuji::NUM1_LIGHT + 0));
		addChild(createLight<VostokOrangeNumberLed<2>>(mm2px(Vec(31.994, 37.665)), module, Fuji::NUM1_LIGHT + 1));
		addChild(createLight<VostokOrangeNumberLed<3>>(mm2px(Vec(31.607, 56.040)), module, Fuji::NUM1_LIGHT + 2));
		addChild(createLight<VostokOrangeNumberLed<4>>(mm2px(Vec(31.607, 74.537)), module, Fuji::NUM1_LIGHT + 3));
		addChild(createLight<VostokOrangeNumberLed<5>>(mm2px(Vec(31.941, 92.853)), module, Fuji::NUM1_LIGHT + 4));
		addChild(createLight<VostokOrangeNumberLed<6>>(mm2px(Vec(31.941, 111.296)), module, Fuji::NUM1_LIGHT + 5));


	}
};


Model* modelFuji = createModel<Fuji, FujiWidget>("Fuji");