#include "plugin.hpp"

struct Sigma : Module {
    constexpr static int NUM_CHANNELS = 4;
    static constexpr float MIN_LENGTH = 1e-3f;
    static constexpr float MAX_LENGTH = 1.f;
    static constexpr float LENGTH_INFINITE_THRESHOLD = 0.95f;
    static constexpr float LENGTH_CURVE = 2.5f;
    static constexpr float RETRIGGER_GAP = 1e-3f;

    enum OutputRange {
        RANGE_5V,
        RANGE_10V
    };

    enum ParamId {
        ENUMS(LENGTH_PARAM, NUM_CHANNELS),
        PARAMS_LEN
    };
    enum InputId {
        ENUMS(GATE_INPUT, NUM_CHANNELS),
        ENUMS(LENGTH_INPUT, NUM_CHANNELS),
        INPUTS_LEN
    };
    enum OutputId {
        ENUMS(OUT_OUTPUT, NUM_CHANNELS),
        ENUMS(NOT_OUTPUT, NUM_CHANNELS),
        ENUMS(AND_OUTPUT, NUM_CHANNELS - 1),
        ENUMS(NAND_OUTPUT, NUM_CHANNELS - 1),
        ENUMS(OR_OUTPUT, NUM_CHANNELS - 1),
        ENUMS(NOR_OUTPUT, NUM_CHANNELS - 1),
        ENUMS(XOR_OUTPUT, NUM_CHANNELS - 1),
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(NUM_LIGHT, NUM_CHANNELS),
        LIGHTS_LEN
    };

    dsp::SchmittTrigger gateTriggers[NUM_CHANNELS]; // detect rising edges per channel
    dsp::PulseGenerator pulseGens[NUM_CHANNELS];    // finite pulse timing per channel
    bool gateStates[NUM_CHANNELS] = {};             // current channel outputs
    bool infiniteStates[NUM_CHANNELS] = {};         // latched infinite-mode states
    bool pendingRetriggers[NUM_CHANNELS] = {};      // output held low for hardware-style retrigger gap
    float pendingDurations[NUM_CHANNELS] = {};      // deferred pulse lengths after retrigger
    float retriggerGaps[NUM_CHANNELS] = {};         // low time remaining before restart
    OutputRange outputRange = RANGE_5V;
    dsp::ClockDivider lightDivider;

    static float getLengthSeconds(float normalized) {
        const float finiteNormalized = clamp(normalized / LENGTH_INFINITE_THRESHOLD, 0.f, 1.f);
        const float curved = std::pow(finiteNormalized, LENGTH_CURVE);
        return MIN_LENGTH * std::pow(MAX_LENGTH / MIN_LENGTH, curved);
    }

    static float getNormalizedLengthFromSeconds(float seconds) {
        const float clampedSeconds = clamp(seconds, MIN_LENGTH, MAX_LENGTH);
        const float curved = std::log(clampedSeconds / MIN_LENGTH) / std::log(MAX_LENGTH / MIN_LENGTH);
        const float finiteNormalized = std::pow(curved, 1.f / LENGTH_CURVE);
        return finiteNormalized * LENGTH_INFINITE_THRESHOLD;
    }

    struct LengthParamQuantity : ParamQuantity {
        float getDisplayValue() override {
            const float value = getValue();
            if (value > LENGTH_INFINITE_THRESHOLD) {
                return INFINITY;
            }
            return getLengthSeconds(value);
        }

        void setDisplayValue(float displayValue) override {
            if (!std::isfinite(displayValue) || displayValue > MAX_LENGTH) {
                setImmediateValue(1.f);
                return;
            }
            setImmediateValue(getNormalizedLengthFromSeconds(displayValue));
        }

        std::string getDisplayValueString() override {
            if (getValue() > LENGTH_INFINITE_THRESHOLD) {
                return "Infinite";
            }
            return ParamQuantity::getDisplayValueString();
        }

        void setDisplayValueString(std::string s) override {
            if (s == "Infinite" || s == "infinite" || s == "Inf" || s == "inf") {
                setImmediateValue(1.f);
                return;
            }
            ParamQuantity::setDisplayValueString(s);
        }

