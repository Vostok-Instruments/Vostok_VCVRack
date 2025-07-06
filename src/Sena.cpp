#include "plugin.hpp"
#include "ChowDSP.hpp"


using simd::float_4;
using simd::Vector;


// references:
// * "REDUCING THE ALIASING OF NONLINEAR WAVESHAPING USING CONTINUOUS-TIME CONVOLUTION" (https://www.dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf)
// * "Antiderivative Antialiasing for Memoryless Nonlinearities" https://acris.aalto.fi/ws/portalfiles/portal/27135145/ELEC_bilbao_et_al_antiderivative_antialiasing_IEEESPL.pdf
// * https://ccrma.stanford.edu/~jatin/Notebooks/adaa.html
// * Pony waveshape  https://www.desmos.com/calculator/1kvahyl4ti

template<typename T>
class FoldStage1 {
public:

	T process(T x, T xt) {
		T y = simd::ifelse(simd::abs(x - xPrev) < 1e-5,
		                   f(0.5 * (xPrev + x), xt),
		                   (F(x, xt) - F(xPrev, xt)) / (x - xPrev));

		xPrev = x;
		return y;
	}

	// xt - threshold x
	static T f(T x, T xt) {
		return simd::ifelse(x > xt, +5 * xt - 4 * x, simd::ifelse(x < -xt, -5 * xt - 4 * x, x));
	}

	static T F(T x, T xt) {
		return simd::ifelse(x > xt,  5 * xt * x - 2 * x * x - 2.5 * xt * xt,
		                    simd::ifelse(x < -xt, -5 * xt * x - 2 * x * x - 2.5 * xt * xt, x * x / 2.f));
	}

	void reset() {
		xPrev = 0.f;
	}

private:
	T xPrev = 0.f;
};

template<typename T>
class FoldStage2 {
public:
	T process(T x) {
		const T y = simd::ifelse(simd::abs(x - xPrev) < 1e-5, f(0.5 * (xPrev + x)), (F(x) - F(xPrev)) / (x - xPrev));
		xPrev = x;
		return y;
	}

	static T f(T x) {
		return simd::ifelse(-(x + 2) > c, c, simd::ifelse(x < -1, -(x + 2), simd::ifelse(x < 1, x, simd::ifelse(-x + 2 > -c, -x + 2, -c))));
	}

	static T F(T x) {
		return simd::ifelse(x > 0, F_signed(x), F_signed(-x));
	}

	static T F_signed(T x) {
		return simd::ifelse(x < 1, x * x * 0.5, simd::ifelse(x < 2.f + c, 2.f * x * (1.f - x * 0.25f) - 1.f,
		                    2.f * (2.f + c) * (1.f - (2.f + c) * 0.25f) - 1.f - c * (x - 2.f - c)));
	}

	void reset() {
		xPrev = 0.f;
	}

private:
	T xPrev = 0.f;
	static constexpr float c = 0.1f;
};


/** Based on pke Paul Kellet's economy method.
http://www.firstpr.com.au/dsp/pink-noise/
*/
struct PinkNoiseGenerator {

	float b0 = 0.f, b1 = 0.f, b2 = 0.f;

