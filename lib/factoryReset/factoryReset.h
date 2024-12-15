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
            preferences.begin("f-rst", false);
            int rebootCount = preferences.getInt("cnt", 0);
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
                preferences.begin("f-rst", false);
                preferences.putInt("cnt", 0);
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
