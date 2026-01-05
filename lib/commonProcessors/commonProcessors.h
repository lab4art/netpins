#pragma once

#include <SensorEventProcessor.h>
#include <settings.h>
#include <map>
#include <vector>
#include <Log.h>

/**
 * Common processor implementations that can be selected by name from config
 */
namespace CommonProcessors {

    /**
     * Direct Mapping Processor
     * Returns the latest value and applies value range mapping from sensor config
     * This is the default processor when none is specified
     */
    class DirectMappingProcessor {
        private:
            std::map<std::string, ValueRange> mappings;
        
        public:
            DirectMappingProcessor(const std::vector<SensorMappingCfg>& sensorMappings) {
                for (const auto& mapping : sensorMappings) {
                    mappings[mapping.sensorName] = mapping.valueRange;
                }
            }
            
            ProcessingResult operator()(const std::vector<SensorEvent>& events) {
                ProcessingResult result;
                result.shouldSetDmx = true;
                
                // Get the latest value for each sensor
                std::map<std::string, int> latestValues;
                for (const auto& event : events) {
                    latestValues[event.sensorName] = event.value;
                }
                
                // Apply value range mapping to DMX range (0-255)
                for (auto it = latestValues.begin(); it != latestValues.end(); ++it) {
                    const std::string& sensorName = it->first;
                    int value = it->second;
                    
                    auto mappingIt = mappings.find(sensorName);
                    if (mappingIt != mappings.end()) {
                        const ValueRange& range = mappingIt->second;
                        // Map from sensor range to DMX range (0-255)
                        long mappedValue = map(value, range.from, range.to, 0, 255);
                        if (mappedValue < 0) mappedValue = 0;
                        if (mappedValue > 255) mappedValue = 255;
                        result.values[sensorName] = static_cast<int>(mappedValue);
                    } else {
                        result.values[sensorName] = value;
                    }
                }
                
                return result;
            }
    };

    /**
     * Averaging Processor
     * Collects events and returns the average value for each sensor
     */
    class AveragingProcessor {
        public:
            ProcessingResult operator()(const std::vector<SensorEvent>& events) {
                ProcessingResult result;
                result.shouldSetDmx = true;
                
                // Group events by sensor name and average their values
                std::map<std::string, std::vector<int>> sensorValues;
                for (const auto& event : events) {
                    sensorValues[event.sensorName].push_back(event.value);
                }
                
                // Calculate averages
                for (auto it = sensorValues.begin(); it != sensorValues.end(); ++it) {
                    const std::string& sensorName = it->first;
                    const std::vector<int>& values = it->second;
                    int sum = 0;
                    for (int val : values) {
                        sum += val;
                    }
                    int average = sum / values.size();
                    result.values[sensorName] = average;
                }
                
                return result;
            }
    };

    /**
     * Peak Detection Processor
     * Returns the maximum value for each sensor if above threshold
     */
    class PeakDetector {
        private:
            int threshold;
        public:
            PeakDetector(int thresh = 500) : threshold(thresh) {}
            
            ProcessingResult operator()(const std::vector<SensorEvent>& events) {
                ProcessingResult result;
                
                // Find the maximum value for each sensor
                std::map<std::string, int> maxValues;
                for (const auto& event : events) {
                    if (maxValues.find(event.sensorName) == maxValues.end() || 
                        event.value > maxValues[event.sensorName]) {
                        maxValues[event.sensorName] = event.value;
                    }
                }
                
                // Only set DMX if peak is above threshold
                for (auto it = maxValues.begin(); it != maxValues.end(); ++it) {
                    const std::string& sensorName = it->first;
                    int maxValue = it->second;
                    if (maxValue > threshold) {
                        result.shouldSetDmx = true;
                        result.values[sensorName] = maxValue;
                    }
                }
                
                return result;
            }
    };

    /**
     * Exponential Moving Average Processor
     * Applies smoothing to sensor values
     */
    class ExponentialMovingAverage {
        private:
            std::map<std::string, float> lastEMA;
            float alpha;  // Smoothing factor (0-1)
        
