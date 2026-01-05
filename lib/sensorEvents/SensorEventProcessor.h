#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <Log.h>

/**
 * Represents a collected sensor event with timestamp
 */
struct SensorEvent {
    std::string sensorName;
    int value;
    unsigned long timestamp;

    SensorEvent(const std::string& name, int val, unsigned long ts) 
        : sensorName(name), value(val), timestamp(ts) {}
};

/**
 * Processor configuration for event collection and conditional processing
 */
struct ProcessorConfig {
    // Maximum number of events to collect before processing
    size_t maxEventsBeforeProcess = 10;
    
    // Maximum time (ms) to wait before forcing processing
    unsigned long maxTimeBeforeProcess = 1000;
    
    // Minimum number of events required before processing
    size_t minEventsForProcess = 1;

    ProcessorConfig() = default;
    
    ProcessorConfig(size_t maxEvents, unsigned long maxTime, size_t minEvents) 
        : maxEventsBeforeProcess(maxEvents), 
          maxTimeBeforeProcess(maxTime),
          minEventsForProcess(minEvents) {}
};

/**
 * Result from processing collected sensor events
 */
struct ProcessingResult {
    std::map<std::string /*sensorName*/, int /*processedValue*/> values;
    bool shouldSetDmx;

    ProcessingResult() : shouldSetDmx(false) {}
};

/**
 * SensorEventProcessor collects sensor events and processes them based on custom conditions.
 * Processing logic is provided via a callback function.
 */
class SensorEventProcessor {
    private:
        std::vector<SensorEvent> collectedEvents;
        ProcessorConfig config;
        unsigned long firstEventTimestamp;
        
        // Processing callback: takes collected events and returns processing result
        std::function<ProcessingResult(const std::vector<SensorEvent>&)> processCallback;

    public:
        SensorEventProcessor(
                const ProcessorConfig& cfg,
                std::function<ProcessingResult(const std::vector<SensorEvent>&)> callback)
            : config(cfg), 
              firstEventTimestamp(0),
              processCallback(callback) {
        }

        /**
         * Add a sensor event to the collection
         * Returns true if processing should be triggered
         */
        bool addEvent(const std::string& sensorName, int value) {
            unsigned long now = millis();
            
            if (collectedEvents.empty()) {
                firstEventTimestamp = now;
            }

            collectedEvents.emplace_back(sensorName, value, now);

            return shouldProcess(now);
        }

        /**
         * Check if conditions are met to trigger processing
         */
        bool shouldProcess(unsigned long currentTime) const {
            if (collectedEvents.size() < config.minEventsForProcess) {
                return false;
            }

            // Check max events limit
            if (collectedEvents.size() >= config.maxEventsBeforeProcess) {
                return true;
            }

            // Check time limit
            if (currentTime - firstEventTimestamp >= config.maxTimeBeforeProcess) {
                return true;
            }

            return false;
        }

        /**
         * Process collected events and return the result
         * Clears the collection after processing
         */
        ProcessingResult process() {
            if (collectedEvents.empty()) {
                return ProcessingResult();
            }

            ProcessingResult result;
            if (processCallback) {
                result = processCallback(collectedEvents);
            }

            clear();
            return result;
        }

        /**
         * Clear collected events
         */
        void clear() {
            collectedEvents.clear();
            firstEventTimestamp = 0;
        }

        /**
         * Get number of collected events
         */
        size_t getEventCount() const {
            return collectedEvents.size();
        }

        /**
         * Get the collected events (for inspection/testing)
         */
        const std::vector<SensorEvent>& getEvents() const {
            return collectedEvents;
        }
};
