#include <waveAnimation.h>
#include <pluginFactory.h>
#include <ArduinoLog.h>
#include <DmxListener.h>
#include <Things.h>

class WaveAnimationFactory : public AnimationFactory {
private:
    static std::vector<WaveAnimation*> waveAnimations;

public:
    std::string getType() const override {
        return "wave-animation";
    }

    bool createAnimation(Scheduler* scheduler, const std::string& config, DmxListener* dmxListener) override {
        try {
            WaveAnimationCfg cfg = WaveAnimationCfg::deserialize(config);
            
            RgbThingGroup* rgbThingGroup = dynamic_cast<RgbThingGroup*>(
                dmxListener->getThing(String(cfg.rgbStripName.c_str()))
            );
            
            if (rgbThingGroup == nullptr || rgbThingGroup->things().empty()) {
                Log.errorln("RGB thing '%s' not found or empty", cfg.rgbStripName.c_str());
                return false;
            }
            
            // Get all lines from the thing group for wave animation
            std::vector<RgbThing*> lines = rgbThingGroup->things();
            
            WaveAnimation* animation = new WaveAnimation(
                scheduler, 
                lines, 
                cfg.maxFadeTime,
                cfg.dimmable
            );
            animation->setMaxFadeTime(cfg.maxFadeTime);
            
            dmxListener->removeMappingForThing(rgbThingGroup->getName());
            dmxListener->addMapping(animation, cfg.dmxCfg);

            waveAnimations.push_back(animation);
            animation->restart();

            Log.noticeln("Created wave animation for RGB thing '%s' with %d lines", 
                        cfg.rgbStripName.c_str(), lines.size());
            
            return true;
            
        } catch (...) {
            Log.errorln("Failed to create wave animation");
            return false;
        }
    }
    
    static const std::vector<WaveAnimation*>& getAnimations() {
        return waveAnimations;
    }
    
    static void cleanup() {
        for (auto* animation : waveAnimations) {
            delete animation;
        }
        waveAnimations.clear();
    }
};

std::vector<WaveAnimation*> WaveAnimationFactory::waveAnimations;

REGISTER_ANIMATION_FACTORY(WaveAnimationFactory);
