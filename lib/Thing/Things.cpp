#include "Things.h"

uint16_t LedThing::gammaTable[256];

LedThing* findLedThing(std::vector<LedThing*> leds, int pin) {
    for (auto led : leds) {
        if (led->getPin() == pin) {
            return led;
        }
    }
    return nullptr;
}
