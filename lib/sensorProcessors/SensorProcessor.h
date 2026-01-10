#pragma once

#include <Arduino.h>
#include <functional>
#include <memory>
#include <Log.h>

/**
 * Sensor data context that flows through the pipeline
 * Contains both the current value and metadata for time-based processing
 */
struct SensorData {
    float value;                    // Current sensor value
    unsigned long timestamp;         // Timestamp of this reading
    bool hasChanged;                 // Whether value changed from previous reading
    float previousValue;             // Previous value (for change detection)
    unsigned long lastChangeTime;    // When the value last changed
    
    SensorData(float val = 0.0f, unsigned long ts = 0) 
        : value(val), 
          timestamp(ts), 
          hasChanged(false),
          previousValue(val),
          lastChangeTime(ts) {}
    
    void updateValue(float newValue, unsigned long newTimestamp) {
        if (newValue != value) {
            hasChanged = true;
            previousValue = value;
            lastChangeTime = newTimestamp;
        } else {
            hasChanged = false;
        }
        value = newValue;
        timestamp = newTimestamp;
    }
};

/**
 * Base interface for all sensor processors
 * Each processor transforms sensor data and passes it to the next processor in the pipeline
 */
class SensorProcessor {
    protected:
        std::string name;
        bool enabled;
        
    public:
        SensorProcessor(const std::string& processorName) 
            : name(processorName), enabled(true) {}
        
        virtual ~SensorProcessor() {}
        
        /**
         * Process sensor data and return the result
         * @param data Input sensor data with context
         * @return Processed sensor data
         */
        virtual SensorData process(const SensorData& data) = 0;
        
        /**
         * Reset internal state of the processor
         */
        virtual void reset() {}
        
        /**
         * Get processor name for debugging
         */
        const std::string& getName() const { return name; }
        
        /**
         * Enable/disable this processor
         */
        void setEnabled(bool enable) { enabled = enable; }
        bool isEnabled() const { return enabled; }
};

/**
 * Passthrough processor - does nothing, just passes data through
 * Useful for testing and as a placeholder
 */
class PassthroughProcessor : public SensorProcessor {
    public:
        PassthroughProcessor() : SensorProcessor("Passthrough") {}
        
        SensorData process(const SensorData& data) override {
            return data;
        }
};