        public:
            ExponentialMovingAverage(float smoothingFactor = 0.3f) : alpha(smoothingFactor) {}
            
            ProcessingResult operator()(const std::vector<SensorEvent>& events) {
                ProcessingResult result;
                result.shouldSetDmx = true;
                
                // Get the latest value for each sensor
                std::map<std::string, int> latestValues;
                for (const auto& event : events) {
                    latestValues[event.sensorName] = event.value;
                }
                
                // Apply exponential moving average
                for (auto it = latestValues.begin(); it != latestValues.end(); ++it) {
                    const std::string& sensorName = it->first;
                    int value = it->second;
                    if (lastEMA.find(sensorName) == lastEMA.end()) {
                        lastEMA[sensorName] = value;
                    } else {
                        lastEMA[sensorName] = alpha * value + (1 - alpha) * lastEMA[sensorName];
                    }
                    result.values[sensorName] = static_cast<int>(lastEMA[sensorName]);
                }
                
                return result;
            }
    };

    /**
     * Median Filter Processor
     * Returns the median value for each sensor (good for noise reduction)
     */
    class MedianProcessor {
        public:
            ProcessingResult operator()(const std::vector<SensorEvent>& events) {
                ProcessingResult result;
                result.shouldSetDmx = true;
                
                // Group events by sensor name
                std::map<std::string, std::vector<int>> sensorValues;
                for (const auto& event : events) {
                    sensorValues[event.sensorName].push_back(event.value);
                }
                
                // Calculate medians
                for (auto it = sensorValues.begin(); it != sensorValues.end(); ++it) {
                    const std::string& sensorName = it->first;
                    std::vector<int>& values = it->second;
                    if (!values.empty()) {
                        std::sort(values.begin(), values.end());
                        size_t mid = values.size() / 2;
                        int median = (values.size() % 2 == 0) 
                            ? (values[mid - 1] + values[mid]) / 2 
                            : values[mid];
                        result.values[sensorName] = median;
                    }
                }
                
                return result;
            }
    };

    /**
     * Latest Value Processor
     * Simply returns the most recent value for each sensor
     */
    class LatestValueProcessor {
        public:
            ProcessingResult operator()(const std::vector<SensorEvent>& events) {
                ProcessingResult result;
                result.shouldSetDmx = true;
                
                // Get the latest value for each sensor
                std::map<std::string, int> latestValues;
                for (const auto& event : events) {
                    latestValues[event.sensorName] = event.value;
                }
                
                result.values = latestValues;
                return result;
            }
    };

    /**
     * Threshold Gate Processor
     * Only allows values through if they exceed a threshold
     */
    class ThresholdGate {
        private:
            int threshold;
            bool aboveThreshold;  // true = must be above, false = must be below
        
        public:
            ThresholdGate(int thresh = 500, bool above = true) 
                : threshold(thresh), aboveThreshold(above) {}
            
            ProcessingResult operator()(const std::vector<SensorEvent>& events) {
                ProcessingResult result;
                
                // Get the latest value for each sensor
                std::map<std::string, int> latestValues;
                for (const auto& event : events) {
                    latestValues[event.sensorName] = event.value;
                }
                
                // Only pass values that meet threshold criteria
                for (auto it = latestValues.begin(); it != latestValues.end(); ++it) {
                    const std::string& sensorName = it->first;
                    int value = it->second;
                    bool passes = aboveThreshold ? (value > threshold) : (value < threshold);
                    if (passes) {
                        result.shouldSetDmx = true;
                        result.values[sensorName] = value;
                    }
                }
                
                return result;
            }
    };

    /**
     * Change Detection Processor
     * Only triggers when value changes by more than a delta
     */
    class ChangeDetector {
        private:
            std::map<std::string, int> lastValues;
            int minDelta;
        
        public:
            ChangeDetector(int delta = 50) : minDelta(delta) {}
            
