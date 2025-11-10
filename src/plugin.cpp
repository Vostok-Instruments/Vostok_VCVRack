#include "plugin.hpp"

Plugin *pluginInstance;

void init(Plugin *p) {
    pluginInstance = p;

    // Add modules here
    p->addModel(modelPath);
    p->addModel(modelTrace);
    p->addModel(modelAsset);
    p->addModel(modelAtlas);
    p->addModel(modelCeres);
    p->addModel(modelFuji);
    p->addModel(modelSena);
    p->addModel(modelHive);
    p->addModel(modelHalo);
}

float_4 gainsForChannels(float routeValue) {
    float_4 routeValueForChannel = (routeValue - crossfaderCentres / 5.f);
    // channels 0 and 3 are special cases
    routeValueForChannel = simd::clamp(routeValueForChannel, crossfaderMins, crossfaderMaxs);
    // abs because we have a cubic
    routeValueForChannel = simd::abs(routeValueForChannel);

    return gains * simd::pow(2.0f, -routeValueForChannel * routeValueForChannel * routeValueForChannel * 290.f);
}