        std::string getUnit() override {
            if (getValue() > LENGTH_INFINITE_THRESHOLD) {
                return "";
            }
            return " s";
        }
    };

    Sigma() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            configParam<LengthParamQuantity>(LENGTH_PARAM + ch, 0.f, 1.f, 0.5f, "Length " + std::to_string(ch + 1));
            configInput(GATE_INPUT + ch, "Gate/Trigger " + std::to_string(ch + 1));
            configInput(LENGTH_INPUT + ch, "Length CV " + std::to_string(ch + 1));
            configOutput(OUT_OUTPUT + ch, "Gate Output " + std::to_string(ch + 1));
            configOutput(NOT_OUTPUT + ch, "Inverted Gate Output " + std::to_string(ch + 1));
        }

        for (int ch = 0; ch < NUM_CHANNELS - 1; ch++) {
            const std::string pair = std::to_string(ch + 1) + " and " + std::to_string(ch + 2);
            configOutput(AND_OUTPUT + ch, "Logical AND Channels " + pair);
            configOutput(NAND_OUTPUT + ch, "Logical NAND Channels " + pair);
            configOutput(OR_OUTPUT + ch, "Logical OR Channels " + pair);
            configOutput(NOR_OUTPUT + ch, "Logical NOR Channels " + pair);
            configOutput(XOR_OUTPUT + ch, "Logical XOR Channels " + pair);
        }

        lightDivider.setDivision(lightUpdateRate);
    }

    void onReset(const ResetEvent &e) override {
        Module::onReset(e);
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            gateTriggers[ch].reset();
            pulseGens[ch].reset();
            gateStates[ch] = false;
            infiniteStates[ch] = false;
            pendingRetriggers[ch] = false;
            pendingDurations[ch] = 0.f;
            retriggerGaps[ch] = 0.f;
        }
        lightDivider.reset();
    }

    float getNormalizedLength(int ch) {
        float normalized = params[LENGTH_PARAM + ch].getValue();
        if (inputs[LENGTH_INPUT + ch].isConnected()) {
            normalized += inputs[LENGTH_INPUT + ch].getVoltage() / 5.f;
        }
        return clamp(normalized, 0.f, 1.f);
    }

    void process(const ProcessArgs &args) override {
        const float pulseVoltage = outputRange == RANGE_10V ? 10.f : 5.f;
        const bool updateLights = lightDivider.process();

        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            const float normalizedLength = getNormalizedLength(ch);
            const bool infiniteMode = normalizedLength > LENGTH_INFINITE_THRESHOLD;

            if (gateTriggers[ch].process(inputs[GATE_INPUT + ch].getVoltage())) {
                const bool isHigh = pulseGens[ch].remaining > 0.f;
                const bool retriggering = infiniteStates[ch] || isHigh || pendingRetriggers[ch];
                infiniteStates[ch] = infiniteMode;

                if (!infiniteMode) {
                    pendingDurations[ch] = getLengthSeconds(normalizedLength);
                }

                if (retriggering) {
                    pulseGens[ch].reset();
                    pendingRetriggers[ch] = true;
                    retriggerGaps[ch] = RETRIGGER_GAP;
                } else if (!infiniteMode) {
                    pulseGens[ch].trigger(pendingDurations[ch]);
                }
            }

            bool gateState = infiniteStates[ch];
            if (pendingRetriggers[ch]) {
                gateState = false;
                retriggerGaps[ch] -= args.sampleTime;
                if (retriggerGaps[ch] <= 0.f) {
                    if (!infiniteStates[ch]) {
                        pulseGens[ch].trigger(pendingDurations[ch]);
                    }
                    pendingRetriggers[ch] = false;
                }
            } else if (!gateState) {
                gateState = pulseGens[ch].process(args.sampleTime);
            }
            gateStates[ch] = gateState;

            const float gateVoltage = gateState ? pulseVoltage : 0.f;
            outputs[OUT_OUTPUT + ch].setVoltage(gateVoltage);
            outputs[NOT_OUTPUT + ch].setVoltage(gateState ? 0.f : pulseVoltage);
            if (updateLights) {
                lights[NUM_LIGHT + ch].setBrightnessSmooth(gateState ? 1.f : 0.f, args.sampleTime * lightUpdateRate, lambda);
            }
        }

        for (int ch = 0; ch < NUM_CHANNELS - 1; ch++) {
            const bool a = gateStates[ch];
            const bool b = gateStates[ch + 1];

            outputs[AND_OUTPUT + ch].setVoltage((a && b) ? pulseVoltage : 0.f);
            outputs[NAND_OUTPUT + ch].setVoltage((a && b) ? 0.f : pulseVoltage);
            outputs[OR_OUTPUT + ch].setVoltage((a || b) ? pulseVoltage : 0.f);
            outputs[NOR_OUTPUT + ch].setVoltage((a || b) ? 0.f : pulseVoltage);
            outputs[XOR_OUTPUT + ch].setVoltage((a != b) ? pulseVoltage : 0.f);
        }
    }

    json_t *dataToJson() override {
        json_t *rootJ = json_object();
        json_object_set_new(rootJ, "outputRange", json_integer(static_cast<int>(outputRange)));
        return rootJ;
    }

    void dataFromJson(json_t *rootJ) override {
        json_t *outputRangeJ = json_object_get(rootJ, "outputRange");
        if (outputRangeJ) {
            const int range = json_integer_value(outputRangeJ);
            if (range == RANGE_5V || range == RANGE_10V) {
                outputRange = static_cast<OutputRange>(range);
            }
        }
    }
};

