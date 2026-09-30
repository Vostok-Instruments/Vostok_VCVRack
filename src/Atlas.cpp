#include "AtlasTptEngine.hpp"
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
        ENUMS(FM_RES1_INPUT, NUM_CHANNELS),
        SCAN_IN_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        ENUMS(OUT1_OUTPUT, NUM_CHANNELS),
        SCAN_OUT_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(NUM1_LIGHT, NUM_CHANNELS),
        LIGHTS_LEN
    };

    enum FilterMode {
        LP,
        HP,
        BP
    };
    enum CVDest {
        RES,
        FM2
    };

    enum FilterEngine {
        ACCURATE_CIRCUIT,
        ECONOMY_TPT
    };

    vostok_ripples::RipplesEngine engines[NUM_CHANNELS][PORT_MAX_CHANNELS];
    AtlasTptEngine4 tptSimdEngines[NUM_CHANNELS][PORT_MAX_CHANNELS / 4];
    dsp::ClockDivider lightDivider;
    bool addLowend = true;
    bool clipOutput = true;
    // New modules use the low-cost engine. dataFromJson() deliberately
    // selects the existing engine for patches saved before this setting was
    // introduced.
    int filterEngine = ECONOMY_TPT;
    int activeFilterEngine = -1;
    int activeVoiceChannels[NUM_CHANNELS]{};
    float currentSampleRate = 1.f;

    Atlas() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        for (int i = 0; i < NUM_CHANNELS; i++) {
            configParam(FREQ1_PARAM + i, std::log2(vostok_ripples::kFreqKnobMin), std::log2(vostok_ripples::kFreqKnobMax),
                        std::log2(dsp::FREQ_C4), string::f("Ch. %d frequency", i + 1), " Hz", 2.f);
            configParam(RES1_PARAM + i, 0.f, 1.f, 0.f, string::f("Ch. %d Resonance", i + 1));
            configSwitch(FM_RES_1_PARAM + i, 0.f, 1.f, 1.f, string::f("Ch. %d CV Dest.", i + 1), {"Resonance", "FM2"});
            configSwitch(MODE1_PARAM + i, 0.f, 2.f, 0.f, string::f("Ch. %d Filter Mode", i + 1),
                         {"LP (4-pole)", "HP (2-pole)", "BP (4-pole)"});
            configInput(IN1_INPUT + i, string::f("Ch. %d", i + 1));
            configInput(FREQ1_INPUT + i, string::f("Ch. %d Freq", i + 1));
            configInput(FM_RES1_INPUT + i, string::f("Ch. %d FM2/Res", i + 1));
            configOutput(OUT1_OUTPUT + i, string::f("Ch. %d", i + 1));
        }

        configParam(SCAN_PARAM, 0.f, 1.f, 0.f, "Scan");
        configInput(SCAN_IN_INPUT, "Scan CV");
        configOutput(SCAN_OUT_OUTPUT, "Scan");

        lightDivider.setDivision(lightUpdateRate);
    }

    void onReset(const ResetEvent &e) override {
        reset(APP->engine->getSampleRate());
        Module::onReset(e);
    }

    void onSampleRateChange(const SampleRateChangeEvent &e) override { reset(e.sampleRate); }

    void reset(float sampleRate) {
        currentSampleRate = sampleRate;
        for (int row = 0; row < NUM_CHANNELS; row++) {
            activeVoiceChannels[row] = 0;
            for (int voice = 0; voice < PORT_MAX_CHANNELS; voice++) {
                engines[row][voice].setSampleRate(sampleRate);
            }
            for (int block = 0; block < PORT_MAX_CHANNELS / 4; block++) {
                tptSimdEngines[row][block].setSampleRate(sampleRate);
            }
        }
    }

    void process(const ProcessArgs &args) override {

        bool resetForSampleRate = false;
        if (currentSampleRate != args.sampleRate) {
            reset(args.sampleRate);
            resetForSampleRate = true;
        }
        filterEngine = clamp(filterEngine, static_cast<int>(ACCURATE_CIRCUIT), static_cast<int>(ECONOMY_TPT));
        if (filterEngine != activeFilterEngine) {
            if (!resetForSampleRate) {
                reset(args.sampleRate);
            }
            activeFilterEngine = filterEngine;
        }

        const bool updateLeds = lightDivider.process();
        float normalInputs[PORT_MAX_CHANNELS]{};
        float normalFrequencies[PORT_MAX_CHANNELS]{};
        float rowOutputs[NUM_CHANNELS][PORT_MAX_CHANNELS]{};
        int rowChannels[NUM_CHANNELS]{};
        int normalInputChannels = 1;
        int normalFrequencyChannels = 1;
        int scanChannels = 1;

        for (int row = 0; row < NUM_CHANNELS; row++) {
            const bool inputConnected = inputs[IN1_INPUT + row].isConnected();
            const bool frequencyConnected = inputs[FREQ1_INPUT + row].isConnected();
            const int inputChannels = inputConnected ? std::max(inputs[IN1_INPUT + row].getChannels(), 1) : normalInputChannels;
            const int frequencyChannels =
                frequencyConnected ? std::max(inputs[FREQ1_INPUT + row].getChannels(), 1) : normalFrequencyChannels;
            const int modulationChannels = std::max(inputs[FM_RES1_INPUT + row].getChannels(), 1);
            const int channels = std::min(std::max({inputChannels, frequencyChannels, modulationChannels}), PORT_MAX_CHANNELS);
            rowChannels[row] = channels;
            scanChannels = std::max(scanChannels, channels);
            outputs[OUT1_OUTPUT + row].setChannels(channels);
            const int oldActiveChannels = activeVoiceChannels[row];
            for (int voice = oldActiveChannels; voice < channels; voice++) {
                engines[row][voice].setSampleRate(currentSampleRate);
            }

            const CVDest cvDest = static_cast<CVDest>(params[FM_RES_1_PARAM + row].getValue());
            const FilterMode mode = static_cast<FilterMode>(params[MODE1_PARAM + row].getValue());
            const float frequencyScaled = rescale(params[FREQ1_PARAM + row].getValue(), std::log2(vostok_ripples::kFreqKnobMin),
                                                  std::log2(vostok_ripples::kFreqKnobMax), 0.f, 1.f);
            float lightVoltage = 0.f;

            // Economy is lane-independent, so process four voices at once.
            // Reset only lanes whose active status changed; continuing voices
            // retain their filter history across normal polyphony changes.
            if (filterEngine == ECONOMY_TPT) {
                for (int voice = std::min(oldActiveChannels, channels); voice < std::max(oldActiveChannels, channels);
                     voice++) {
                    tptSimdEngines[row][voice / 4].resetLane(voice % 4);
                }

                const float resKnob = 0.8f * params[RES1_PARAM + row].getValue();
                for (int voice = 0; voice < channels; voice += 4) {
                    float_4 input = inputConnected ? inputs[IN1_INPUT + row].getPolyVoltageSimd<float_4>(voice)
                                                   : float_4::load(normalInputs + voice);
                    float_4 frequencyCv = frequencyConnected ? inputs[FREQ1_INPUT + row].getPolyVoltageSimd<float_4>(voice)
                                                             : float_4::load(normalFrequencies + voice);
                    float_4 modulationCv = inputs[FM_RES1_INPUT + row].getPolyVoltageSimd<float_4>(voice);
                    float_4 resonanceCv = (cvDest == RES) ? simd::clamp(modulationCv / 5.f, -1.f, 1.f) : float_4::zero();
                    float_4 fmCv = (cvDest == FM2) ? modulationCv : float_4::zero();

                    // The final block may contain fewer than four active
                    // voices.  Clear its inactive lanes before processing so
                    // they cannot influence state or become stale output.
                    for (int lane = std::max(channels - voice, 0); lane < 4; lane++) {
                        input[lane] = 0.f;
                        frequencyCv[lane] = 0.f;
                        resonanceCv[lane] = 0.f;
                        fmCv[lane] = 0.f;
                    }

                    float_4 output =
                        tptSimdEngines[row][voice / 4].process(input, frequencyCv, fmCv, resonanceCv, frequencyScaled, resKnob,
                                                               addLowend, true, clipOutput, static_cast<int>(mode));
                    float outputValues[4];
                    float inputValues[4];
                    output.store(outputValues);
                    input.store(inputValues);
                    const int activeLanes = std::min(4, channels - voice);
                    if (activeLanes == 4) {
                        outputs[OUT1_OUTPUT + row].setVoltageSimd(output, voice);
                    } else {
                        for (int lane = 0; lane < activeLanes; lane++) {
                            outputs[OUT1_OUTPUT + row].setVoltage(outputValues[lane], voice + lane);
                        }
                    }
                    for (int lane = 0; lane < activeLanes; lane++) {
                        rowOutputs[row][voice + lane] = outputValues[lane];
                        lightVoltage = std::max(lightVoltage, std::abs(inputValues[lane]));
                    }
                }
            } else {
                for (int voice = 0; voice < channels; voice++) {
                    const float input = inputConnected ? inputs[IN1_INPUT + row].getPolyVoltage(voice) : normalInputs[voice];
                    const float frequencyCv =
                        frequencyConnected ? inputs[FREQ1_INPUT + row].getPolyVoltage(voice) : normalFrequencies[voice];
                    const float modulationCv = inputs[FM_RES1_INPUT + row].getPolyVoltage(voice);
                    const float resonanceCv = (cvDest == RES) ? clamp(modulationCv / 5.f, -1.f, 1.f) : 0.f;

                    vostok_ripples::RipplesEngine::Frame frame;
                    frame.fm_knob = 1.f;
                    frame.addLowend = addLowend;
                    frame.gainCompensation = true;
                    frame.clipOutputs = clipOutput;
                    frame.res_knob = clamp(0.8f * params[RES1_PARAM + row].getValue() + 0.9f * resonanceCv, 0.f, 0.9f);
                    frame.freq_knob = frequencyScaled;
                    frame.fm_cv = (cvDest == FM2) ? modulationCv : 0.f;
                    frame.freq_cv = frequencyCv;
                    frame.input = input;

                    engines[row][voice].process(frame);

                    // Atlas corrects the reference engine's inverting effect.
                    const float output = -(mode == LP ? frame.lp4 : (mode == BP ? frame.bp4 : 0.5f * frame.hp2));
                    rowOutputs[row][voice] = output;
                    outputs[OUT1_OUTPUT + row].setVoltage(output, voice);
                    lightVoltage = std::max(lightVoltage, std::abs(input));
                }
            }

            activeVoiceChannels[row] = channels;

            for (int voice = 0; voice < PORT_MAX_CHANNELS; voice++) {
                normalInputs[voice] = inputConnected ? inputs[IN1_INPUT + row].getPolyVoltage(voice) : normalInputs[voice];
                normalFrequencies[voice] =
                    frequencyConnected ? inputs[FREQ1_INPUT + row].getPolyVoltage(voice) : normalFrequencies[voice];
            }
            normalInputChannels = inputChannels;
            normalFrequencyChannels = frequencyChannels;

            if (updateLeds) {
                const float sampleTime = args.sampleTime * lightUpdateRate;
                lights[NUM1_LIGHT + row].setBrightnessSmooth(lightVoltage / 5.f, sampleTime, lambda);
            }
        }

        // Scan output. Mono Scan CV broadcasts; polyphonic Scan CV addresses
        // the corresponding voice independently.
        scanChannels = std::max(scanChannels, std::max(inputs[SCAN_IN_INPUT].getChannels(), 1));
        outputs[SCAN_OUT_OUTPUT].setChannels(scanChannels);
        for (int voice = 0; voice < scanChannels; voice++) {
            const float scanValue =
                clamp(params[SCAN_PARAM].getValue() + inputs[SCAN_IN_INPUT].getPolyVoltage(voice) / 10.f, 0.f, 1.f);
            const float_4 outGains = gainsForChannels(scanValue);
            float scanOut = 0.f;
            for (int row = 0; row < NUM_CHANNELS; row++) {
                const float rowOutput =
                    rowChannels[row] == 1 ? rowOutputs[row][0] : (voice < rowChannels[row] ? rowOutputs[row][voice] : 0.f);
                scanOut += rowOutput * outGains[row];
            }
            outputs[SCAN_OUT_OUTPUT].setVoltage(scanOut, voice);
        }
    }

    json_t *dataToJson() override {
        json_t *rootJ = json_object();
        json_object_set_new(rootJ, "addLowend", json_boolean(addLowend));
        json_object_set_new(rootJ, "clipOutput", json_boolean(clipOutput));
        const char *engineName = "accurate";
        if (filterEngine == ECONOMY_TPT) {
            engineName = "economy-tpt";
        }
        json_object_set_new(rootJ, "filterEngine", json_string(engineName));

        return rootJ;
    }

    void dataFromJson(json_t *rootJ) override {
        json_t *jAddLowend = json_object_get(rootJ, "addLowend");
        if (jAddLowend) {
            addLowend = json_boolean_value(jAddLowend);
        }

        json_t *jClipOutput = json_object_get(rootJ, "clipOutput");
        if (jClipOutput) {
            clipOutput = json_boolean_value(jClipOutput);
        }

        // Both values of Atlas' former hidden numeric selector used the
        // existing circuit engine. A missing key therefore identifies a
        // legacy patch and must retain its sound, while newly created modules
        // keep the Economy default set by the member initializer.
        json_t *jFilterEngine = json_object_get(rootJ, "filterEngine");
        if (jFilterEngine && json_is_string(jFilterEngine)) {
            const std::string value = json_string_value(jFilterEngine);
            if (value == "economy-tpt") {
                filterEngine = ECONOMY_TPT;
            } else {
                filterEngine = ACCURATE_CIRCUIT;
            }
        } else {
            filterEngine = ACCURATE_CIRCUIT;
        }
    }
};

