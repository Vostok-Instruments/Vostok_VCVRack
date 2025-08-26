#include "plugin.hpp"
#include "ChowDSP.hpp"


using simd::float_4;
using simd::Vector;



// references:
// * "REDUCING THE ALIASING OF NONLINEAR WAVESHAPING USING CONTINUOUS-TIME CONVOLUTION" (https://www.dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf)
// * "Antiderivative Antialiasing for Memoryless Nonlinearities" https://acris.aalto.fi/ws/portalfiles/portal/27135145/ELEC_bilbao_et_al_antiderivative_antialiasing_IEEESPL.pdf
// * https://ccrma.stanford.edu/~jatin/Notebooks/adaa.html
// * Sena waveshape https://www.desmos.com/calculator/238408c86f

class FoldStage1 {
public:

	float process(float x, float t, bool adaa = true) {
		if (adaa) {
			float y = (std::fabs(x - xPrev) < 1e-5) ? f(0.5 * (xPrev + x), t) : (F(x, t) - F(xPrev, t)) / (x - xPrev);
			xPrev = x;
			return y;
		}
		else {
			return f(x, t);
		}
	}

	// xt - threshold x
	static float f(float x, float t) {
		return ((t < x) ? (-m * x + t * (m + 1)) : ((t < -x) ? (-m * x - t * (m + 1)) : (x)));
	}

	static float F(float x, float t) {
		if (t < -x) {
			return (0.5 * x * (-m * x - 2 * t * (m + 1)));
		}
		else if (t >= x) {
			return (0.5 * (m + 1) * t * t + 0.5 * x * x);
		}
		else {
			return 0.5 * x * (-m * x + 2 * t * (m + 1));
		}
	}

	void reset() {
		xPrev = 0.f;
	}

private:
	float xPrev = 0.f;
	const static int m = 9; 	// downward slope after hitting threshold t
};


class FoldStage2 {
public:
	float process(float x, bool adaa = true) {
		if (adaa) {
			const float y = (std::abs(x - xPrev) < 1e-5) ? f(0.5 * (xPrev + x)) : (F(x) - F(xPrev)) / (x - xPrev);
			xPrev = x;
			return y;
		}
		else {
			return f(x);
		}
	}

	static float f(float x) {
		return (-d * (x + 1) - 1 > c) ? c : ((x < -1) ? -d * x - (d + 1) : ((x < 1) ? x : ((d + 1) - d * x > -c) ? d + 1 - d* x : -c));
	}

	static float F(float x) {
		return (x > 0) ? F_signed(x) : F_signed(-x);
	}

	static float F_signed(float x) {
		const float x_switch = (d + 1 + c) / d;
		if (x < 1) {
			return x * x * 0.5f;
		}
		else if (x < x_switch) {
			return -0.5f * d * x * x + (d + 1) * x - 0.5f * (d + 1);
		}
		else {
			return -c * x + (1 + c * c + d + 2 * c * (1 + d)) / (2.f * d);
		}
		//return (x < 1, x * x * 0.5, simd::ifelse(x < 2.f + c, 2.f * x * (1.f - x * 0.25f) - 1.f,
		//                    2.f * (2.f + c) * (1.f - (2.f + c) * 0.25f) - 1.f - c * (x - 2.f - c)));
	}

	void reset() {
		xPrev = 0.f;
	}

private:
	float xPrev = 0.f;
	static constexpr float c = 0.1f;  	// final value of the fold function (independent of x))
	static constexpr float d = 1.5f; 	// slope of downward part before hitting constant c
};




class HardClip {

public:
	float process(float x, bool adaa = true) {
		if (adaa) {
			const float y = (std::abs(x - xPrev) < 1e-5) ? f(0.5f * (xPrev + x)) : (F(x) - F(xPrev)) / (x - xPrev);
			xPrev = x;
			return y;
		}
		else {
			return f(x);
		}
	}

	static int sgn(float val) {
		return (0.f < val) - (val < 0.f);
	}

