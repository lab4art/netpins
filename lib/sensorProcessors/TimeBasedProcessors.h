#pragma once

#include "SensorProcessor.h"

/**
 * Debounce processor - only passes value changes if they remain stable for specified duration
 * Useful for noisy sensors like mechanical switches
 */
class DebounceProcessor : public SensorProcessor {
    private:
        unsigned long debounceTimeMs;
        float pendingValue;
        unsigned long pendingStartTime;
        float stableValue;
        bool hasPendingChange;
        
    public:
        DebounceProcessor(unsigned long debounceMs)
            : SensorProcessor("Debounce"),
              debounceTimeMs(debounceMs),
              pendingValue(0),
              pendingStartTime(0),
              stableValue(0),
              hasPendingChange(false) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            // First reading - initialize
            if (!hasPendingChange && pendingStartTime == 0) {
                stableValue = data.value;
                result.value = stableValue;
                return result;
            }
            
            // Value changed - start debounce timer
            if (data.value != pendingValue) {
                pendingValue = data.value;
                pendingStartTime = data.timestamp;
                hasPendingChange = true;
            }
            
            // Check if debounce period has elapsed
            if (hasPendingChange && (data.timestamp - pendingStartTime >= debounceTimeMs)) {
                stableValue = pendingValue;
                hasPendingChange = false;
            }
            
            result.value = stableValue;
            return result;
        }
        
        void reset() override {
            hasPendingChange = false;
            pendingStartTime = 0;
        }
};

/**
 * Persistence processor - value must persist for specified duration before being passed through
 * Example: Motion sensor must detect motion for 10s before setting DMX to 255
 */
class PersistenceProcessor : public SensorProcessor {
    private:
        unsigned long persistenceTimeMs;
        float targetValue;           // Value that must persist
        float defaultValue;          // Value to output if target hasn't persisted long enough
        unsigned long targetStartTime;
        bool targetIsActive;
        bool outputActive;
        
    public:
        PersistenceProcessor(unsigned long persistMs, float target, float defaultVal = 0.0f)
            : SensorProcessor("Persistence"),
              persistenceTimeMs(persistMs),
              targetValue(target),
              defaultValue(defaultVal),
              targetStartTime(0),
              targetIsActive(false),
              outputActive(false) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            // Check if we're seeing the target value
            if (data.value == targetValue) {
                if (!targetIsActive) {
                    // Target value just started
                    targetStartTime = data.timestamp;
                    targetIsActive = true;
                }
                
                // Check if persistence time has elapsed
                if (data.timestamp - targetStartTime >= persistenceTimeMs) {
                    outputActive = true;
                    result.value = targetValue;
                } else {
                    result.value = defaultValue;
                }
            } else {
                // Value is not the target - reset
                targetIsActive = false;
                outputActive = false;
                result.value = defaultValue;
            }
            
            return result;
        }
        
        void reset() override {
            targetIsActive = false;
            outputActive = false;
            targetStartTime = 0;
        }
};

/**
 * Timeout processor - resets to default value after no activity for specified duration
 * Example: Motion sensor goes back to 0 after 60s of no motion
 */
class TimeoutProcessor : public SensorProcessor {
    private:
        unsigned long timeoutMs;
        float timeoutValue;
        float triggerThreshold;      // Value must be above this to reset timeout
        unsigned long lastActiveTime;
        bool isTimedOut;
        
    public:
        TimeoutProcessor(unsigned long timeoutMs, float timeoutVal = 0.0f, float threshold = 0.0f)
            : SensorProcessor("Timeout"),
              timeoutMs(timeoutMs),
              timeoutValue(timeoutVal),
              triggerThreshold(threshold),
              lastActiveTime(0),
              isTimedOut(false) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            // Check if value is above threshold (activity detected)
            if (data.value > triggerThreshold) {
                lastActiveTime = data.timestamp;
                isTimedOut = false;
                result.value = data.value;
            } else {
                // No activity - check timeout
                if (lastActiveTime == 0) {
                    lastActiveTime = data.timestamp;
                }
                
                if (data.timestamp - lastActiveTime >= timeoutMs) {
                    isTimedOut = true;
                    result.value = timeoutValue;
                } else {
                    result.value = data.value;
                }
            }
            
            return result;
        }
        
        void reset() override {
            lastActiveTime = 0;
            isTimedOut = false;
        }
};

/**
 * Rate limiter - limits how often values can change
 * Only allows changes every N milliseconds
 */
class RateLimitProcessor : public SensorProcessor {
    private:
        unsigned long minIntervalMs;
        unsigned long lastUpdateTime;
        float lastOutputValue;
        bool hasOutput;
        
    public:
        RateLimitProcessor(unsigned long intervalMs)
            : SensorProcessor("RateLimit"),
              minIntervalMs(intervalMs),
              lastUpdateTime(0),
              lastOutputValue(0),
              hasOutput(false) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            if (!hasOutput) {
                lastOutputValue = data.value;
                lastUpdateTime = data.timestamp;
                hasOutput = true;
                return result;
            }
            
            // Check if enough time has passed
            if (data.timestamp - lastUpdateTime >= minIntervalMs) {
                lastOutputValue = data.value;
                lastUpdateTime = data.timestamp;
                result.value = data.value;
            } else {
                result.value = lastOutputValue;
            }
            
            return result;
        }
        
        void reset() override {
            hasOutput = false;
            lastUpdateTime = 0;
        }
};

/**
 * Delay processor - delays the output by specified duration
 * Useful for synchronized effects
 */
class DelayProcessor : public SensorProcessor {
    private:
        unsigned long delayMs;
        struct DelayedValue {
            float value;
            unsigned long timestamp;
        };
        std::vector<DelayedValue> valueQueue;
        
    public:
        DelayProcessor(unsigned long delay)
            : SensorProcessor("Delay"),
              delayMs(delay) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            // Add current value to queue
            valueQueue.push_back({data.value, data.timestamp});
            
            SensorData result = data;
            
            // Find the first value that's old enough
            for (auto it = valueQueue.begin(); it != valueQueue.end(); ++it) {
                if (data.timestamp - it->timestamp >= delayMs) {
                    result.value = it->value;
                    // Remove processed values
                    valueQueue.erase(valueQueue.begin(), it + 1);
                    return result;
                }
            }
            
            // If no value is old enough, keep the last known value
            if (!valueQueue.empty()) {
                result.value = valueQueue.front().value;
            }
            
            return result;
        }
        
        void reset() override {
            valueQueue.clear();
        }
};
