#pragma once

#include "SensorProcessor.h"
#include <algorithm>

/**
 * Maps sensor values from one range to another
 * Example: Temperature sensor (-40 to 100°C) -> DMX (0-255)
 */
class RangeMappingProcessor : public SensorProcessor {
    private:
        float fromMin, fromMax;
        float toMin, toMax;
        bool clamp;
        
    public:
        RangeMappingProcessor(float fromMin, float fromMax, float toMin, float toMax, bool clamp = true)
            : SensorProcessor("RangeMapping"),
              fromMin(fromMin), fromMax(fromMax),
              toMin(toMin), toMax(toMax),
              clamp(clamp) {
            
            if (fromMin >= fromMax) {
                Log::errorln("RangeMapping: Invalid source range [%f, %f]", fromMin, fromMax);
            }
            if (toMin >= toMax) {
                Log::errorln("RangeMapping: Invalid target range [%f, %f]", toMin, toMax);
            }
        }
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            // Map value from source range to target range
            float normalized = (data.value - fromMin) / (fromMax - fromMin);
            float mapped = toMin + normalized * (toMax - toMin);
            
            // Clamp to target range if requested
            if (clamp) {
                mapped = std::max(toMin, std::min(toMax, mapped));
            }
            
            result.value = mapped;
            return result;
        }
};

/**
 * Scales values by a constant factor
 */
class ScaleProcessor : public SensorProcessor {
    private:
        float scaleFactor;
        float offset;
        
    public:
        ScaleProcessor(float factor, float offset = 0.0f)
            : SensorProcessor("Scale"),
              scaleFactor(factor),
              offset(offset) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            result.value = data.value * scaleFactor + offset;
            return result;
        }
};

/**
 * Clamps values to a specific range
 */
class ClampProcessor : public SensorProcessor {
    private:
        float minValue, maxValue;
        
    public:
        ClampProcessor(float min, float max)
            : SensorProcessor("Clamp"),
              minValue(min), maxValue(max) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            result.value = std::max(minValue, std::min(maxValue, data.value));
            return result;
        }
};

/**
 * Inverts the value within a range
 * Example: For range 0-255, value 100 becomes 155
 */
class InvertProcessor : public SensorProcessor {
    private:
        float minValue, maxValue;
        
    public:
        InvertProcessor(float min, float max)
            : SensorProcessor("Invert"),
              minValue(min), maxValue(max) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            result.value = maxValue - (data.value - minValue);
            return result;
        }
};

/**
 * Rounds values to nearest integer or specified precision
 */
class RoundProcessor : public SensorProcessor {
    private:
        float precision;  // 1.0 for integer, 0.1 for one decimal, etc.
        
    public:
        RoundProcessor(float precision = 1.0f)
            : SensorProcessor("Round"),
              precision(precision) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            if (precision > 0) {
                result.value = round(data.value / precision) * precision;
            } else {
                result.value = round(data.value);
            }
            return result;
        }
};

