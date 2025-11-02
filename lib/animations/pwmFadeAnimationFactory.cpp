#include <animations.h>
#include <pluginFactory.h>
#include <ArduinoLog.h>
#include <DmxListener.h>
#include <Things.h>
#include <pwmFadeAnimation.h>

// Forward declaration of external PWM access function
extern std::vector<PwmThing*>* getGlobalPwmList();

class PWMFadeAnimationFactory : public AnimationFactory {
private:

public:
    std::string getType() const override {
        return "pwm-fade";
    }

    bool createAnimation(Scheduler* scheduler, const std::string& config, DmxListener* dmxListener) override {
        try {
            PwmFadeCfg cfg = PwmFadeCfg::deserialize(config);

            Thing* thing = dmxListener->getThing(String(cfg.pwmName.c_str()));
            if (thing == nullptr) {
                Log.errorln("PWM thing '%s' not found", cfg.pwmName.c_str());
                return false;
            }
            
            PwmThing* pwmThing = static_cast<PwmThing*>(thing);
            
            PWMFadeAnimation* fadeAnimation = new PWMFadeAnimation(
                pwmThing,
                cfg.maxFadeDuration);

            dmxListener->removeMappingForThing(String(cfg.pwmName.c_str()));

            PWMFadeAnimationThing* pwmFadeThing = new PWMFadeAnimationThing(
                fadeAnimation);
            
            dmxListener->addMapping(pwmFadeThing, cfg.dmxCfg);
            
            fadeAnimation->schedule(scheduler);

            return true;
        } catch (...) {
            Log.errorln("Failed to create PWM fade animation");
            return false;
        }
    }
};
