#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoLog.h>

class FactoryReset {
    private:
        Preferences preferences;
        bool couterReseted = false;
        bool doReset = false;

    public:
        FactoryReset() {
            preferences.begin("sys", false);
            int rebootCount = preferences.getInt("rst-cnt", 0); // factory reset counter
            rebootCount++;
            Log.noticeln("FactoryReset counter: %d", rebootCount);
            if (rebootCount >= 5) {
                doReset = true;
                preferences.putInt("cnt", 0);
            } else {
                preferences.putInt("cnt", rebootCount);
            }
            preferences.end();
        }

        bool resetCounter(boolean force = false) {
            if (!couterReseted && (force || millis() > 8000)) {
                Log.noticeln("Resetting factoryReset counter ...");
                preferences.begin("sys", false);
                preferences.putUInt("rst-cnt", 0);
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