	// hard clip function
	static float f(float x) {
		return std::abs(x) > 1.f ? sgn(x) : x;
	}

	// integral of the hard clip function
	static float F(float x) {
		return (std::abs(x) > 1.f) ? x * sgn(x) - 0.5f : x * x * 0.5f;
	}

	void reset() {
		xPrev = 0.f;
	}
private:
	float xPrev = 0.f; 	// previous input value
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

// taken from VCV Fundamental VCO.cpp (gpl-3.0-or-later)
float analogSine(float phase) {
	// Quadratic approximation of sine, slightly richer harmonics
	bool halfPhase = (phase < 0.5f);
	float x = phase - (halfPhase ? 0.25f : 0.75f);
	float v = 1.f - 16.f * x * x;
	return v * (halfPhase ? 1.f : -1.f);
}

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
	enum RangeMode {
		LFO,
		VCO,
	};
	enum TuneMode {
		COARSE,
		FINE,
	};

	// uses a 2*6=12th order Butterworth filter
	chowdsp::VariableOversampling<6, float_4> oversamplerFM;
	chowdsp::VariableOversampling<6, float_4> oversamplerMode;
	chowdsp::VariableOversampling<6, float_4> oversamplerOutput;
	int oversamplingIndex = 1; 	// default is 2^oversamplingIndex == x2 oversampling
	bool useAdaa = true; // default is to use antiderivative antialiasing
	dsp::ClockDivider lightDivider;
	bool removePulseDC = true;

	FoldStage1 stage1;
	FoldStage2 stage2;
	HardClip hardClip;

	// noise parts
	PinkNoiseGenerator pinkNoiseGenerator;
	float lastBrown = 0.f;
	float lastPink = 0.f;
	PinkNoiseGenerator pinkNoiseGenerator2;
	chowdsp::BiquadFilter noiseDcBlockFilter;
	chowdsp::TBiquadFilter<float_4> fmDcBlockFilter;

	const std::array<std::string, 4> modeNames = { "Fold", "Shape", "Phase", "PWM" };
	const std::array<std::string, 4> channelNames = { "Sine", "Triangle", "Saw", "Square" };

	// VCO mode ranges
	constexpr static float kVcoFreqKnobMinCoarse = 15.f; // min frequency knob value
	constexpr static float kVcoFreqKnobMaxCoarse = 1000.f; // max frequency knob value
	const float defaultFreqCoarse = rescale(std::log2(dsp::FREQ_C4), std::log2(kVcoFreqKnobMinCoarse), std::log2(kVcoFreqKnobMaxCoarse), 0.f, 1.f);

	constexpr static float FREQ_C0 = 16.352; // C0 frequency
	constexpr static float kVcoFreqKnobMinFine = FREQ_C0 - 1.f / 12.f; 	// min frequency knob value
	constexpr static float kVcoFreqKnobMaxFine = FREQ_C0 + 1.f / 12.f; 	// max frequency knob value

	// LFO mode ranges
	constexpr static float kLfoFreqKnobMinCoarse = 0.15f; // max frequency knob value
	constexpr static float kLfoFreqKnobMaxCoarse = 10.f; // min frequency knob value

	constexpr static float kLfoFreqKnobMinFine = 0.15f; // max frequency knob value
	constexpr static float kLfoFreqKnobMaxFine = 0.17f; // min frequency knob value


	static constexpr std::pair<float, float> getMinMaxRange(RangeMode mode, TuneMode tuneMode) {
		if (mode == VCO) {
			if (tuneMode == COARSE) {
				return { kVcoFreqKnobMinCoarse, kVcoFreqKnobMaxCoarse };
			}
			else {
				return { kVcoFreqKnobMinFine, kVcoFreqKnobMaxFine };
			}
		}
		else {
			if (tuneMode == COARSE) {
				return { kLfoFreqKnobMinCoarse, kLfoFreqKnobMaxCoarse };
			}
			else {
				return { kLfoFreqKnobMinFine, kLfoFreqKnobMaxFine };
			}
		}
	}