            ProcessingResult operator()(const std::vector<SensorEvent>& events) {
                ProcessingResult result;
                
                // Get the latest value for each sensor
                std::map<std::string, int> latestValues;
                for (const auto& event : events) {
                    latestValues[event.sensorName] = event.value;
                }
                
                // Only pass values that changed significantly
                for (auto it = latestValues.begin(); it != latestValues.end(); ++it) {
                    const std::string& sensorName = it->first;
                    int value = it->second;
                    auto lastIt = lastValues.find(sensorName);
                    if (lastIt == lastValues.end() || abs(value - lastIt->second) >= minDelta) {
                        result.shouldSetDmx = true;
                        result.values[sensorName] = value;
                        lastValues[sensorName] = value;
                    }
                }
                
                return result;
            }
    };

    /**
     * Multi-Sensor Gesture Processor
     * Detects sequential sensor triggers
     */
    class GestureDetector {
        private:
            std::vector<std::string> sequence;
            int threshold;
            unsigned long maxTimeBetween;
        
        public:
            GestureDetector(const std::vector<std::string>& seq, int thresh = 100, unsigned long maxTime = 500)
                : sequence(seq), threshold(thresh), maxTimeBetween(maxTime) {}
            
            ProcessingResult operator()(const std::vector<SensorEvent>& events) {
                ProcessingResult result;
                
                // Check if events match the sequence
                size_t sequenceIndex = 0;
                unsigned long lastTriggerTime = 0;
                
                for (const auto& event : events) {
                    if (sequenceIndex < sequence.size() && 
                        event.sensorName == sequence[sequenceIndex] && 
                        event.value > threshold) {
                        
                        if (sequenceIndex == 0 || 
                            (event.timestamp - lastTriggerTime) <= maxTimeBetween) {
                            sequenceIndex++;
                            lastTriggerTime = event.timestamp;
                        }
                    }
                }
                
                // If complete sequence detected, trigger all sensors
                if (sequenceIndex == sequence.size()) {
                    result.shouldSetDmx = true;
                    for (const auto& sensorName : sequence) {
                        result.values[sensorName] = 1023;  // Max value
                    }
                    Log::infoln("Gesture detected!");
                }
                
                return result;
            }
    };

    /**
     * Value Range Mapping Processor
     * Wraps another processor and applies value range mapping to the output
     */
    class ValueRangeMappingProcessor {
        private:
            std::function<ProcessingResult(const std::vector<SensorEvent>&)> innerProcessor;
            std::map<std::string, ValueRange> mappings;
        
        public:
            ValueRangeMappingProcessor(
                std::function<ProcessingResult(const std::vector<SensorEvent>&)> processor,
                const std::map<std::string, ValueRange>& rangeMappings)
                : innerProcessor(processor), mappings(rangeMappings) {}
            
            ProcessingResult operator()(const std::vector<SensorEvent>& events) {
                // First, run the inner processor
                ProcessingResult result = innerProcessor(events);
                
                // Then apply value range mapping to DMX range (0-255)
                if (result.shouldSetDmx) {
                    for (auto it = result.values.begin(); it != result.values.end(); ++it) {
                        const std::string& sensorName = it->first;
                        int& value = it->second;
                        
                        auto mappingIt = mappings.find(sensorName);
                        if (mappingIt != mappings.end()) {
                            const ValueRange& range = mappingIt->second;
                            // Map from sensor range to DMX range (0-255)
                            long mappedValue = map(value, range.from, range.to, 0, 255);
                            if (mappedValue < 0) mappedValue = 0;
                            if (mappedValue > 255) mappedValue = 255;
                            value = static_cast<int>(mappedValue);
                        }
                    }
                }
                
                return result;
            }
    };

    /**
     * Pass-Through Processor
     * Returns the most recent value without any processing (for testing)
     */
    class PassThroughProcessor : public LatestValueProcessor {
    };

} // namespace CommonProcessors
