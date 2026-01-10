#include "pluginFactory.h"
#include <Log.h>

PluginFactory* PluginFactory::instance = nullptr;

PluginFactory& PluginFactory::getInstance() {
    if (instance == nullptr) {
        instance = new PluginFactory();
    }
    return *instance;
}

void PluginFactory::registerFactory(std::unique_ptr<AnimationFactory> factory) {
    if (factory == nullptr) {
        Log::errorln("Attempted to register null factory.");
        return;
    }
    
    std::string type = factory->getType();
    if (type.empty()) {
        Log::errorln("Attempted to register factory with empty type.");
        return;
    }
    
    Log::infoln("Registering animation factory for type: %s", type.c_str());
    factories[type] = std::move(factory);
}

int PluginFactory::createAnimationsFromPlugins(
    Scheduler* scheduler,
    const std::vector<PluginCfg>& plugins,
    DmxManager* dmxManager) {
    
    int createdCount = 0;
    
    for (const auto& plugin : plugins) {
        if (createAnimationFromPlugin(scheduler, plugin, dmxManager)) {
            createdCount++;
        }
    }
    
    return createdCount;
}

bool PluginFactory::createAnimationFromPlugin(
    Scheduler* scheduler,
    const PluginCfg& plugin,
    DmxManager* dmxManager) {
    
    if (plugin.type.empty()) {
        Log::errorln("Plugin has empty type, skipping");
        return false;
    }
    
    auto factoryIt = factories.find(plugin.type);
    if (factoryIt == factories.end()) {
        Log::errorln("No factory registered for plugin type: %s", plugin.type.c_str());
        return false;
    }
    
    try {
        return factoryIt->second->createAnimation(scheduler, plugin.config, dmxManager);
    } catch (const std::exception& e) {
        Log::errorln("Exception creating animation %s: %s", plugin.name.c_str(), e.what());
        return false;
    } catch (...) {
        Log::errorln("Unknown exception creating animation: %s", plugin.name.c_str());
        return false;
    }
}