	struct FreqParamQuantity : public ParamQuantity {
		RangeMode rangeMode = VCO; // default mode is VCO
		TuneMode tuneMode = COARSE; // default tuning mode is coarse

		float getDisplayValue() override {

			auto [minFreq, maxFreq] = getMinMaxRange(rangeMode, tuneMode);
			float frequenciesScaled = rescale(getValue(), 0.f, 1.f, std::log2(minFreq), std::log2(maxFreq));
			return std::pow(2.f, frequenciesScaled);
		}

		void setDisplayValue(float displayValue) override {
			float frequenciesScaled = std::log2(displayValue);
			auto [minFreq, maxFreq] = getMinMaxRange(rangeMode, tuneMode);
			setValue(rescale(frequenciesScaled, std::log2(minFreq), std::log2(maxFreq), 0.f, 1.f));
		}
	};

	FreqParamQuantity* freqParams[NUM_CHANNELS];

	Sena() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		for (int i = 0; i < NUM_CHANNELS; ++i) {
			freqParams[i] = configParam<FreqParamQuantity>(FREQ1_PARAM + i, 0.f, 1.f, defaultFreqCoarse, "Frequency", "Hz");
			configParam(MOD1_PARAM + i, 0.f, 1.f, 0.f, modeNames[i]);
			configSwitch(VCO_LFO_MODE1_PARAM + i, 0.f, 1.f, 1.f, "Rate Mode", {"LFO", "VCO"});
			configSwitch(VOCT_FM1_PARAM + i, 0.f, 1.f, 0.f, "FM Type", {"V/OCT", "FM"});
			configSwitch(FINE1_PARAM + i, 0.f, 1.f, 0.f, "Tuning", {"Coarse", "Fine"});
			configInput(VOCT1_INPUT + i, string::f("%s v/oct", channelNames[i].c_str()));
			configInput(MOD1_INPUT + i, string::f("%s CV", modeNames[i].c_str()));
			configOutput(OUT1_OUTPUT + i, string::f("%s", channelNames[i].c_str()));
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

		noiseDcBlockFilter.setParameters(chowdsp::BiquadFilter::HIGHPASS, (80. / sampleRate), 0.707, 1.0f);
		noiseDcBlockFilter.reset();

		fmDcBlockFilter.setParameters(chowdsp::TBiquadFilter<float_4>::HIGHPASS, (22.05 / sampleRate), 0.707, 1.0f);
		fmDcBlockFilter.reset();

		// reset the oversampling buffer pointers
		osBufferOutput = oversamplerOutput.getOSBuffer();
		osBufferMod = oversamplerMode.getOSBuffer();
		osBufferFM = oversamplerFM.getOSBuffer();

		stage1.reset();
		stage2.reset();
		hardClip.reset();
	}

	// implementation taken from "Alias-Suppressed Oscillators Based on Differentiated Polynomial Waveforms",
	// also the notes from Surge Synthesier repo:
	// https://github.com/surge-synthesizer/surge/blob/09f1ec8e103265bef6fc0d8a0fc188238197bf8c/src/common/dsp/oscillators/ModernOscillator.cpp#L19

	float_4 phase = {}; 	// phase at current (sub)sample
	float_4* osBufferFM, * osBufferMod, * osBufferOutput;

	float_4 frequencyPotsPitch = {}, isLinearFm = {}, frequencyMins = {}, frequencyMaxes = {};

	float aliasSuppressedTri(float_4* phases, Waveform waveform) {
		float triBuffer[3];
		for (int i = 0; i < 3; ++i) {
			float p = 2 * phases[i][waveform] - 1.0; 				// range -1.0 to +1.0
			float s = 0.5 - std::abs(p); 				// eq 30
			triBuffer[i] = (s * s * s - 0.75 * s) / 3.0; 	// eq 29
		}
		return (triBuffer[0] - 2.0 * triBuffer[1] + triBuffer[2]);
	}

