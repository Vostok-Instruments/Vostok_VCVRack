#include "plugin.hpp"


Plugin* pluginInstance;


void init(Plugin* p) {
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
	
	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
