#include "plugin.hpp"

using simd::float_4;
using simd::Vector;

struct Halo : Module {

    static const int NUM_ROWS = 4;

    enum ParamId {
        ENUMS(FREQ_PARAM, NUM_ROWS),
        ENUMS(LO_HI_PARAM, NUM_ROWS),
        ENUMS(TRI_SQUARE_PARAM, NUM_ROWS),
        PARAMS_LEN,
    };
    enum InputId {
        ENUMS(FREQ_INPUT, NUM_ROWS),
        INPUTS_LEN,
    };
    enum OutputId {
        ENUMS(TRI_OUTPUT, NUM_ROWS),
        ENUMS(SQUARE_OUTPUT, NUM_ROWS),
        AND12_OUTPUT,
        OR12_OUTPUT,
        XOR12_OUTPUT,
        MIX12_OUTPUT,
        AND34_OUTPUT,
        OR34_OUTPUT,
        XOR34_OUTPUT,
        MIX34_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(NUM_LIGHT, NUM_ROWS),
        LIGHTS_LEN
    };

    float_4 phases = {};

    Halo() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        for (int i = 0; i < NUM_ROWS; i++) {
            configInput(FREQ_INPUT + i, "Frequency " + std::to_string(i + 1));
            configOutput(TRI_OUTPUT + i, "Triangle Wave " + std::to_string(i + 1));
            configOutput(SQUARE_OUTPUT + i, "Square Wave " + std::to_string(i + 1));
            configSwitch(LO_HI_PARAM + i, 0.0f, 1.0f, 0.0f, "Frequency Range " + std::to_string(i + 1), {"Low", "High"});
            configSwitch(TRI_SQUARE_PARAM + i, 0.0f, 1.0f, 0.0f, "Logic Waveform " + std::to_string(i + 1),
                         {"Triangle", "Square"});
            configParam(FREQ_PARAM + i, 0.0f, 1.0f, 0.5f, "Frequency " + std::to_string(i + 1));
        }

        configOutput(AND12_OUTPUT, "Logical AND Channels 1 and 2");
        configOutput(OR12_OUTPUT, "Logical OR Channels 1 and 2");
        configOutput(XOR12_OUTPUT, "Logical XOR Channels 1 and 2");
        configOutput(MIX12_OUTPUT, "Mixed Output Channels 1 and 2");
        configOutput(AND34_OUTPUT, "Logical AND Channels 3 and 4");
        configOutput(OR34_OUTPUT, "Logical OR Channels 3 and 4");
        configOutput(XOR34_OUTPUT, "Logical XOR Channels 3 and 4");
        configOutput(MIX34_OUTPUT, "Mixed Output Channels 3 and 4");
    }

    // low mode
    static constexpr float LOW_MIN_FREQ = 0.00875;
    static constexpr float LOW_MAX_FREQ = 8.75f;
    // high mode
    static constexpr float HI_MIN_FREQ = 0.05f;
    static constexpr float HI_MAX_FREQ = 87.5f;
    void process(const ProcessArgs &args) override {

        // freq = pot (0 - 1) + cv / 10V
        float_4 freqs = float_4(params[FREQ_PARAM + 0].getValue(), params[FREQ_PARAM + 1].getValue(),
                                params[FREQ_PARAM + 2].getValue(), params[FREQ_PARAM + 3].getValue());
        const float_4 cvs = float_4(inputs[FREQ_INPUT + 0].getVoltage(), inputs[FREQ_INPUT + 1].getVoltage(),
                                    inputs[FREQ_INPUT + 2].getVoltage(), inputs[FREQ_INPUT + 3].getVoltage());
        freqs = clamp(freqs + cvs / 10.f, 0.f, 1.f);

        const float_4 hiRange = float_4(params[LO_HI_PARAM + 0].getValue(), params[LO_HI_PARAM + 1].getValue(),
                                        params[LO_HI_PARAM + 2].getValue(), params[LO_HI_PARAM + 3].getValue()) > 0.5f;

        freqs = ifelse(hiRange, HI_MIN_FREQ * simd::pow(HI_MAX_FREQ / HI_MIN_FREQ, freqs),
                       LOW_MIN_FREQ * simd::pow(LOW_MAX_FREQ / LOW_MIN_FREQ, freqs));

        phases += freqs * args.sampleTime;
        phases -= simd::floor(phases);

        float_4 tri = 5.f * ifelse(phases < 0.5f, (phases * 4.f - 1.f), ((1.f - phases) * 4.f - 1.f));
        float_4 square = 5.f * ifelse(phases < 0.5f, 1.f, -1.f);

        const float_4 useSquare =
            float_4(params[TRI_SQUARE_PARAM + 0].getValue(), params[TRI_SQUARE_PARAM + 1].getValue(),
                    params[TRI_SQUARE_PARAM + 2].getValue(), params[TRI_SQUARE_PARAM + 3].getValue()) > 0.5f;
        const float_4 waveformForLogic = ifelse(useSquare, square, tri);

        // Process each LFO channel
        for (int i = 0; i < NUM_ROWS; i++) {
            // Output waveforms
            outputs[TRI_OUTPUT + i].setVoltage(tri[i]);
            outputs[SQUARE_OUTPUT + i].setVoltage(square[i]);

            lights[NUM_LIGHT + i].setBrightnessSmooth((waveformForLogic[i] + 5.f) / 10.f, args.sampleTime, 15.f);
        }

        // Logic operations for channels 1-2
        processLogic(waveformForLogic[0], waveformForLogic[1], AND12_OUTPUT, OR12_OUTPUT, XOR12_OUTPUT, MIX12_OUTPUT);

        // Logic operations for channels 3-4
        processLogic(waveformForLogic[2], waveformForLogic[3], AND34_OUTPUT, OR34_OUTPUT, XOR34_OUTPUT, MIX34_OUTPUT);
    }

    void processLogic(float sig1, float sig2, OutputId andOut, OutputId orOut, OutputId xorOut, OutputId mixOut) {

        // Logic operations (output ±5V)
        float andResult = fmin(sig1, sig2);
        float orResult = fmax(sig1, sig2);
        float xorResult = orResult - andResult;
        outputs[andOut].setVoltage(andResult);
        outputs[orOut].setVoltage(orResult);
        outputs[xorOut].setVoltage(xorResult);

        // Mix output (average of selected waveforms)
        outputs[mixOut].setVoltage((sig1 + sig2) * 0.5f);
    }
};