	float aliasSuppressedSaw(float_4* phases, Waveform waveform) {
		float sawBuffer[3];
		for (int i = 0; i < 3; ++i) {
			float p = 2 * phases[i][waveform] - 1.0; 		// range -1 to +1
			sawBuffer[i] = (p * p * p - p) / 6.0;	// eq 11
		}

		return (sawBuffer[0] - 2.0 * sawBuffer[1] + sawBuffer[2]);
	}

	float aliasSuppressedOffsetSaw(float_4* phases, float pw, Waveform waveform) {
		float sawOffsetBuff[3];

		for (int i = 0; i < 3; ++i) {
			float p = 2 * phases[i][waveform] - 1.0; 		// range -1 to +1
			float pwp = p + 2 * pw;					// phase after pw (pw in [0, 1])
			pwp += (pwp > 1) ? -2 : 0;     			// modulo on [-1, +1]
			sawOffsetBuff[i] = (pwp * pwp * pwp - pwp) / 6.0;	// eq 11
		}
		return (sawOffsetBuff[0] - 2.0 * sawOffsetBuff[1] + sawOffsetBuff[2]);
	}

	void process(const ProcessArgs& args) override {

		const int oversamplingRatio = oversamplerFM.getOversamplingRatio();
		const bool doUpdate = lightDivider.process();

		if (doUpdate) {
			// update modulation and frequency pots (infrequently)
			setupSlowSimdBuffers();
		}
		// upsample incoming CV inputs
		upsampleCVInputs();

		for (int i = 0; i < oversamplingRatio; ++i) {

			const float_4 deltaBasePhase = simd::clamp(osBufferFM[i] * args.sampleTime / oversamplingRatio, 1e-7, 0.5f);
			// floating point arithmetic doesn't work well at low frequencies, specifically because the finite difference denominator
			// becomes tiny - we check for that scenario and use naive / 1st order waveforms in that frequency regime (as aliasing isn't
			// a problem there). With no oversampling, at 44100Hz, the threshold frequency is 44.1Hz.
			const float_4 lowFreqRegime = simd::abs(deltaBasePhase) < 1e-3;

			// 1 / denominator for the second-order FD
			const float_4 denominatorInv = 0.25 / (deltaBasePhase * deltaBasePhase);

			phase += deltaBasePhase;
			// ensure within [0, 1]
			phase -= simd::floor(phase);

			// phase as extrapolated to the current and two previous samples
			float_4 phases[3];
			phases[0] = phase - 2 * deltaBasePhase + simd::ifelse(phase < 2 * deltaBasePhase, 1.f, 0.f);
			phases[1] = phase - deltaBasePhase + simd::ifelse(phase < deltaBasePhase, 1.f, 0.f);
			phases[2] = phase;

			// sine
			{
				float foldAmount = 1.f - 0.5f * osBufferMod[i][SINE]; // fold amount for sine wave
				float sine = analogSine(phase[0]);
				// only bother using antiderivative antialiasing for sine if the output is connected
				bool useAdaaForSin = useAdaa && outputs[OUT1_OUTPUT + SINE].isConnected();
				osBufferOutput[i][SINE] = stage2.process(stage1.process(sine, foldAmount, useAdaaForSin), useAdaaForSin);
			}

			// triangle
			{
				if (lowFreqRegime[TRIANGLE] || !useAdaa || !outputs[OUT1_OUTPUT + TRIANGLE].isConnected()) {
					osBufferOutput[i][TRIANGLE] = 1.0 - 2.0 * std::abs(2 * phase[TRIANGLE] - 1.0);
				}
				else {
					osBufferOutput[i][TRIANGLE] = aliasSuppressedTri(phases, TRIANGLE) * denominatorInv[TRIANGLE];
				}

				const float scale = 1 + 1.5 * osBufferMod[i][TRIANGLE];

				osBufferOutput[i][TRIANGLE] = hardClip.process(scale * osBufferOutput[i][TRIANGLE], useAdaa);
			}

			// saw
			{
				float offsetPhase = phase[SAW] - osBufferMod[i][SAW]; // sawtooth phase offset
				offsetPhase -= std::floor(offsetPhase); // ensure within [0, 1]

				// use cheap version for low frequencies, or when ADAA is disabled, or if only used for LEDs
				if (lowFreqRegime[SAW] || !useAdaa || !outputs[OUT1_OUTPUT + SAW].isConnected()) {
					float saw1 = 2.f * phase[SAW] - 1.f;
					float saw2 = 2.f * offsetPhase - 1.f;
					osBufferOutput[i][SAW] = (saw1 - 0.1 * saw2);
				}
				else {
					float saw1 = aliasSuppressedSaw(phases, SAW);
					float saw2 = aliasSuppressedOffsetSaw(phases, 1 - osBufferMod[i][SAW], SAW);
					osBufferOutput[i][SAW] = (saw1 - 0.1 * saw2) * denominatorInv[SAW];
				}
			}

			// square
			{
				const float pulseWidth = 0.5f - osBufferMod[i][SQUARE] * 0.45f; // pulse width modulation
				const float pulseDCOffset = (!removePulseDC) * 2.f * (0.5f - pulseWidth);

				// use cheap version for low frequencies, or when ADAA is disabled, or if only used for LEDs
				if (lowFreqRegime[SQUARE] || !useAdaa || !outputs[OUT1_OUTPUT + SQUARE].isConnected()) {
					// simple square wave
					float square = (phase[SQUARE] < 1 - pulseWidth) ? +1.f : -1.f;
					osBufferOutput[i][SQUARE] = square;
				}
				else {
					// alias-suppressed square wave (DPW order 3)
					float saw = aliasSuppressedSaw(phases, SQUARE);
					float sawOffset = aliasSuppressedOffsetSaw(phases, pulseWidth, SQUARE);
					float dpwOrder3 = (sawOffset - saw) * denominatorInv[SQUARE] + pulseDCOffset;
					osBufferOutput[i][SQUARE] = dpwOrder3;
				}
			}
		}

		// outputs and lights
		{
			float_4 isLfo = float_4(
			                  params[VCO_LFO_MODE1_PARAM + SINE].getValue(),
			                  params[VCO_LFO_MODE1_PARAM + TRIANGLE].getValue(),
			                  params[VCO_LFO_MODE1_PARAM + SAW].getValue(),
			                  params[VCO_LFO_MODE1_PARAM + SQUARE].getValue()
			                ) < 0.5f;

			float_4 out = 5.f * ((oversamplingRatio > 1) ? oversamplerOutput.downsample() : osBufferOutput[0]);
			// if LFO, use the first oversampling sample to avoid bandliming artifacts in the LFO range
			out = simd::ifelse(isLfo, 5 * osBufferOutput[0], out);

			outputs[OUT1_OUTPUT + SINE].setVoltage(out[SINE]);
			outputs[OUT1_OUTPUT + TRIANGLE].setVoltage(out[TRIANGLE]);
			outputs[OUT1_OUTPUT + SAW].setVoltage(out[SAW]);
			outputs[OUT1_OUTPUT + SQUARE].setVoltage(out[SQUARE]);

			if (doUpdate) {
				const float sampleTimeLights = args.sampleTime * lightUpdateRate;
				// update lights
				for (int i = 0; i < NUM_CHANNELS; ++i) {
					if (osBufferFM[0][i] > 35.) {
						// above 35Hz, just leave LED on
						lights[NUM1_LIGHT + i].setBrightness(1.0);
					}
					else {
						lights[NUM1_LIGHT + i].setBrightnessSmooth(std::max(out[i] / 5.f, 0.f), sampleTimeLights, lambda);
					}
				}
			}
		}

		// noise outputs
		processNoise();
	}

