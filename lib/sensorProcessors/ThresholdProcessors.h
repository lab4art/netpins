#pragma once

#include "SensorProcessor.h"

/**
 * Simple threshold processor - outputs high/low value based on threshold
 * Example: Temperature > 30°C -> 255, otherwise 0
 */
class ThresholdProcessor : public SensorProcessor {
    private:
        float threshold;
        float highValue;
        float lowValue;
        bool aboveThreshold;  // true = trigger when above, false = trigger when below
        
    public:
        ThresholdProcessor(float thresh, float high = 255.0f, float low = 0.0f, bool above = true)
            : SensorProcessor("Threshold"),
              threshold(thresh),
              highValue(high),
              lowValue(low),
              aboveThreshold(above) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            if (aboveThreshold) {
                result.value = (data.value > threshold) ? highValue : lowValue;
            } else {
                result.value = (data.value < threshold) ? highValue : lowValue;
            }
            
            return result;
        }
};

/**
 * Threshold with hysteresis - prevents oscillation near threshold
 * Example: Turn on at 30°C, turn off at 28°C (2° hysteresis)
 */
class HysteresisProcessor : public SensorProcessor {
    private:
        float upperThreshold;
        float lowerThreshold;
        float highValue;
        float lowValue;
        bool currentState;  // true = high, false = low
        bool initialized;
        
    public:
        HysteresisProcessor(float upper, float lower, float high = 255.0f, float low = 0.0f)
            : SensorProcessor("Hysteresis"),
              upperThreshold(upper),
              lowerThreshold(lower),
              highValue(high),
              lowValue(low),
              currentState(false),
              initialized(false) {
            
            if (lower >= upper) {
                Log::errorln("Hysteresis: Lower threshold must be < upper threshold");
            }
        }
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            // Initialize state on first reading
            if (!initialized) {
                currentState = (data.value > upperThreshold);
                initialized = true;
            }
            
            // Update state based on hysteresis
            if (data.value > upperThreshold) {
                currentState = true;
            } else if (data.value < lowerThreshold) {
                currentState = false;
            }
            // Between thresholds - maintain current state
            
            result.value = currentState ? highValue : lowValue;
            return result;
        }
        
        void reset() override {
            initialized = false;
            currentState = false;
        }
};

/**
 * Time-based threshold - value must exceed threshold for specified duration
 * Example: Temperature must be > 30°C for 60 seconds to trigger
 */
class TimeBasedThresholdProcessor : public SensorProcessor {
    private:
        float threshold;
        unsigned long requiredDurationMs;
        float highValue;
        float lowValue;
        bool aboveThreshold;
        unsigned long thresholdStartTime;
        bool isAboveThreshold;
        bool isTriggered;
        
    public:
        TimeBasedThresholdProcessor(float thresh, unsigned long durationMs, 
                                   float high = 255.0f, float low = 0.0f, bool above = true)
            : SensorProcessor("TimeBasedThreshold"),
              threshold(thresh),
              requiredDurationMs(durationMs),
              highValue(high),
              lowValue(low),
              aboveThreshold(above),
              thresholdStartTime(0),
              isAboveThreshold(false),
              isTriggered(false) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            bool conditionMet = aboveThreshold ? (data.value > threshold) : (data.value < threshold);
            
            if (conditionMet) {
                if (!isAboveThreshold) {
                    // Condition just became true
                    thresholdStartTime = data.timestamp;
                    isAboveThreshold = true;
                }
                
                // Check if duration has elapsed
                if (data.timestamp - thresholdStartTime >= requiredDurationMs) {
                    isTriggered = true;
                }
            } else {
                // Condition not met - reset
                isAboveThreshold = false;
                isTriggered = false;
            }
            
            result.value = isTriggered ? highValue : lowValue;
            return result;
        }
        
        void reset() override {
            isAboveThreshold = false;
            isTriggered = false;
            thresholdStartTime = 0;
        }
};

/**
 * Range threshold - triggers when value is within a range
 * Example: Temperature between 20-25°C triggers high value
 */
class RangeThresholdProcessor : public SensorProcessor {
    private:
        float minThreshold;
        float maxThreshold;
        float insideValue;
        float outsideValue;
        
    public:
        RangeThresholdProcessor(float min, float max, float inside = 255.0f, float outside = 0.0f)
            : SensorProcessor("RangeThreshold"),
              minThreshold(min),
              maxThreshold(max),
              insideValue(inside),
              outsideValue(outside) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            bool inRange = (data.value >= minThreshold && data.value <= maxThreshold);
            result.value = inRange ? insideValue : outsideValue;
            return result;
        }
};

/**
 * Band-pass filter - only passes values within specified range
 * Values outside range are clamped to range boundaries
 */
class BandPassProcessor : public SensorProcessor {
    private:
        float minValue;
        float maxValue;
        float defaultValue;
        
    public:
        BandPassProcessor(float min, float max, float defaultVal = 0.0f)
            : SensorProcessor("BandPass"),
              minValue(min),
              maxValue(max),
              defaultValue(defaultVal) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            if (data.value < minValue || data.value > maxValue) {
                result.value = defaultValue;
            } else {
                result.value = data.value;
            }
            
            return result;
        }
};

/**
 * Dead zone processor - ignores values within specified range around zero/center
 * Useful for joysticks, analog sensors with noise around center
 */
class DeadZoneProcessor : public SensorProcessor {
    private:
        float centerValue;
        float deadZoneRadius;
        
    public:
        DeadZoneProcessor(float center = 0.0f, float radius = 5.0f)
            : SensorProcessor("DeadZone"),
              centerValue(center),
              deadZoneRadius(radius) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            float distance = abs(data.value - centerValue);
            
            if (distance < deadZoneRadius) {
                result.value = centerValue;
            } else {
                result.value = data.value;
            }
            
            return result;
        }
};

/**
 * Toggle processor - flips latched output on each input edge
 * Useful for momentary push buttons where each press toggles ON/OFF state.
 */
class ToggleProcessor : public SensorProcessor {
    private:
        float edgeThreshold;
        float onValue;
        float offValue;
        bool initialOn;
        bool toggleOnRising;

        bool latchedOn;
        bool hasPreviousInput;
        bool previousInputHigh;

    public:
        ToggleProcessor(float threshold = 0.5f,
                        float onVal = 255.0f,
                        float offVal = 0.0f,
                        bool initialStateOn = false,
                        bool onRising = true)
            : SensorProcessor("Toggle"),
              edgeThreshold(threshold),
              onValue(onVal),
              offValue(offVal),
              initialOn(initialStateOn),
              toggleOnRising(onRising),
              latchedOn(initialStateOn),
              hasPreviousInput(false),
              previousInputHigh(false) {}

        SensorData process(const SensorData& data) override {
            if (!enabled) return data;

            SensorData result = data;
            bool inputHigh = data.value > edgeThreshold;

            if (!hasPreviousInput) {
                hasPreviousInput = true;
                previousInputHigh = inputHigh;
                result.value = latchedOn ? onValue : offValue;
                return result;
            }

            bool edgeDetected = toggleOnRising
                ? (!previousInputHigh && inputHigh)
                : (previousInputHigh && !inputHigh);

            if (edgeDetected) {
                latchedOn = !latchedOn;
            }

            previousInputHigh = inputHigh;
            result.value = latchedOn ? onValue : offValue;
            return result;
        }

        void reset() override {
            latchedOn = initialOn;
            hasPreviousInput = false;
            previousInputHigh = false;
        }
};

