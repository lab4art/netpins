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
    static std::vector<PWMFadeAnimationThing*> pwmFadeAnimations;

public:
    std::string getType() const override {
        return "pwm-fade";
    }

    bool createAnimation(Scheduler* scheduler, const std::string& config, DmxListener* dmxListener) override {
        try {
            // Parse JSON config to get PwmFadeCfg-like structure
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, config);
            if (error) {
                Log.errorln("Failed to parse PWM fade config JSON");
                return false;
            }
            
            JsonObject json = doc.as<JsonObject>();
            std::string name = json["name"].as<std::string>();
            std::string pwmName = json["pwm_name"].as<std::string>();
            DmxCfg dmxCfg;
            if (json.containsKey("dmx")) {
                dmxCfg = DmxCfg::deserialize(json["dmx"].as<std::string>());
            }
            
            // Get the global PWM list
            std::vector<PwmThing*>* globalPwms = getGlobalPwmList();
            if (globalPwms == nullptr) {
                Log.errorln("Global PWM list not available for PWM fade factory");
                return false;
            }
            
            // Find the PWM thing by name
            PwmThing* pwm = findPwmThing(*globalPwms, String(pwmName.c_str()));
            if (pwm == nullptr) {
                Log.errorln("PWM thing '%s' not found for PWM fade", pwmName.c_str());
                return false;
            }
            
            // Create the PWM fade animation
            PWMFadeAnimationThing* pwmFade = new PWMFadeAnimationThing(
                scheduler, 
                pwm,
                String(name.c_str())
            );
            
            // Remove the original PWM mapping and add the fade animation mapping
            dmxListener->removeMappingForThing(String(pwmName.c_str()));
            dmxListener->addMapping(pwmFade, dmxCfg);
            
            // Store the animation
            pwmFadeAnimations.push_back(pwmFade);
            
            Log.infoln("Created PWM fade animation: %s for PWM: %s", name.c_str(), pwmName.c_str());
            return true;
            
        } catch (...) {
            Log.errorln("Failed to create PWM fade animation");
            return false;
        }
    }
    
    static const std::vector<PWMFadeAnimationThing*>& getAnimations() {
        return pwmFadeAnimations;
    }
    
    static void cleanup() {
        for (auto* animation : pwmFadeAnimations) {
            delete animation;
        }
        pwmFadeAnimations.clear();
    }
};

std::vector<PWMFadeAnimationThing*> PWMFadeAnimationFactory::pwmFadeAnimations;

REGISTER_ANIMATION_FACTORY(PWMFadeAnimationFactory);

// Force linker to include this file
extern "C" void __pwmFadeAnimationFactory_init() {}