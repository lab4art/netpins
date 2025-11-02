#include <tailAnimation.h>
#include <pluginFactory.h>
#include <ArduinoLog.h>
#include <DmxListener.h>
#include <Things.h>

class TailAnimationFactory : public AnimationFactory {
private:
    static std::vector<TailAnimation*> tailAnimations;

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
            
            TailAnimation* animation = new TailAnimation(scheduler, line, cfg.direction, true);
            animation->setColor1(cfg.color1);
            animation->setColor2(cfg.color2);
            animation->setDimm(cfg.dimm);
            animation->setDuration(cfg.duration);
            animation->setTailLength(cfg.tailLength);
            animation->setHeadLength(cfg.headLength);
            
            dmxListener->removeMappingForThing(rgbThing->getName());
            tailAnimations.push_back(animation);
            animation->restart();
            
            return true;
            
        } catch (...) {
            Log.errorln("Failed to create tail animation");
            return false;
        }
    }
    
    static const std::vector<TailAnimation*>& getAnimations() {
        return tailAnimations;
    }
    
    static void cleanup() {
        for (auto* animation : tailAnimations) {
            delete animation;
        }
        tailAnimations.clear();
    }
};

std::vector<TailAnimation*> TailAnimationFactory::tailAnimations;

REGISTER_ANIMATION_FACTORY(TailAnimationFactory);

// Force linker to include this file
extern "C" void __tailAnimationFactory_init() {}
