#include <waveEffect.h>
#include <pluginFactory.h>
#include <ArduinoLog.h>
#include <DmxListener.h>
#include <Things.h>

class WaveEffectFactory : public AnimationFactory {
private:

public:
    std::string getType() const override {
        return "wave-effect";
    }

    bool createAnimation(Scheduler* scheduler, const std::string& config, DmxListener* dmxListener) override {
        try {
            WaveEffectCfg cfg = WaveEffectCfg::deserialize(config);
            
            Thing* thing = dmxListener->getThing(String(cfg.rgbStripName.c_str()));
            if (thing == nullptr) {
                Log.errorln("RGB thing '%s' not found", cfg.rgbStripName.c_str());
                return false;
            }
            
            RgbThingGroup* rgbThingGroup = static_cast<RgbThingGroup*>(thing);
            
            if (rgbThingGroup->things().empty()) {
                Log.errorln("RGB thing '%s' is empty", cfg.rgbStripName.c_str());
                return false;
            }
            
            // Get all lines from the thing group for wave animation
            std::vector<RgbThing*> lines = rgbThingGroup->things();
            
            WaveEffect* waveEffect = new WaveEffect(
                lines, 
                cfg.maxFadeTime,
                cfg.dimmable
            );
            waveEffect->setName(String("WA ") + String(cfg.rgbStripName.c_str()));
            waveEffect->setMaxFadeTime(cfg.maxFadeTime);
            
            dmxListener->removeMappingForThing(rgbThingGroup->getName());
            dmxListener->addMapping(waveEffect, cfg.dmxCfg);

            // waveEffects.push_back(waveEffect);
            waveEffect->schedule(scheduler);
            waveEffect->restart();

            Log.noticeln("Created wave effect for RGB thing '%s' with %d lines", 
                        cfg.rgbStripName.c_str(), lines.size());
            
            return true;
            
        } catch (...) {
            Log.errorln("Failed to create wave effect");
            return false;
        }
    }
};