	void processNoise() {
		if (outputs[WHITE_OUTPUT].isConnected()) {
			float white = random::normal();
			outputs[WHITE_OUTPUT].setVoltage(white);
		}

		if (outputs[PINK_OUTPUT].isConnected() || outputs[BLUE_OUTPUT].isConnected()) {
			// Pink noise: -3dB/oct
			float white = random::normal();

			float pink = pinkNoiseGenerator.process(white);
			outputs[PINK_OUTPUT].setVoltage(pink * 0.125);

			// Blue noise: 3dB/oct
			if (outputs[BLUE_OUTPUT].isConnected()) {
				// apply a +6dB/oct filter to the pink noise (which is -3dB/oct) to get a +3dB/oct blue noise
				float blue = (pink - lastPink) * 0.25f * (APP->engine->getSampleRate() / 44100.f);
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
			brown = noiseDcBlockFilter.process(brown);
			outputs[BROWN_OUTPUT].setVoltage(brown * 0.1f);
		}
	}

	TuneMode tuneModes[NUM_CHANNELS] = { COARSE, COARSE, COARSE, COARSE }; // default tuning mode is coarse
	void setupSlowSimdBuffers() {

		// work out which channels use linear FM
		isLinearFm = float_4(
		               params[VOCT_FM1_PARAM + SINE].getValue(),
		               params[VOCT_FM1_PARAM + TRIANGLE].getValue(),
		               params[VOCT_FM1_PARAM + SAW].getValue(),
		               params[VOCT_FM1_PARAM + SQUARE].getValue()
		             ) > 0.5f;

		// setup the frequency ranges for each channel
		for (int i = 0; i < NUM_CHANNELS; ++i) {
			RangeMode rangeMode = static_cast<RangeMode>(params[VCO_LFO_MODE1_PARAM + i].getValue());
			TuneMode tuneMode = static_cast<TuneMode>(params[FINE1_PARAM + i].getValue());

			// if we've switched into fine tuning mode, reset the frequency pot to the middle of the fine range (C0)
			if (tuneMode != tuneModes[i] && tuneMode == FINE) {
				params[FREQ1_PARAM + i].setValue(0.5f); // reset to middle of fine range
			}
			// remember the current tuning mode
			tuneModes[i] = tuneMode;

			// also update UI label details whilst we're here
			freqParams[i]->rangeMode = rangeMode;
			freqParams[i]->tuneMode = tuneMode;

			auto [minFreq, maxFreq] = getMinMaxRange(rangeMode, tuneMode);
			frequencyMins[i] = minFreq;
			frequencyMaxes[i] = maxFreq;

			getParamQuantity(FREQ1_PARAM + i)->defaultValue = (tuneMode == COARSE) ? defaultFreqCoarse : 0.5f;
		}

		const float_4 frequencyPots = float_4(
		                                params[FREQ1_PARAM + SINE].getValue(),
		                                params[FREQ1_PARAM + TRIANGLE].getValue(),
		                                params[FREQ1_PARAM + SAW].getValue(),
		                                params[FREQ1_PARAM + SQUARE].getValue()
		                              );

		frequencyPotsPitch = simd::rescale(frequencyPots, 0.f, 1.f, simd::log2(frequencyMins), simd::log2(frequencyMaxes));
	}

	void upsampleCVInputs() {
		const int oversamplingRatio = oversamplerFM.getOversamplingRatio();

		// upsample FM inputs (if any are connected), performance is the same whether it's 1 channel or 4 because of simd
		if (inputs[VOCT1_INPUT + SINE].isConnected() ||
		    inputs[VOCT1_INPUT + TRIANGLE].isConnected() ||
		    inputs[VOCT1_INPUT + SAW].isConnected() ||
		    inputs[VOCT1_INPUT + SQUARE].isConnected()) {

			float_4 fmInputs;

			float normalVoltage = 0.f;
			for (int i = 0; i < NUM_CHANNELS; ++i) {
				fmInputs[i] = inputs[VOCT1_INPUT + i].getNormalVoltage(normalVoltage);
				normalVoltage = fmInputs[i];
			}

			// in linear FM mode, we are AC coupled
			fmInputs = simd::ifelse(isLinearFm, fmDcBlockFilter.process(fmInputs), fmInputs);

			// pitch is v/oct (if mode is selected) + frequency pot value
			const float_4 pitch = simd::ifelse(isLinearFm, float_4::zero(), fmInputs) + frequencyPotsPitch;
			// convert to frequency in Hz
			const float_4 freq = simd::pow(2.f, pitch) + 120 * simd::ifelse(isLinearFm, fmInputs, float_4::zero());

			oversamplerFM.upsample(freq);
		}
		else {
			// if no CVs are connected, just use the frequency pots
			std::fill(osBufferFM, &osBufferFM[oversamplingRatio], simd::pow(2.f, frequencyPotsPitch));
		}


		// get pot values for the mode inputs

		// upsample mode inputs (if any are connected)
		if (inputs[MOD1_INPUT + SINE].isConnected() ||
		    inputs[MOD1_INPUT + TRIANGLE].isConnected() ||
		    inputs[MOD1_INPUT + SAW].isConnected() ||
		    inputs[MOD1_INPUT + SQUARE].isConnected()) {

			float_4 modOffsets = float_4(
			                       params[MOD1_PARAM + SINE].getValue(),
			                       params[MOD1_PARAM + TRIANGLE].getValue(),
			                       params[MOD1_PARAM + SAW].getValue(),
			                       0.f);
			float_4 modScales = float_4(1.f, 1.f, 1.f, params[MOD1_PARAM + SQUARE].getValue());


			float_4 modInputs = float_4(
			                      inputs[MOD1_INPUT + SINE].getVoltage(),
			                      inputs[MOD1_INPUT + TRIANGLE].getVoltage(),
			                      inputs[MOD1_INPUT + SAW].getVoltage(),
			                      inputs[MOD1_INPUT + SQUARE].getVoltage()
			                    );

			// combination of pot and CV controls the mods in range [0, 1]
			modInputs = simd::clamp(simd::clamp(modInputs / 10.f, -1.f, 1.f) * modScales + modOffsets, 0.f, 1.f);
			oversamplerMode.upsample(modInputs);
		}
		else {
			float_4 modPots = float_4(
			                    params[MOD1_PARAM + SINE].getValue(),
			                    params[MOD1_PARAM + TRIANGLE].getValue(),
			                    params[MOD1_PARAM + SAW].getValue(),
			                    params[MOD1_PARAM + SQUARE].getValue());

			std::fill(osBufferMod, &osBufferMod[oversamplingRatio], modPots);
		}

		// zero outputs (in case they are used in main loop)
		std::fill(osBufferOutput, &osBufferOutput[oversamplingRatio], float_4::zero());
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "oversamplingIndex", json_integer(oversamplingIndex));
		json_object_set_new(rootJ, "useAdaa", json_boolean(useAdaa));
		json_object_set_new(rootJ, "removePulseDC", json_boolean(removePulseDC));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* jOversamplingIndex = json_object_get(rootJ, "oversamplingIndex");
		if (jOversamplingIndex) {
			oversamplingIndex = json_integer_value(jOversamplingIndex);
			onSampleRateChange();
		}

		json_t* jUseAdaa = json_object_get(rootJ, "useAdaa");
		if (jUseAdaa) {
			useAdaa = json_boolean_value(jUseAdaa);
		}

		json_t* jRemovePulseDC = json_object_get(rootJ, "removePulseDC");
		if (jRemovePulseDC) {
			removePulseDC = json_boolean_value(jRemovePulseDC);
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

		menu->addChild(createSubmenuItem("Anti-aliasing", "",
		[ = ](Menu * menu) {

			menu->addChild(createIndexSubmenuItem("Oversampling",
			{"Off", "x2", "x4", "x8"},
			[ = ]() {
				return module->oversamplingIndex;
			},
			[ = ](int mode) {
				module->oversamplingIndex = mode;
				module->onSampleRateChange();
			}));

			menu->addChild(createBoolPtrMenuItem("Use ADAA", "", &module->useAdaa));
		}));

		menu->addChild(createBoolPtrMenuItem("Remove Pulse DC Offset", "", &module->removePulseDC));
	}
};


Model* modelSena = createModel<Sena, SenaWidget>("Sena");