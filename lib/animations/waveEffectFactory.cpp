#include <waveEffect.h>
#include <pluginFactory.h>
#include <Log.h>
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
            
            Thing* thing = dmxListener->getThing(cfg.rgbStripName);
            if (thing == nullptr) {
                Log::error((std::string("RGB thing '") + cfg.rgbStripName + "' not found").c_str());
                return false;
            }
            
            RgbThingGroup* rgbThingGroup = static_cast<RgbThingGroup*>(thing);
            
            if (rgbThingGroup->things().empty()) {
                Log::error((std::string("RGB thing '") + cfg.rgbStripName + "' is empty").c_str());
                return false;
            }
            
            // Get all lines from the thing group for wave animation
            std::vector<RgbThing*> lines = rgbThingGroup->things();
            
            WaveEffect* waveEffect = new WaveEffect(
                lines, 
                cfg.maxFadeTime,
                cfg.dimmable
            );
            waveEffect->setName("WA " + cfg.rgbStripName);
            waveEffect->setMaxFadeTime(cfg.maxFadeTime);
            
            dmxListener->removeMappingForThing(rgbThingGroup->getName());
            dmxListener->addMapping(waveEffect, cfg.dmxCfg);

            // waveEffects.push_back(waveEffect);
            waveEffect->schedule(scheduler);
            waveEffect->restart();

            Log::info((std::string("Created wave effect for RGB thing '") + cfg.rgbStripName + "' with " + std::to_string(lines.size()) + " lines").c_str());
            
            return true;
            
        } catch (...) {
            Log::error("Failed to create wave effect");
            return false;
        }
    }
};