struct SigmaWidget : ModuleWidget {
    SigmaWidget(Sigma *module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Sigma.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        for (int ch = 0; ch < Sigma::NUM_CHANNELS; ch++) {
            constexpr float dY = 33.803 - 15.403;
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.315, 15.403 + ch * dY)), module, Sigma::GATE_INPUT + ch));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.515, 15.356 + ch * dY)), module, Sigma::LENGTH_INPUT + ch));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(26.715, 15.403 + ch * dY)), module, Sigma::OUT_OUTPUT + ch));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(35.915, 15.403 + ch * dY)), module, Sigma::NOT_OUTPUT + ch));
            addParam(
                createParamCentered<RoundBlackKnob>(mm2px(Vec(51.256, 15.356 + ch * dY)), module, Sigma::LENGTH_PARAM + ch));
        }

        // paired channel logic outputs
        for (int ch = 0; ch < Sigma::NUM_CHANNELS - 1; ch++) {
            constexpr float dY = 100.353 - 87.603;

            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.315, 87.603 + ch * dY)), module, Sigma::AND_OUTPUT + ch));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(17.515, 87.603 + ch * dY)), module, Sigma::NAND_OUTPUT + ch));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(26.715, 87.603 + ch * dY)), module, Sigma::OR_OUTPUT + ch));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(35.915, 87.603 + ch * dY)), module, Sigma::NOR_OUTPUT + ch));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(45.115, 87.603 + ch * dY)), module, Sigma::XOR_OUTPUT + ch));
        }

        // leds
        addChild(createLight<VostokOrangeNumberLed<1>>(mm2px(Vec(40.947, 19.259)), module, Sigma::NUM_LIGHT + 0));
        addChild(createLight<VostokOrangeNumberLed<2>>(mm2px(Vec(40.947, 37.665)), module, Sigma::NUM_LIGHT + 1));
        addChild(createLight<VostokOrangeNumberLed<3>>(mm2px(Vec(40.947, 56.040)), module, Sigma::NUM_LIGHT + 2));
        addChild(createLight<VostokOrangeNumberLed<4>>(mm2px(Vec(40.947, 74.537)), module, Sigma::NUM_LIGHT + 3));
    }

    void appendContextMenu(Menu *menu) override {
        Sigma *module = dynamic_cast<Sigma *>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator());
        menu->addChild(createSubmenuItem("Hardware compatibility", "", [=](Menu *menu) {
            menu->addChild(createIndexPtrSubmenuItem("Output range", {"0-5V", "0-10V"}, &module->outputRange));
        }));
    }
};

Model *modelSigma = createModel<Sigma, SigmaWidget>("Sigma");