	float process(float white) {
		b0 = 0.99765 * b0 + white * 0.0990460;
		b1 = 0.96300 * b1 + white * 0.2965164;
		b2 = 0.57000 * b2 + white * 1.0526913;
		return b0 + b1 + b2 + white * 0.1848;
	}
};



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
	enum Waveform {
		SINE,
		TRIANGLE,
		SAW,
		SQUARE
	};
	enum Mode {
		LFO,
		VCO,
	};

	// uses a 2*6=12th order Butterworth filter
	chowdsp::VariableOversampling<6, float_4> oversamplerFM;
	chowdsp::VariableOversampling<6, float_4> oversamplerMode;
	chowdsp::VariableOversampling<6, float_4> oversamplerOutput;
	int oversamplingIndex = 1; 	// default is 2^oversamplingIndex == x2 oversampling
	dsp::ClockDivider lightDivider;
	const int lightUpdateRate = 16;

	const std::string modeNames[4] = { "Fold", "Shape", "Phase", "PWM" };
	const std::string channelNames[4] = { "Sine", "Triangle", "Saw", "Square" };

	// VCO mode ranges
	constexpr static float kVcoFreqKnobMin = 15.f; // max frequency knob value
	constexpr static float kVcoFreqKnobMax = 1000.f; // min frequency knob value

	// LFO mode ranges
	constexpr static float kLfoFreqKnobMin = 0.15f; // max frequency knob value
	constexpr static float kLfoFreqKnobMax = 10.f; // min frequency knob value

	// noise parts
	PinkNoiseGenerator pinkNoiseGenerator;
	float lastBrown = 0.f;
	float lastPink = 0.f;
	PinkNoiseGenerator pinkNoiseGenerator2;
	chowdsp::BiquadFilter dcBlockFilter;

	struct FreqParamQuantity : public ParamQuantity {
		Mode mode = VCO; // default mode is VCO

		float getDisplayValue() override {

			float frequenciesScaled;
			if (mode == VCO) {
				frequenciesScaled = rescale(getValue(), 0.f, 1.f, std::log2(kVcoFreqKnobMin), std::log2(kVcoFreqKnobMax));
			}
			else {
				frequenciesScaled = rescale(getValue(), 0.f, 1.f, std::log2(kLfoFreqKnobMin), std::log2(kLfoFreqKnobMax));
			}
			return std::pow(2.f, frequenciesScaled);
		}

		void setDisplayValue(float displayValue) override {
			float frequenciesScaled = std::log2(displayValue);
			if (mode == VCO) {
				setValue(rescale(frequenciesScaled, std::log2(kVcoFreqKnobMin), std::log2(kVcoFreqKnobMax), 0.f, 1.f));
			}
			else {
				setValue(rescale(frequenciesScaled, std::log2(kLfoFreqKnobMin), std::log2(kLfoFreqKnobMax), 0.f, 1.f));
			}			
		}
	};

	FreqParamQuantity* freqParams[NUM_CHANNELS];

	Sena() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		const float defaultFreq = rescale(std::log2(dsp::FREQ_C4), std::log2(kVcoFreqKnobMin), std::log2(kVcoFreqKnobMax), 0.f, 1.f);

		for (int i = 0; i < NUM_CHANNELS; ++i) {
			freqParams[i] = configParam<FreqParamQuantity>(FREQ1_PARAM + i, 0.f, 1.f, defaultFreq, "Frequency", "Hz");
			configParam(MOD1_PARAM + i, 0.f, 1.f, 0.f, modeNames[i]);
			configSwitch(VCO_LFO_MODE1_PARAM + i, 0.f, 1.f, 1.f, "Rate Mode", {"LFO", "VCO"});
			configSwitch(VOCT_FM1_PARAM + i, 0.f, 1.f, 0.f, "FM Type", {"V/OCT", "FM"});
			configSwitch(FINE1_PARAM + i, 0.f, 1.f, 0.f, "Tuning", {"Coarse", "Fine"});
			configInput(VOCT1_INPUT + i, string::f("%s v/oct", channelNames[i]));
			configInput(MOD1_INPUT + i, string::f("%s CV", modeNames[i]));
			configOutput(OUT1_OUTPUT + i, string::f("%s", channelNames[i]));
		}

		configOutput(WHITE_OUTPUT, "White Noise");
		configOutput(PINK_OUTPUT, "Pink Noise (-3dB/oct)");
		configOutput(BLUE_OUTPUT, "Blue Noise (+3dB/oct)");
		configOutput(BROWN_OUTPUT, "Brown Noise (-6dB/oct)");

		lightDivider.setDivision(lightUpdateRate);
	}

	void onSampleRateChange() override {
		float sampleRate = APP->engine->getSampleRate();

		oversamplerFM.setOversamplingIndex(oversamplingIndex);
		oversamplerFM.reset(sampleRate);

		oversamplerMode.setOversamplingIndex(oversamplingIndex);
		oversamplerMode.reset(sampleRate);

		oversamplerOutput.setOversamplingIndex(oversamplingIndex);
		oversamplerOutput.reset(sampleRate);

		dcBlockFilter.setParameters(chowdsp::BiquadFilter::HIGHPASS, (80. / sampleRate), 0.707, 1.0f);
		dcBlockFilter.reset();
	}

	// implementation taken from "Alias-Suppressed Oscillators Based on Differentiated Polynomial Waveforms",
	// also the notes from Surge Synthesier repo:
	// https://github.com/surge-synthesizer/surge/blob/09f1ec8e103265bef6fc0d8a0fc188238197bf8c/src/common/dsp/oscillators/ModernOscillator.cpp#L19

	float_4 phase = {}; 	// phase at current (sub)sample
	float_4* osBufferFM, * osBufferMode, * osBufferOutput;
	void process(const ProcessArgs& args) override {

		const int oversamplingRatio = oversamplerFM.getOversamplingRatio();

		const float frequencies[4] = {
			params[FREQ1_PARAM + SINE].getValue(),
			params[FREQ1_PARAM + TRIANGLE].getValue(),
			params[FREQ1_PARAM + SAW].getValue(),
			params[FREQ1_PARAM + SQUARE].getValue()
		};
		const float fmTypeModes_[4] = {
			params[VOCT_FM1_PARAM + SINE].getValue(),
			params[VOCT_FM1_PARAM + TRIANGLE].getValue(),
			params[VOCT_FM1_PARAM + SAW].getValue(),
			params[VOCT_FM1_PARAM + SQUARE].getValue()
		};
		const float_4 fmTypeModes = float_4::load(fmTypeModes_);
		const float lfoVcoModes[4] = {
			params[VCO_LFO_MODE1_PARAM + SINE].getValue(),
			params[VCO_LFO_MODE1_PARAM + TRIANGLE].getValue(),
			params[VCO_LFO_MODE1_PARAM + SAW].getValue(),
			params[VCO_LFO_MODE1_PARAM + SQUARE].getValue()
		};
		const float_4 frequenciesScaledVCO = simd::rescale(float_4::load(frequencies), 0.f, 1.f, std::log2(kVcoFreqKnobMin), std::log2(kVcoFreqKnobMax));
		const float_4 frequenciesScaledLFO = simd::rescale(float_4::load(frequencies), 0.f, 1.f, std::log2(kLfoFreqKnobMin), std::log2(kLfoFreqKnobMax));
		// select frequency scaling based on the mode
		const float_4 freqScaled = simd::ifelse(float_4::load(lfoVcoModes) > 0.5, frequenciesScaledVCO, frequenciesScaledLFO);

		upsampleInputs();
		osBufferOutput = oversamplerOutput.getOSBuffer();
		for (int i = 0; i < oversamplingRatio; ++i) {

			const float_4 fmVoltage = osBufferFM[i];
			const float_4 pitch = simd::ifelse(fmTypeModes < 0.5, fmVoltage, float_4::zero()) + freqScaled;
			const float_4 freq = simd::pow(2.f, pitch) + dsp::FREQ_C4 * simd::ifelse(fmTypeModes > 0.5, fmVoltage, float_4::zero());

			const float_4 deltaBasePhase = simd::clamp(freq * args.sampleTime / oversamplingRatio, 1e-7, 0.5f);
			// floating point arithmetic doesn't work well at low frequencies, specifically because the finite difference denominator
			// becomes tiny - we check for that scenario and use naive / 1st order waveforms in that frequency regime (as aliasing isn't
			// a problem there). With no oversampling, at 44100Hz, the threshold frequency is 44.1Hz.
			const float_4 lowFreqRegime = simd::abs(deltaBasePhase) < 1e-3;

			// 1 / denominator for the second-order FD
			const float_4 denominatorInv = 0.25 / (deltaBasePhase * deltaBasePhase);

			phase += deltaBasePhase;
			// ensure within [0, 1]
			phase -= simd::floor(phase);

			// TODO: proper antiderivative antialiasing
			// sine
			if (outputs[OUT1_OUTPUT + SINE].isConnected()) {
				osBufferOutput[i][SINE] = -std::sin(2.0 * M_PI * phase[0]);
			}

			// triangle
			if (outputs[OUT1_OUTPUT + TRIANGLE].isConnected()) {
				float triangle = 2.f * phase[1] - 1.f;
				triangle = triangle < 0.f ? -triangle : triangle;
				triangle = 2 * triangle - 1.f;

				float scale = 1 + params[MOD1_PARAM + TRIANGLE].getValue();

				osBufferOutput[i][TRIANGLE] = clamp(scale * triangle, -1.f, +1.f);
			}

			// saw
			if (outputs[OUT1_OUTPUT + SAW].isConnected()) {
				float saw = 2.f * phase[2] - 1.f;
				osBufferOutput[i][SAW] = saw;
			}

			// square
			if (outputs[OUT1_OUTPUT + SQUARE].isConnected()) {
				float square = phase[3] < 0.5f ? -1.f : 1.f;
				osBufferOutput[i][SQUARE] = square;
			}
		}

		if (outputs[OUT1_OUTPUT + SINE].isConnected() ||
		    outputs[OUT1_OUTPUT + TRIANGLE].isConnected() ||
		    outputs[OUT1_OUTPUT + SAW].isConnected() ||
		    outputs[OUT1_OUTPUT + SQUARE].isConnected()) {

			float_4 out = 5.f * ((oversamplingRatio > 1) ? oversamplerOutput.downsample() : osBufferOutput[0]);

			outputs[OUT1_OUTPUT + SINE].setVoltage(out[SINE]);
			outputs[OUT1_OUTPUT + TRIANGLE].setVoltage(out[TRIANGLE]);
			outputs[OUT1_OUTPUT + SAW].setVoltage(out[SAW]);
			outputs[OUT1_OUTPUT + SQUARE].setVoltage(out[SQUARE]);
		}

		if (outputs[WHITE_OUTPUT].isConnected()) {
			float white = 0.25 * random::normal();
			outputs[WHITE_OUTPUT].setVoltage(white);
		}

		if (outputs[PINK_OUTPUT].isConnected() || outputs[BLUE_OUTPUT].isConnected()) {
			// Pink noise: -3dB/oct
			float white = 0.25 * random::normal();

			float pink = pinkNoiseGenerator.process(white);
			outputs[PINK_OUTPUT].setVoltage(pink * 0.5);

			// Blue noise: 3dB/oct
			if (outputs[BLUE_OUTPUT].isConnected()) {
				// apply a +6dB/oct filter to the pink noise (which is -3dB/oct) to get a +3dB/oct blue noise
				float blue = (pink - lastPink) * 0.5f;
				lastPink = pink;
				outputs[BLUE_OUTPUT].setVoltage(blue);
			}
		}

		if (outputs[BROWN_OUTPUT].isConnected()) {
			// Brown noise: -6dB/oct
			float white = 0.25 * random::normal();
			// apply a -3dB/oct filter to the white noise to get pink noise, and again to get -6dB/oct brown noise
			float pink = pinkNoiseGenerator.process(white);
			float brown = pinkNoiseGenerator2.process(pink);
			// need to mitigate high energy at DC, so filter here
			brown = dcBlockFilter.process(brown);
			outputs[BROWN_OUTPUT].setVoltage(brown * 0.1f);

		}

		for (int i = 0; i < NUM_CHANNELS; ++i) {
			freqParams[i]->mode = (Mode)params[VCO_LFO_MODE1_PARAM + i].getValue();
		}
	}

	void upsampleInputs() {
		const int oversamplingRatio = oversamplerFM.getOversamplingRatio();

		// upsample FM inputs (if any are connected)
		osBufferFM = oversamplerFM.getOSBuffer();
		if (inputs[VOCT1_INPUT + SINE].isConnected() ||
		    inputs[VOCT1_INPUT + TRIANGLE].isConnected() ||
		    inputs[VOCT1_INPUT + SAW].isConnected() ||
		    inputs[VOCT1_INPUT + SQUARE].isConnected()) {

			const float voctInputs[4] = {
				inputs[VOCT1_INPUT + SINE].getVoltage(),
				inputs[VOCT1_INPUT + TRIANGLE].getVoltage(),
				inputs[VOCT1_INPUT + SAW].getVoltage(),
				inputs[VOCT1_INPUT + SQUARE].getVoltage()
			};
			oversamplerFM.upsample(float_4::load(voctInputs));
		}
		else {
			std::fill(osBufferFM, &osBufferFM[oversamplingRatio], float_4::zero());
		}

		// upsample mode inputs (if any are connected)
		osBufferMode = oversamplerMode.getOSBuffer();
		if (inputs[MOD1_INPUT + SINE].isConnected() ||
		    inputs[MOD1_INPUT + TRIANGLE].isConnected() ||
		    inputs[MOD1_INPUT + SAW].isConnected() ||
		    inputs[MOD1_INPUT + SQUARE].isConnected()) {

			const float modeInputs[4] = {
				inputs[MOD1_INPUT + SINE].getVoltage(),
				inputs[MOD1_INPUT + TRIANGLE].getVoltage(),
				inputs[MOD1_INPUT + SAW].getVoltage(),
				inputs[MOD1_INPUT + SQUARE].getVoltage()
			};

			oversamplerMode.upsample(float_4::load(modeInputs));
		}
		else {
			std::fill(osBufferMode, &osBufferMode[oversamplingRatio], float_4::zero());
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "oversamplingIndex", json_integer(oversamplingIndex));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* jOversamplingIndex = json_object_get(rootJ, "oversamplingIndex");
		if (jOversamplingIndex) {
			oversamplingIndex = json_integer_value(jOversamplingIndex);
			onSampleRateChange();
		}
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
		addChild(createLight<VostokOrangeNumberLed<1>>(mm2px(Vec(30.878, 19.259)), module, Sena::NUM1_LIGHT + 0));
		addChild(createLight<VostokOrangeNumberLed<2>>(mm2px(Vec(30.878, 42.639)), module, Sena::NUM1_LIGHT + 1));
		addChild(createLight<VostokOrangeNumberLed<3>>(mm2px(Vec(30.490, 66.014)), module, Sena::NUM1_LIGHT + 2));
		addChild(createLight<VostokOrangeNumberLed<4>>(mm2px(Vec(30.490, 89.511)), module, Sena::NUM1_LIGHT + 3));
	}

	void appendContextMenu(Menu* menu) override {
		Sena* module = dynamic_cast<Sena*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());

		menu->addChild(createIndexSubmenuItem("Oversampling",
		{"Off", "x2", "x4", "x8"},
		[ = ]() {
			return module->oversamplingIndex;
		},
		[ = ](int mode) {
			module->oversamplingIndex = mode;
			module->onSampleRateChange();
		}
		                                     ));
	}
};


Model* modelSena = createModel<Sena, SenaWidget>("Sena");