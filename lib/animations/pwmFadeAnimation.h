#pragma once

#include <Things.h>
#include <vector>
#include <animations.h>

class Scheduler;

class PWMFadeAnimation: public Animation {
    private:
        PwmThing* led;
        uint8_t value1; // value to fade from (off)
        uint8_t value2; // value to fade to (on)
        boolean fadeInMode = false; // if false, fadeOut

        uint8_t linearBlend(uint8_t left, uint8_t right, float progress) {
            return left + (right - left) * progress;
        }

        uint8_t getCurrentValue() {
            if (fadeInMode) {
                return linearBlend(value1, value2, getProgress());
            } else {
                return linearBlend(value2, value1, getProgress());
            }
        }

    public:
        PWMFadeAnimation(
            Scheduler* aScheduler, 
            PwmThing* led):
            Animation(aScheduler, false),
            led(led) {
        }

        void animate() {
            uint8_t data[1] = {getCurrentValue()};
            // Log.traceln("Setting PWM fade animation data: %d", data[0]);
            led->setData(data);
        }

        void setValue1(uint8_t value) {
            value1 = value;
            if (!fadeInMode && !isRunning()) {
                uint8_t data[1] = {value};
                led->setData(data);
            }
        }

        void setValue2(uint8_t value) {
            value2 = value;
            if (fadeInMode && !isRunning()) {
                uint8_t data[1] = {value};
                led->setData(data);
            }
        }

        void fadeIn(std::uint16_t fadeInDuration) {
            if (!isRunning()) {
                Log.traceln("Fresh Fade in to: %d, duration: %d, progress: %s.", this->value2, fadeInDuration, String(getProgress(), 4));
                this->fadeInMode = true;
                setDuration(fadeInDuration);
                restart();
            } else if (!fadeInMode) {
                Log.traceln("Middle Fade in to: %d, duration: %d, progress: %s.", this->value2, fadeInDuration, String(getProgress(), 4));
                float currentProgress = getProgress();
                this->fadeInMode = true;
                setDuration(fadeInDuration * getProgress());
                restart(1 - currentProgress);
            }
        }
        void fadeOut(std::uint16_t fadeOutDuration) {
            if (!isRunning()) {
                Log.traceln("Fresh Fade out to: %d, duration: %d, progress: %s.", this->value1, fadeOutDuration, String(getProgress(), 4));
                this->fadeInMode = false;
                setDuration(fadeOutDuration);
                restart();
            } else if (fadeInMode) {
                Log.traceln("Middle Fade out to: %d, duration: %d, progress: %s.", this->value1, fadeOutDuration, String(getProgress(), 4));
                float currentProgress = getProgress();
                this->fadeInMode = false;
                setDuration(fadeOutDuration);
                restart(1 - currentProgress);
            }
        }

        void togleFade(
                std::uint16_t fadeInDuration,
                std::uint16_t fadeOutDuration) {
            if (fadeInMode) {
                fadeOut(fadeOutDuration);
            } else {
                fadeIn(fadeInDuration);
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