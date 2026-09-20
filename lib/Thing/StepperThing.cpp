#include "StepperThing.h"

#include <Log.h>

namespace {
constexpr bool kDirectionHighCountsUp = true;
constexpr bool kEnableActiveLow = true;
constexpr uint8_t kDirectionThreshold = 128;
constexpr uint32_t kDirectionChangeDelayUs = 0;
constexpr uint32_t kDelayToEnableUs = 50;
constexpr uint16_t kDelayToDisableMs = 50;
constexpr uint32_t kLinearAccelerationSteps = 0;
constexpr uint32_t kJumpStartSteps = 0;
}

StepperThing::StepperThing(FastAccelStepperEngine& engine, const StepperCfg& cfg)
    : engine(engine),
      stepper(nullptr),
      cfg(cfg),
      lastSpeedValue(0),
      lastDirectionValue(0),
      lastDirectionForward(true),
      initialized(false) {
    name = cfg.name;
}

StepperThing::~StepperThing() {
    stop();
}

bool StepperThing::begin() {
    if (cfg.stepPin == 255) {
        Log::errorln("Stepper '%s': missing step pin", name.c_str());
        return false;
    }

    stepper = engine.stepperConnectToPin(cfg.stepPin);
    if (stepper == nullptr) {
        Log::errorln("Stepper '%s': failed to connect to step pin %d", name.c_str(), cfg.stepPin);
        return false;
    }

    if (cfg.dirPin != 255) {
        stepper->setDirectionPin(cfg.dirPin, kDirectionHighCountsUp, kDirectionChangeDelayUs);
    }

    if (cfg.enablePin != 255) {
        stepper->setEnablePin(cfg.enablePin, kEnableActiveLow);
    }

    stepper->setAutoEnable(cfg.autoEnable);

    if (kDelayToEnableUs > 0) {
        stepper->setDelayToEnable(kDelayToEnableUs);
    }
    if (kDelayToDisableMs > 0) {
        stepper->setDelayToDisable(kDelayToDisableMs);
    }

    if (cfg.maxSpeedHz > 0) {
        stepper->setSpeedInHz(cfg.maxSpeedHz);
    }
    if (cfg.acceleration > 0) {
        stepper->setAcceleration(cfg.acceleration);
    }
    if (kLinearAccelerationSteps > 0) {
        stepper->setLinearAcceleration(kLinearAccelerationSteps);
    }
    if (kJumpStartSteps > 0) {
        stepper->setJumpStart(kJumpStartSteps);
    }

    stepper->setCurrentPosition(0);
    initialized = true;
    Log::infoln("Stepper '%s' initialized on step pin %d", name.c_str(), cfg.stepPin);
    return true;
}

void StepperThing::stop() {
    if (stepper != nullptr) {
        stepper->stopMove();
        stepper = nullptr;
    }
    initialized = false;
}

int StepperThing::numChannels() {
    return 2;
}

uint32_t StepperThing::mapSpeedToHz(uint8_t value) const {
    if (value == 0 || cfg.maxSpeedHz == 0) {
        return 0;
    }
    if (cfg.maxSpeedHz <= 1) {
        return 1;
    }
    uint32_t mapped = 1 + (static_cast<uint32_t>(value) * (cfg.maxSpeedHz - 1)) / 255;
    if (mapped > cfg.maxSpeedHz) {
        mapped = cfg.maxSpeedHz;
    }
    return mapped;
}

bool StepperThing::computeForward(uint8_t directionValue) const {
    return directionValue >= kDirectionThreshold;
}

void StepperThing::applyMotion() {
    if (!initialized || stepper == nullptr) {
        return;
    }

    const uint32_t targetSpeedHz = mapSpeedToHz(lastSpeedValue);
    const bool forward = computeForward(lastDirectionValue);

    if (targetSpeedHz == 0) {
        stepper->stopMove();
        return;
    }

    if (cfg.acceleration > 0) {
        stepper->setAcceleration(cfg.acceleration);
    }
    stepper->setSpeedInHz(targetSpeedHz);

    if (!stepper->isRunning()) {
        if (!cfg.autoEnable) {
            stepper->enableOutputs();
        }
        if (forward) {
            stepper->runForward();
        } else {
            stepper->runBackward();
        }
        lastDirectionForward = forward;
        return;
    }

    if (forward != lastDirectionForward) {
        if (forward) {
            stepper->runForward();
        } else {
            stepper->runBackward();
        }
        lastDirectionForward = forward;
        return;
    }

    stepper->applySpeedAcceleration();
}

void StepperThing::setData(uint8_t* data) {
    if (data == nullptr) {
        return;
    }

    const uint8_t speedValue = data[0];
    const uint8_t directionValue = data[1];
    if (speedValue == lastSpeedValue && directionValue == lastDirectionValue) {
        return;
    }

    lastSpeedValue = speedValue;
    lastDirectionValue = directionValue;
    applyMotion();
}