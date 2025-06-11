#include "plugin.hpp"
#include "ripples.hpp"

struct Atlas : Module {
	static const int NUM_CHANNELS = 4;
	enum ParamId {
		ENUMS(FREQ1_PARAM, NUM_CHANNELS),
		ENUMS(RES1_PARAM, NUM_CHANNELS),
		ENUMS(FM_RES_1_PARAM, NUM_CHANNELS),
		ENUMS(MODE1_PARAM, NUM_CHANNELS),
		SCAN_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(IN1_INPUT, NUM_CHANNELS),
		ENUMS(FREQ1_INPUT, NUM_CHANNELS),
		ENUMS(FM_RES1_INPUT,		 NUM_CHANNELS),
		SCAN_IN_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(OUT1_OUTPUT, NUM_CHANNELS),
		SCAN_OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		SCAN_LIGHT,
		LIGHTS_LEN
	};

	enum FilterMode {
		LP,
		HP,
		BP
	};

	ripples::RipplesEngine engines[NUM_CHANNELS];


	Atlas() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		for (int i = 0; i < NUM_CHANNELS; i++) {
			configParam(FREQ1_PARAM + i, std::log2(ripples::kFreqKnobMin), std::log2(ripples::kFreqKnobMax), std::log2(ripples::kFreqKnobMax), string::f("Ch. %d frequency", i), " Hz", 2.f);
			configParam(RES1_PARAM + i, 0.f, 1.f, 0.f, string::f("Ch. %d Resonance", i + 1));
			configSwitch(FM_RES_1_PARAM + i, 0.f, 1.f, 0.f, string::f("Ch. %d CV Dest.", i + 1), {"Resonance", "FM"});
			configSwitch(MODE1_PARAM + i, 0.f, 2.f, 0.f, string::f("Ch. %d Filter Mode", i + 1), {"LP (4-pole)", "HP (2-pole)", "BP (4-pole)"});
			configInput(IN1_INPUT + i, string::f("Ch. %d", i + 1));
			configInput(FREQ1_INPUT + i, string::f("Ch. %d Freq", i + 1));
			configInput(FM_RES1_INPUT + i, string::f("Ch. %d FM2/Res", i + 1));
			configOutput(OUT1_OUTPUT + i, string::f("Ch. %d", i + 1));
		}

		configParam(SCAN_PARAM, 0.f, 1.f, 0.f, "Scan");
		configOutput(SCAN_OUT_OUTPUT, "Scan");
	}


	void onReset(const ResetEvent& e) override {
		reset(APP->engine->getSampleRate());
		Module::onReset(e);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		reset(e.sampleRate);
	}

	void reset(float sampleRate) {
		for (int c = 0; c < NUM_CHANNELS; c++) {
			engines[c].setSampleRate(sampleRate);
		}
	}

	void process(const ProcessArgs& args) override {

		float normalInput = 0.f;
		for (int i = 0; i < NUM_CHANNELS; i++) {

			// Reuse the same frame object for multiple engines because the params aren't touched.
			ripples::RipplesEngine::Frame frame;
			frame.res_knob = params[RES1_PARAM + i].getValue();
			frame.freq_knob = rescale(params[FREQ1_PARAM + i].getValue(), std::log2(ripples::kFreqKnobMin), std::log2(ripples::kFreqKnobMax), 0.f, 1.f);
			frame.fm_knob = 0.; //params[FM_PARAM].getValue();
			frame.gain_cv_present = false; //inputs[GAIN_INPUT].isConnected();

			frame.res_cv = 0.f; //inputs[RES_INPUT].getPolyVoltage(c);
			frame.freq_cv = 0.f; //inputs[FREQ_INPUT].getPolyVoltage(c);
			frame.fm_cv = 0.f; //inputs[FM_INPUT].getPolyVoltage(c);

			frame.input = inputs[IN1_INPUT + i].isConnected() ? inputs[IN1_INPUT + i].getVoltageSum() : normalInput;
			normalInput = frame.input;
			//frame.gain_cv = inputs[GAIN_INPUT].getPolyVoltage(c);

			engines[i].process(frame);

			const FilterMode mode = static_cast<FilterMode>(params[MODE1_PARAM + i].getValue());
			float output = mode == LP ? frame.lp4 : (mode == BP ? frame.bp4 : frame.hp2);
			outputs[OUT1_OUTPUT + i].setVoltage(output);
		}
	}
};


struct AtlasWidget : ModuleWidget {
	AtlasWidget(Atlas* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Atlas.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const float first_y = 13.918;
		const float last_y = 84.197;
		const float step_y = (last_y - first_y) / (Atlas::NUM_CHANNELS - 1);
		for (int i = 0; i < Atlas::NUM_CHANNELS; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(51.201, 15.303 + i * step_y)), module, Atlas::FREQ1_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(68.872, 15.303 + i * step_y)), module, Atlas::RES1_PARAM + i));
			addParam(createParam<CKSSHoriz3>(mm2px(Vec(18.52, 24.418 + i * step_y)), module, Atlas::MODE1_PARAM + i));
			addParam(createParam<CKSSNarrow>(mm2px(Vec(58.254, 21.444 + i * step_y)), module, Atlas::FM_RES_1_PARAM + i));

			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.204, 15.297 + i * step_y)), module, Atlas::IN1_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.426, 15.297 + i * step_y)), module, Atlas::FREQ1_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(26.649, 15.297 + i * step_y)), module, Atlas::FM_RES1_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(35.872, 15.297 + i * step_y)), module, Atlas::OUT1_OUTPUT + i));

		}

		addParam(createLightParam<VostokLightSliderHoriz<YellowLight>>(mm2px(Vec(46.752, 111.224)), module, Atlas::SCAN_PARAM, Atlas::SCAN_LIGHT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(37.914, 112.704)), module, Atlas::SCAN_OUT_OUTPUT));
	}
};


Model* modelAtlas = createModel<Atlas, AtlasWidget>("Atlas");