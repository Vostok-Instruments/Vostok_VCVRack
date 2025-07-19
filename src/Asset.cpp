#include "plugin.hpp"


struct Asset : Module {
	static const int NUM_CHANNELS = 6;
	enum ParamId {
		ENUMS(LEVEL1_PARAM, NUM_CHANNELS),
		ENUMS(OFFSET1_PARAM, NUM_CHANNELS),
		ENUMS(INPUT_POLARITY1_PARAM, NUM_CHANNELS),
		ENUMS(OFFSET_POLARITY1_PARAM, NUM_CHANNELS),
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(IN1_INPUT, NUM_CHANNELS),
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(OUT1_OUTPUT, NUM_CHANNELS),
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(NUM1_LIGHT, NUM_CHANNELS * 2), 	// orange/blue bipolar lights
		LIGHTS_LEN
	};

	ParamQuantity* gains[NUM_CHANNELS];
	ParamQuantity* offsets[NUM_CHANNELS];
	bool clipOutput = true;

	Asset() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < NUM_CHANNELS; i++) {
			gains[i] = configParam(LEVEL1_PARAM + i, 0.f, 1.f, 1.f, string::f("Ch. %d level", i + 1));
			offsets[i] = configParam(OFFSET1_PARAM + i, 0.f, 10.f, 0.f, string::f("Ch. %d offset", i + 1), "V");
			configSwitch(INPUT_POLARITY1_PARAM + i, 0.f, 1.f, 0.f, string::f("Ch. %d input", i + 1), {"Normal", "Inverted"});
			configSwitch(OFFSET_POLARITY1_PARAM + i, 0.f, 1.f, 0.f, string::f("Ch. %d DC Offset polarity", i + 1), {"Positive", "Negative"});
			configInput(IN1_INPUT + i, string::f("Ch. %d", i + 1));
			configOutput(OUT1_OUTPUT + i, string::f("Ch. %d", i + 1));
		}
	}

	void process(const ProcessArgs& args) override {

		float normalVoltage = 0.f;
		for (int i = 0; i < NUM_CHANNELS; i++) {

			const float inputPolarity = params[INPUT_POLARITY1_PARAM + i].getValue() ? -1.f : +1.f;
			const float offsetPolarity = params[OFFSET_POLARITY1_PARAM + i].getValue() ? -1.f : +1.f;

			gains[i]->displayMultiplier = inputPolarity;
			offsets[i]->displayMultiplier = offsetPolarity;

			float level = params[LEVEL1_PARAM + i].getValue() * inputPolarity;
			float offset = params[OFFSET1_PARAM + i].getValue() * offsetPolarity;
			float in = inputs[IN1_INPUT + i].getNormalVoltage(normalVoltage);
			normalVoltage = in;

			float out = (in * level) + offset;
			if (clipOutput) {
				out = clamp(out, -10.f, +10.f);
			}
			outputs[OUT1_OUTPUT + i].setVoltage(out);

			// orange for positive, blue for negative
			lights[NUM1_LIGHT + 2 * i + 0].setBrightness(out > 0.f ? +out / 10.f : 0.f);
			lights[NUM1_LIGHT + 2 * i + 1].setBrightness(out < 0.f ? -out / 10.f : 0.f);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "clipOutput", json_boolean(clipOutput));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* clipOutputJ = json_object_get(rootJ, "clipOutput");
		if (clipOutputJ) {
			clipOutput = json_boolean_value(clipOutputJ);
		}
	}
};


struct AssetWidget : ModuleWidget {
	AssetWidget(Asset* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Asset.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const float first_y = 13.517;
		const float last_y = 105.499;
		const float step_y = (last_y - first_y) / (Asset::NUM_CHANNELS - 1);

		for (int i = 0; i < Asset::NUM_CHANNELS; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(27.473, 15.356 + i * step_y)), module, Asset::LEVEL1_PARAM + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(41.971, 15.356 + i * step_y)), module, Asset::OFFSET1_PARAM + i));
			addParam(createParam<CKSSHoriz2>(mm2px(Vec(3.635, 23.511 + i * step_y)), module, Asset::INPUT_POLARITY1_PARAM + i));
			addParam(createParam<CKSSHoriz2>(mm2px(Vec(13.577, 23.511 + i * step_y)), module, Asset::OFFSET_POLARITY1_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.587, 15.337 + i * step_y)), module, Asset::IN1_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.731, 15.337 + i * step_y)), module, Asset::OUT1_OUTPUT + i));
		}

		addChild(createLight<VostokUpperOrangeNumberLed<1>>(mm2px(Vec(31.994, 19.259)), module, Asset::NUM1_LIGHT + 2 * 0 + 0));
		addChild(createLight<VostokUpperOrangeNumberLed<2>>(mm2px(Vec(31.994, 37.665)), module, Asset::NUM1_LIGHT + 2 * 1 + 0));
		addChild(createLight<VostokUpperOrangeNumberLed<3>>(mm2px(Vec(31.607, 56.040)), module, Asset::NUM1_LIGHT + 2 * 2 + 0));
		addChild(createLight<VostokUpperOrangeNumberLed<4>>(mm2px(Vec(31.607, 74.537)), module, Asset::NUM1_LIGHT + 2 * 3 + 0));
		addChild(createLight<VostokUpperOrangeNumberLed<5>>(mm2px(Vec(31.941, 92.853)), module, Asset::NUM1_LIGHT + 2 * 4 + 0));
		addChild(createLight<VostokUpperOrangeNumberLed<6>>(mm2px(Vec(31.941, 111.296)), module, Asset::NUM1_LIGHT + 2 * 5 + 0));

		addChild(createLight<VostokLowerBlueNumberLed<1>>(mm2px(Vec(31.994, 19.259)), module, Asset::NUM1_LIGHT + 2 * 0 + 1));
		addChild(createLight<VostokLowerBlueNumberLed<2>>(mm2px(Vec(31.994, 37.665)), module, Asset::NUM1_LIGHT + 2 * 1 + 1));
		addChild(createLight<VostokLowerBlueNumberLed<3>>(mm2px(Vec(31.607, 56.040)), module, Asset::NUM1_LIGHT + 2 * 2 + 1));
		addChild(createLight<VostokLowerBlueNumberLed<4>>(mm2px(Vec(31.607, 74.537)), module, Asset::NUM1_LIGHT + 2 * 3 + 1));
		addChild(createLight<VostokLowerBlueNumberLed<5>>(mm2px(Vec(31.941, 92.853)), module, Asset::NUM1_LIGHT + 2 * 4 + 1));
		addChild(createLight<VostokLowerBlueNumberLed<6>>(mm2px(Vec(31.941, 111.296)), module, Asset::NUM1_LIGHT + 2 * 5 + 1));
	}

	void appendContextMenu(Menu* menu) override {
		Asset* asset = dynamic_cast<Asset*>(module);
		assert(asset);

		menu->addChild(new MenuSeparator());
		menu->addChild(createBoolPtrMenuItem("Clip Output ±10V", "", &asset->clipOutput));
	}
};


Model* modelAsset = createModel<Asset, AssetWidget>("Asset");