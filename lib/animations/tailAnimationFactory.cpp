#include <tailAnimation.h>
#include <pluginFactory.h>
#include <Log.h>
#include <DmxManager.h>
#include <Things.h>

class TailAnimationFactory : public AnimationFactory {
private:


public:
    std::string getType() const override {
        return "tail-animation";
    }

    bool createAnimation(Scheduler* scheduler, const std::string& config, DmxManager* dmxManager) override {
        try {
            TailAnimationCfg cfg = TailAnimationCfg::deserialize(config);
            
            Thing* thing = dmxManager->getThing(cfg.rgbStripName);
            if (thing == nullptr) {
                Log::error((std::string("RGB thing '") + cfg.rgbStripName + "' not found").c_str());
                return false;
            }
            
            RgbThingGroup* rgbThing = static_cast<RgbThingGroup*>(thing);
            
            if (rgbThing->things().empty()) {
                Log::error((std::string("RGB thing '") + cfg.rgbStripName + "' is empty").c_str());
                return false;
            }
            
            RgbThing* line = rgbThing->things().front();
            
            TailAnimation* animation = new TailAnimation(line, cfg.direction, true);
            animation->setName(std::string("TA ") + cfg.rgbStripName.c_str());
            animation->setDuration(cfg.maxDuration);
            
            dmxManager->removeMappingForThing(rgbThing->getName());
            
            TailAnimationThing* tailAnimationThing = new TailAnimationThing(animation, cfg.maxDuration);
            tailAnimationThing->setName("TA Thing " + cfg.rgbStripName);
            dmxManager->addMapping(tailAnimationThing, cfg.dmxCfg);
            
            animation->schedule(scheduler);
            animation->restart();
            
            return true;
        } catch (...) {
            Log::error("Failed to create tail animation");
            return false;
        }
    }
};