struct HaloWidget : ModuleWidget {
    HaloWidget(Halo *module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Halo.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // lfo rows
        {
            const float x1 = 8.32f, y1 = 15.40f;
            const float dx = 17.51 - x1;
            const float dy = 33.83f - y1;
            const float potX = 41.07f;
            const float x1Switch = 6.f, y1Switch = 23.4f;
            const float dxSwitch = 18.48f - x1Switch;

            for (int i = 0; i < Halo::NUM_ROWS; i++) {
                // ins/outs
                addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x1, y1 + i * dy)), module, Halo::FREQ_INPUT + i));
                addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x1 + dx, y1 + i * dy)), module, Halo::TRI_OUTPUT + i));
                addOutput(
                    createOutputCentered<PJ301MPort>(mm2px(Vec(x1 + 2 * dx, y1 + i * dy)), module, Halo::SQUARE_OUTPUT + i));
                // pot
                addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(potX, y1 + i * dy)), module, Halo::FREQ_PARAM + i));
                // switches
                addParam(createParam<CKSSHoriz2>(mm2px(Vec(x1Switch, y1Switch + i * dy)), module, Halo::LO_HI_PARAM + i));
                addParam(createParam<CKSSHoriz2>(mm2px(Vec(x1Switch + dxSwitch, y1Switch + i * dy)), module,
                                                 Halo::TRI_SQUARE_PARAM + i));
            }
        }

        // logic outs
        {
            const float ys12 = 96.47f;
            const float ys34 = 111.45f;
            const float x0 = 8.92f, x3 = 35.78f;
            const float dx = (x3 - x0) / 3.f;
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x0, ys12)), module, Halo::AND12_OUTPUT));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x0 + dx, ys12)), module, Halo::OR12_OUTPUT));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x0 + 2 * dx, ys12)), module, Halo::XOR12_OUTPUT));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x0 + 3 * dx, ys12)), module, Halo::MIX12_OUTPUT));
            // 34s
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x0, ys34)), module, Halo::AND34_OUTPUT));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x0 + dx, ys34)), module, Halo::OR34_OUTPUT));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x0 + 2 * dx, ys34)), module, Halo::XOR34_OUTPUT));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x0 + 3 * dx, ys34)), module, Halo::MIX34_OUTPUT));
        }

        // leds
        addChild(createLight<VostokOrangeNumberLed<1>>(mm2px(Vec(30.847, 19.259)), module, Halo::NUM_LIGHT + 0));
        addChild(createLight<VostokOrangeNumberLed<2>>(mm2px(Vec(30.847, 37.665)), module, Halo::NUM_LIGHT + 1));
        addChild(createLight<VostokOrangeNumberLed<3>>(mm2px(Vec(30.460, 56.040)), module, Halo::NUM_LIGHT + 2));
        addChild(createLight<VostokOrangeNumberLed<4>>(mm2px(Vec(30.460, 74.537)), module, Halo::NUM_LIGHT + 3));
    }
};

Model *modelHalo = createModel<Halo, HaloWidget>("Halo");