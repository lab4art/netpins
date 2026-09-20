#pragma once

#include <Arduino.h>
#include <FastAccelStepper.h>

#include <Things.h>
#include <settings.h>

class StepperThing : public Thing {
private:
    FastAccelStepperEngine& engine;
    FastAccelStepper* stepper;
    StepperCfg cfg;

    uint8_t lastSpeedValue;
    uint8_t lastDirectionValue;
    bool lastDirectionForward;
    bool initialized;

    uint32_t mapSpeedToHz(uint8_t value) const;
    bool computeForward(uint8_t directionValue) const;
    void applyMotion();

public:
    StepperThing(FastAccelStepperEngine& engine, const StepperCfg& cfg);
    ~StepperThing();

    bool begin();
    void stop();

    int numChannels() override;
    void setData(uint8_t* data) override;
};