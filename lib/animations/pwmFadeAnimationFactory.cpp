#include <animations.h>
#include <pluginFactory.h>
#include <Log.h>
#include <DmxManager.h>
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

    bool createAnimation(Scheduler* scheduler, const std::string& config, DmxManager* dmxManager) override {
        try {
            PwmFadeCfg cfg = PwmFadeCfg::deserialize(config);

            Thing* thing = dmxManager->getThing(cfg.pwmName);
            if (thing == nullptr) {
                Log::errorln("PWM thing '%s' not found", cfg.pwmName.c_str());
                return false;
            }
            
            PwmThing* pwmThing = static_cast<PwmThing*>(thing);
            
            PWMFadeAnimation* fadeAnimation = new PWMFadeAnimation(
                pwmThing,
                cfg.maxFadeDuration);

            dmxManager->removeMappingForThing(cfg.pwmName);

            PWMFadeAnimationThing* pwmFadeThing = new PWMFadeAnimationThing(
                fadeAnimation);
            
            dmxManager->addMapping(pwmFadeThing, cfg.dmxCfg);
            
            fadeAnimation->schedule(scheduler);

            return true;
        } catch (...) {
            Log::errorln("Failed to create PWM fade animation");
            return false;
        }
    }
};