struct AtlasWidget : ModuleWidget {
    AtlasWidget(Atlas *module) {
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
            addParam(
                createParamCentered<RoundBlackKnob>(mm2px(Vec(51.201, 15.303 + i * step_y)), module, Atlas::FREQ1_PARAM + i));
            addParam(
                createParamCentered<RoundBlackKnob>(mm2px(Vec(68.872, 15.303 + i * step_y)), module, Atlas::RES1_PARAM + i));
            addParam(createParam<CKSSHoriz3>(mm2px(Vec(18.52, 24.418 + i * step_y)), module, Atlas::MODE1_PARAM + i));
            addParam(createParam<CKSSNarrow>(mm2px(Vec(58.254, 21.444 + i * step_y)), module, Atlas::FM_RES_1_PARAM + i));

            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.204, 15.297 + i * step_y)), module, Atlas::IN1_INPUT + i));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.426, 15.297 + i * step_y)), module, Atlas::FREQ1_INPUT + i));
            addInput(
                createInputCentered<PJ301MPort>(mm2px(Vec(26.649, 15.297 + i * step_y)), module, Atlas::FM_RES1_INPUT + i));
            addOutput(
                createOutputCentered<PJ301MPort>(mm2px(Vec(35.872, 15.297 + i * step_y)), module, Atlas::OUT1_OUTPUT + i));
        }

        addParam(createParam<VostokSliderHoriz>(mm2px(Vec(46.752, 109.224)), module, Atlas::SCAN_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.68, 112.704)), module, Atlas::SCAN_IN_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(37.914, 112.704)), module, Atlas::SCAN_OUT_OUTPUT));

        // leds
        addChild(createLight<VostokOrangeNumberLed<1>>(mm2px(Vec(41.461, 19.259)), module, Atlas::NUM1_LIGHT + 0));
        addChild(createLight<VostokOrangeNumberLed<2>>(mm2px(Vec(41.461, 42.639)), module, Atlas::NUM1_LIGHT + 1));
        addChild(createLight<VostokOrangeNumberLed<3>>(mm2px(Vec(41.074, 66.014)), module, Atlas::NUM1_LIGHT + 2));
        addChild(createLight<VostokOrangeNumberLed<4>>(mm2px(Vec(41.074, 89.511)), module, Atlas::NUM1_LIGHT + 3));
    }

    void appendContextMenu(Menu *menu) override {
        Atlas *module = dynamic_cast<Atlas *>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator());
        menu->addChild(createSubmenuItem("Hardware compatibility", "", [=](Menu *menu) {
            menu->addChild(createBoolPtrMenuItem("Clip Output ±10V", "", &module->clipOutput));
        }));

        menu->addChild(createIndexPtrSubmenuItem("Filter engine", {"Legacy circuit model (high CPU)", "Efficient module (low CPU)"},
                                                 &module->filterEngine));

        // debug options only, don't expose to users yet
        // menu->addChild(createBoolPtrMenuItem("Add lowend to HP", "", &module->addLowend));
    }
};

Model *modelAtlas = createModel<Atlas, AtlasWidget>("Atlas");
