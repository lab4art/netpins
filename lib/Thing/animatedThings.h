#pragma once

#include <Things.h>
#include <animations.h>
#include <settings.h>

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
                Scheduler* aScheduler, 
                PwmThing* led, 
                String name) {
            fadeAnimation = new PWMFadeAnimation(
                aScheduler, 
                led);
            setName(name);
        }

        int numChannels() {
            return 5;
        }

        void setData(uint8_t* data) {
            boolean onOffChanged = lastDmxData[4] != data[4];
            if (setLastDmxData(data)) {
                // Log.traceln("Setting PWM fade animation data: %d %d %d %d %d", data[0], data[1], data[2], data[3], data[4]);
                fadeAnimation->setValue1(data[0]);
                fadeAnimation->setValue2(data[1]);
                // trigger fade only if data[4] (on/off) changed
                if (onOffChanged) { // TODO apply new fade data although on/off does not change
                    if (data[4] > 0) {
                        fadeAnimation->fadeIn(data[2] * 100);
                    } else {
                        fadeAnimation->fadeOut(data[3] * 100);
                    }
                }
            }
        }
};

PWMFadeAnimationThing* findPwmFadeAnimationThing(std::vector<PWMFadeAnimationThing*> pwmFades, String name) {
    for (auto pwmFade : pwmFades) {
        if (pwmFade->getName().equals(name)) {
            return pwmFade;
        }
    }
    return nullptr;
};
