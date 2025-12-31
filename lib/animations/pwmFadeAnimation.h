#pragma once

#include <Things.h>
#include <vector>
#include <animations.h>
#include <string>
#include <netpinsCommons.h>

struct PwmFadeCfg {
    std::string pwmName;
    DmxCfg dmxCfg;
    std::uint16_t maxFadeDuration = 10000; // ms, maximum fade time

    bool operator==(const PwmFadeCfg& other) const {
        return pwmName == other.pwmName &&
            dmxCfg == other.dmxCfg &&
            maxFadeDuration == other.maxFadeDuration;
    };

    bool operator!=(const PwmFadeCfg& other) const {
        return !(*this == other);
    };

    static PwmFadeCfg deserialize(std::string jsonString) {
        PwmFadeCfg p;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        if (error) {
            Log::error("Failed to deserialize PwmFadeCfg");
            return p; // Return default config on error
        }
        JsonObject json = doc.as<JsonObject>();
        p.pwmName = json["pwm_name"].as<std::string>();
        if (json.containsKey("dmx")) {
            p.dmxCfg = DmxCfg::deserialize(json["dmx"].as<std::string>());
        }
        p.maxFadeDuration = json["max_fade_duration"].as<std::uint16_t>();
        return p;
    };

    static void serialize(JsonObject& json, const PwmFadeCfg& p) {
        json["pwm_name"] = p.pwmName;
        json["dmx"] = DmxCfg::serialize(p.dmxCfg);
        json["max_fade_duration"] = p.maxFadeDuration;
    };
};

class PWMFadeAnimation: public Animation {
    private:
        PwmThing* pwmThing;
        uint8_t value1; // value to fade from (off)
        uint8_t value2; // value to fade to (on)
        uint16_t maxFadeDuration;
        uint16_t fadeInDuration;
        uint16_t fadeOutDuration;

        boolean fadeInMode = false; // if false, fadeOut

        uint8_t getCurrentValue() {
            if (fadeInMode) {
                return NetpinsCommons::linearBlend(value1, value2, getProgress());
            } else {
                return NetpinsCommons::linearBlend(value2, value1, getProgress());
            }
        }

    public:
        PWMFadeAnimation(
                PwmThing* pwmThing,
                uint16_t maxFadeDuration):
                Animation(false),
                pwmThing(pwmThing),
                maxFadeDuration(maxFadeDuration) {
            // setName(std::string("PWM FA for ") + pwmThing->getName().c_str());
            setName(std::string("PWM FA for "));
            setDuration(maxFadeDuration);
            Log::traceln("Created PWM fade animation for PWM thing '%s' with max fade duration: %d", pwmThing->getName().c_str(), maxFadeDuration);
        }

        void animate() {
            uint8_t data[1] = {getCurrentValue()};
            // Log.traceln("Setting PWM fade animation data: %d", data[0]);
            pwmThing->setData(data);
        }

        void setValue1(uint8_t value) {
            value1 = value;
        }

        void setValue2(uint8_t value) {
            value2 = value;
        }

        void setFadeInDuration(uint8_t fInDuration) {
            this->fadeInDuration = fInDuration * this->maxFadeDuration / 255;
        }

        void setFadeOutDuration(uint8_t fOutDuration) {
            this->fadeOutDuration = fOutDuration * this->maxFadeDuration / 255;
        }

        void fadeIn() {
            if (!isRunning()) {
                // Log.traceln("Fresh FadeIn to: %d, duration: %d, progress: %s.", this->value2, fadeInDuration, String(getProgress(), 4));
                this->fadeInMode = true;
                setDuration(fadeInDuration);
                restart();
            } else if (!fadeInMode) {
                // Log.traceln("Middle FadeIn to: %d, duration: %d, progress: %s.", this->value2, fadeInDuration, String(getProgress(), 4));
                float currentProgress = getProgress();
                this->fadeInMode = true;
                setDuration(fadeInDuration * currentProgress);
                restart(1 - currentProgress);
            }
        }
        void fadeOut() {
            if (!isRunning()) {
                // Log.traceln("Fresh FadeOut to: %d, duration: %d, progress: %s.", this->value1, fadeOutDuration, String(getProgress(), 4));
                this->fadeInMode = false;
                setDuration(fadeOutDuration);
                restart();
            } else if (fadeInMode) {
                // Log.traceln("Middle FadeOut to: %d, duration: %d, progress: %s.", this->value1, fadeOutDuration, String(getProgress(), 4));
                float currentProgress = getProgress();
                this->fadeInMode = false;
                setDuration(fadeOutDuration * currentProgress); // TODO 1 - currentProgress ??
                restart(1 - currentProgress);
            }
        }

        void toggleFade() {
            if (fadeInMode) {
                fadeOut();
            } else {
                fadeIn();
            }
        }
};

class PWMFadeAnimationThing: public Thing {
    private:
        PWMFadeAnimation* fadeAnimation;
        uint8_t lastDmxData[5] = {0, 0, 0, 0, 0}; // value1, value2, fadeInDuration, fadeOutDuration, on/off
    
        boolean setLastDmxData(uint8_t* data) {
            bool changed = false;
            for (int i = 0; i < 5; i++) {
                if (lastDmxData[i] != data[i]) {
                    lastDmxData[i] = data[i];
                    changed = true;
                }
            }
            return changed;
        }

    public:
        PWMFadeAnimationThing(
                PWMFadeAnimation* fadeAnimation):
                fadeAnimation(fadeAnimation) {
        }

        int numChannels() {
            return 5;
        }

        void setData(uint8_t* data) {
            boolean onOffChanged = lastDmxData[4] != data[4];
            if (setLastDmxData(data)) {
                Log::traceln("Setting PWM fade animation data: %d %d %d %d %d", data[0], data[1], data[2], data[3], data[4]);
                fadeAnimation->setValue1(data[0]);
                fadeAnimation->setValue2(data[1]);
                fadeAnimation->setFadeInDuration(data[2]);
                fadeAnimation->setFadeOutDuration(data[3]);
                // trigger fade only if data[4] (on/off) changed
                if (onOffChanged) { // TODO apply new fade data although on/off does not change
                    if (data[4] > 0) {
                        fadeAnimation->fadeIn();
                    } else {
                        fadeAnimation->fadeOut();
                    }
                }
            }
        }
};
