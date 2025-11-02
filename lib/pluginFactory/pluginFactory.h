#pragma once

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <ArduinoJson.h>
#include <settings.h>

class DmxListener;
class Scheduler;

class AnimationFactory { // TODO what's the difference between this and PluginFactory?
public:
    virtual ~AnimationFactory() = default;
    virtual bool createAnimation(Scheduler* scheduler, const std::string& config, DmxListener* dmxListener) = 0;
    virtual std::string getType() const = 0;
};

class PluginFactory {
private:
    static PluginFactory* instance;
    std::map<std::string, std::unique_ptr<AnimationFactory>> factories;
    PluginFactory() = default;

public:
    static PluginFactory& getInstance();
    void registerFactory(std::unique_ptr<AnimationFactory> factory);
    int createAnimationsFromPlugins(Scheduler* scheduler, const std::vector<PluginCfg>& plugins, DmxListener* dmxListener);
    bool createAnimationFromPlugin(Scheduler* scheduler, const PluginCfg& plugin, DmxListener* dmxListener);
};

template<typename FactoryType>
class FactoryRegistrar {
public:
    FactoryRegistrar() {
        PluginFactory::getInstance().registerFactory(std::unique_ptr<AnimationFactory>(new FactoryType()));
    }
};

#define REGISTER_ANIMATION_FACTORY(FactoryClass) static FactoryRegistrar<FactoryClass> _registrar_##FactoryClass;
