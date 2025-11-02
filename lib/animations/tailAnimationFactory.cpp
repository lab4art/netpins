#include <tailAnimation.h>
#include <pluginFactory.h>
#include <ArduinoLog.h>
#include <DmxListener.h>
#include <Things.h>

class TailAnimationFactory : public AnimationFactory {
private:


public:
    std::string getType() const override {
        return "tail-animation";
    }

    bool createAnimation(Scheduler* scheduler, const std::string& config, DmxListener* dmxListener) override {
        try {
            TailAnimationCfg cfg = TailAnimationCfg::deserialize(config);
            
            Thing* thing = dmxListener->getThing(String(cfg.rgbStripName.c_str()));
            if (thing == nullptr) {
                Log.errorln("RGB thing '%s' not found", cfg.rgbStripName.c_str());
                return false;
            }
            
            RgbThingGroup* rgbThing = static_cast<RgbThingGroup*>(thing);
            
            if (rgbThing->things().empty()) {
                Log.errorln("RGB thing '%s' is empty", cfg.rgbStripName.c_str());
                return false;
            }
            
            RgbThing* line = rgbThing->things().front();
            
            TailAnimation* animation = new TailAnimation(line, cfg.direction, true);
            animation->setName(std::string("TA ") + cfg.rgbStripName.c_str());
            animation->setDuration(cfg.maxDuration);
            
            dmxListener->removeMappingForThing(rgbThing->getName());
            TailAnimationThing* tailAnimationThing = new TailAnimationThing(animation, cfg.maxDuration);
            tailAnimationThing->setName(String("TA Thing ") + (cfg.rgbStripName).c_str());
            dmxListener->addMapping(tailAnimationThing, cfg.dmxCfg);
            
            animation->schedule(scheduler);
            animation->restart();
            
            return true;
        } catch (...) {
            Log.errorln("Failed to create tail animation");
            return false;
        }
    }
};
