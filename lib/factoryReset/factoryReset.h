#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoLog.h>

/**
 * FactoryReset singleton class
 */
class FactoryReset {
    private:
        Preferences preferences;
        bool couterReseted = false;
        bool doReset = false;
        uint8_t secondaryCounterInitalValue = 2;
        bool usingButton = false;

        FactoryReset() {}

    public:
        // Delete copy constructor and assignment operator
        FactoryReset(const FactoryReset&) = delete;
        FactoryReset& operator=(const FactoryReset&) = delete;
        
        static FactoryReset& getInstance() {
            static FactoryReset instance;
            return instance;
        }
        
        /**
         * Evaluate the factory reset button or power cycle.
         * The button is evaluated only if the pin set to a value >= 0, otherwise the power cycle is evaluated.
         */
        void evaluate(int pin = -1) {
            if (pin >= 0) {
                usingButton = true;
                evaluateButton(pin);
            } else {
                evaluatePowerCycle();
            }
        }
        
        void evaluatePowerCycle() {
            preferences.begin("sys", false);
            int rebootCount = preferences.getInt("rst-cnt", 0); // factory reset counter
            int rebootCount2 = preferences.getInt("rst-cnt-2", secondaryCounterInitalValue); // factory reset counter
            rebootCount++;
            Log.noticeln("FactoryReset counter: %d, 2nd counter %d.", rebootCount, rebootCount2);
            // save the counter before the delay
            preferences.putInt("rst-cnt", rebootCount);
            preferences.end();
            Log.traceln("FactoryReset counter: %d, 2nd counter %d.", rebootCount, rebootCount2);
            // prevent factory reset in case of immediate reboots
            if (rebootCount >= secondaryCounterInitalValue) {
                Log.traceln("Waiting for 5 sec ...");
                delay(5000);

                /**
                    Validate primary counter, reset both if immediate reboot happend.
                    Immediate reboot is identified by the difference in counters,
                    if reboot hapens in less than N sec, primary counter takes a lead.
                    NOTE: accidental reboot might happen during the lights test (after this evaluation), due to hight energy consumption.             
                */
                if (rebootCount - rebootCount2 > 0) {
                    rebootCount = 0;
                    rebootCount2 = secondaryCounterInitalValue;
                    preferences.begin("sys", false);
                    preferences.putInt("rst-cnt", 0);
                    preferences.putInt("rst-cnt-2", secondaryCounterInitalValue);
                    preferences.end();
                    return;
                }
                rebootCount2++;
            }

            preferences.begin("sys", false);
            Log.traceln("Should reset counter: %d, 2nd counter %d.", rebootCount, rebootCount2);
            if (rebootCount >= 5) {
                Log.noticeln("Factory reset flagged ...");
                doReset = true;
                preferences.putInt("rst-cnt", 0);
                preferences.putInt("rst-cnt-2", secondaryCounterInitalValue);
            } else {
                preferences.putInt("rst-cnt-2", rebootCount2);
            }
            preferences.end();
        }

        void evaluateButton(int pin) {
            pinMode(pin, INPUT_PULLUP);
            // Leave place to avoid bootloader mode, when pin 0 is used.
            // Button can be pressed 1 sec after the power up.
            delay(1500);

            // check if the factory reset button is pressed
            if (digitalRead(pin) == LOW) {
                delay(1000);
                if (digitalRead(pin) == LOW) {
                    Log.noticeln("Factory reset button pressed ...");
                    doReset = true;
                }
            }
        }

        /**
         * Reset factoryReset counter if the time is greater than a threshold.
         * Returns true if the counter was reset.
         */
        bool resetCounter(boolean force = false) {
            if (!usingButton && !couterReseted && (force || millis() > 10000)) {
                Log.noticeln("Resetting factoryReset counter ...");
                preferences.begin("sys", false);
                preferences.putInt("rst-cnt", 0);
                preferences.putInt("rst-cnt-2", secondaryCounterInitalValue);
                preferences.end();
                couterReseted = true;
                return true;
            } else {
                return false;
            }
        }

        bool shouldReset() {
            return doReset;
        }
};
