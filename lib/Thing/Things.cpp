#include "Things.h"

uint16_t PwmThing::gammaTable[256];

PwmThing* findPwmThing(std::vector<PwmThing*> pwms, String name) {
    for (auto pwm : pwms) {
        if (pwm->getName().equals(name)) {
            return pwm;
        }
    }
    return nullptr;
}
