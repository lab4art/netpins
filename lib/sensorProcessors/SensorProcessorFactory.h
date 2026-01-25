#pragma once

#include "ProcessorPipeline.h"
#include "RangeMappingProcessors.h"
#include "TimeBasedProcessors.h"
#include "ThresholdProcessors.h"
#include "SmoothingProcessors.h"
#include "MotionStateProcessor.h"
#include <ArduinoJson.h>
#include <map>
#include <string>

// Forward declaration for settings integration
struct SensorProcessorCfg;

/**
 * Configuration structures for different processor types
 */

struct ProcessorConfig {
    std::string type;  // Processor type name
    bool enabled = true;
    std::map<std::string, float> floatParams;
    std::map<std::string, int> intParams;
    std::map<std::string, std::string> stringParams;
    std::map<std::string, bool> boolParams;
    
    // Helper methods to get parameters with defaults
    float getFloat(const std::string& key, float defaultValue = 0.0f) const {
        auto it = floatParams.find(key);
        return (it != floatParams.end()) ? it->second : defaultValue;
    }
    
    int getInt(const std::string& key, int defaultValue = 0) const {
        auto it = intParams.find(key);
        return (it != intParams.end()) ? it->second : defaultValue;
    }
    
    std::string getString(const std::string& key, const std::string& defaultValue = "") const {
        auto it = stringParams.find(key);
        return (it != stringParams.end()) ? it->second : defaultValue;
    }
    
    bool getBool(const std::string& key, bool defaultValue = false) const {
        auto it = boolParams.find(key);
        return (it != boolParams.end()) ? it->second : defaultValue;
    }
};

struct PipelineConfig {
    std::string name;
    std::vector<ProcessorConfig> processors;
    bool enabled = true;
};

/**
 * Factory for creating sensor processors from configuration
 */
class SensorProcessorFactory {
    public:
        /**
         * Create a single processor from configuration
         */
        static std::shared_ptr<SensorProcessor> createProcessor(const ProcessorConfig& config) {
            const std::string& type = config.type;
            
            // Range mapping processors
            if (type == "range_mapping" || type == "map") {
                float fromMin = config.getFloat("input_min", 0.0f);
                float fromMax = config.getFloat("input_max", 100.0f);
                float toMin = config.getFloat("output_min", 0.0f);
                float toMax = config.getFloat("output_max", 255.0f);
                bool clamp = config.getBool("clamp", true);
                return std::make_shared<RangeMappingProcessor>(fromMin, fromMax, toMin, toMax, clamp);
            }
            
            if (type == "scale") {
                float factor = config.getFloat("factor", 1.0f);
                float offset = config.getFloat("offset", 0.0f);
                return std::make_shared<ScaleProcessor>(factor, offset);
            }
            
            if (type == "clamp") {
                float min = config.getFloat("min", 0.0f);
                float max = config.getFloat("max", 255.0f);
                return std::make_shared<ClampProcessor>(min, max);
            }
            
            if (type == "invert") {
                float min = config.getFloat("min", 0.0f);
                float max = config.getFloat("max", 255.0f);
                return std::make_shared<InvertProcessor>(min, max);
            }
            
            if (type == "round") {
                float precision = config.getFloat("precision", 1.0f);
                return std::make_shared<RoundProcessor>(precision);
            }
            
            // Time-based processors
            if (type == "debounce") {
                // Support both time_ms and duration_ms
                unsigned long timeMs = config.getInt("time_ms", config.getInt("duration_ms", 50));
                return std::make_shared<DebounceProcessor>(timeMs);
            }
            
            if (type == "persistence") {
                unsigned long timeMs = config.getInt("time_ms", config.getInt("duration_ms", 1000));
                float target = config.getFloat("target", 1.0f);
                float defaultVal = config.getFloat("default", 0.0f);
                return std::make_shared<PersistenceProcessor>(timeMs, target, defaultVal);
            }
            
            if (type == "timeout") {
                unsigned long timeMs = config.getInt("time_ms", config.getInt("duration_ms", 60000));
                float timeoutVal = config.getFloat("timeout_value", 0.0f);
                float threshold = config.getFloat("threshold", 0.0f);
                return std::make_shared<TimeoutProcessor>(timeMs, timeoutVal, threshold);
            }
            
            if (type == "rate_limit") {
                unsigned long intervalMs = config.getInt("interval_ms", 100);
                return std::make_shared<RateLimitProcessor>(intervalMs);
            }
            
            if (type == "delay") {
                unsigned long delayMs = config.getInt("delay_ms", 1000);
                return std::make_shared<DelayProcessor>(delayMs);
            }
            
            if (type == "motion_state") {
                unsigned long persistMs = config.getInt("persist_ms", 30000);
                unsigned long noMotionMs = config.getInt("no_motion_ms", 30000);
                float threshold = config.getFloat("threshold", 0.0f);
                return std::make_shared<MotionStateProcessor>(persistMs, noMotionMs, threshold);
            }
            
            // Threshold processors
            if (type == "threshold") {
                float threshold = config.getFloat("threshold", 128.0f);
                float high = config.getFloat("high", 255.0f);
                float low = config.getFloat("low", 0.0f);
                bool above = config.getBool("above", true);
                return std::make_shared<ThresholdProcessor>(threshold, high, low, above);
            }
            
            if (type == "hysteresis") {
                float upper = config.getFloat("upper", 150.0f);
                float lower = config.getFloat("lower", 100.0f);
                float high = config.getFloat("high", 255.0f);
                float low = config.getFloat("low", 0.0f);
                return std::make_shared<HysteresisProcessor>(upper, lower, high, low);
            }
            
            if (type == "time_threshold") {
                float threshold = config.getFloat("threshold", 128.0f);
                unsigned long timeMs = config.getInt("time_ms", 1000);
                float high = config.getFloat("high", 255.0f);
                float low = config.getFloat("low", 0.0f);
                bool above = config.getBool("above", true);
                return std::make_shared<TimeBasedThresholdProcessor>(threshold, timeMs, high, low, above);
            }
            
            if (type == "range_threshold") {
                float min = config.getFloat("min", 100.0f);
                float max = config.getFloat("max", 200.0f);
                float inside = config.getFloat("inside", 255.0f);
                float outside = config.getFloat("outside", 0.0f);
                return std::make_shared<RangeThresholdProcessor>(min, max, inside, outside);
            }
            
            if (type == "band_pass") {
                float min = config.getFloat("min", 50.0f);
                float max = config.getFloat("max", 200.0f);
                float defaultVal = config.getFloat("default", 0.0f);
                return std::make_shared<BandPassProcessor>(min, max, defaultVal);
            }
            
            if (type == "dead_zone") {
                float center = config.getFloat("center", 0.0f);
                float radius = config.getFloat("radius", 5.0f);
                return std::make_shared<DeadZoneProcessor>(center, radius);
            }
            
            // Smoothing processors
            if (type == "moving_average" || type == "avg" || type == "average") {
                size_t windowSize = config.getInt("window_size", config.getInt("size", 5));
                return std::make_shared<MovingAverageProcessor>(windowSize);
            }
            
            if (type == "ema" || type == "exponential_average") {
                float alpha = config.getFloat("alpha", config.getFloat("smoothing", 0.3f));
                return std::make_shared<ExponentialMovingAverageProcessor>(alpha);
            }
            
            if (type == "median") {
                size_t windowSize = config.getInt("window_size", config.getInt("size", 5));
                return std::make_shared<MedianFilterProcessor>(windowSize);
            }
            
            if (type == "weighted_average") {
                size_t windowSize = config.getInt("window_size", config.getInt("size", 5));
                // TODO: Support custom weights from config
                return std::make_shared<WeightedMovingAverageProcessor>(windowSize);
            }
            
            if (type == "low_pass" || type == "lowpass") {
                float coefficient = config.getFloat("coefficient", config.getFloat("alpha", 0.1f));
                return std::make_shared<LowPassFilterProcessor>(coefficient);
            }
            
            if (type == "kalman") {
                float processNoise = config.getFloat("process_noise", 0.01f);
                float measurementNoise = config.getFloat("measurement_noise", 0.1f);
                return std::make_shared<KalmanFilterProcessor>(processNoise, measurementNoise);
            }
            
            if (type == "passthrough" || type == "none") {
                return std::make_shared<PassthroughProcessor>();
            }
            
            Log::errorln("Unknown processor type: %s", type.c_str());
            return nullptr;
        }
        
        /**
         * Create a complete pipeline from configuration
         */
        static ProcessorPipeline* createPipeline(const PipelineConfig& config) {
            auto* pipeline = new ProcessorPipeline(config.name);
            pipeline->setEnabled(config.enabled);
            
            for (const auto& procConfig : config.processors) {
                auto processor = createProcessor(procConfig);
                if (processor) {
                    processor->setEnabled(procConfig.enabled);
                    pipeline->addProcessor(processor);
                } else {
                    Log::errorln("Failed to create processor of type: %s", procConfig.type.c_str());
                }
            }
            
            Log::infoln("Created pipeline '%s' with %d processors", 
                       config.name.c_str(), pipeline->size());
            return pipeline;
        }
        
        /**
         * Parse processor configuration from JSON
         */
        static ProcessorConfig parseProcessorConfig(JsonObject json) {
            ProcessorConfig config;
            config.type = json["type"].as<std::string>();
            
            if (json.containsKey("enabled")) {
                config.enabled = json["enabled"].as<bool>();
            }
            
            // Parse all other keys as parameters
            for (JsonPair kv : json) {
                std::string key = kv.key().c_str();
                if (key == "type" || key == "enabled") continue;
                
                if (kv.value().is<float>() || kv.value().is<double>()) {
                    config.floatParams[key] = kv.value().as<float>();
                } else if (kv.value().is<int>()) {
                    config.intParams[key] = kv.value().as<int>();
                } else if (kv.value().is<bool>()) {
                    config.boolParams[key] = kv.value().as<bool>();
                } else if (kv.value().is<const char*>()) {
                    config.stringParams[key] = kv.value().as<std::string>();
                }
            }
            
            return config;
        }
        
        /**
         * Parse pipeline configuration from JSON
         */
        static PipelineConfig parsePipelineConfig(JsonObject json) {
            PipelineConfig config;
            config.name = json["name"].as<std::string>();
            
            if (json.containsKey("enabled")) {
                config.enabled = json["enabled"].as<bool>();
            }
            
            if (json.containsKey("processors")) {
                JsonArray processorsArray = json["processors"].as<JsonArray>();
                for (JsonObject processorJson : processorsArray) {
                    config.processors.push_back(parseProcessorConfig(processorJson));
                }
            }
            
            return config;
        }
        
        /**
         * Create processor from SensorProcessorCfg (from settings.h)
         * This allows integration with the existing settings system
         */
        static std::shared_ptr<SensorProcessor> createProcessorFromConfig(const SensorProcessorCfg& cfg) {
            ProcessorConfig config;
            config.type = cfg.name;
            
            // Parse parameters from the params map
            for (const auto& param : cfg.params) {
                const std::string& key = param.first;
                const std::string& value = param.second;
                
                // Try to parse as different types
                char* endPtr;
                
                // Try int
                long intVal = strtol(value.c_str(), &endPtr, 10);
                if (*endPtr == '\0') {
                    config.intParams[key] = static_cast<int>(intVal);
                    continue;
                }
                
                // Try float
                float floatVal = strtof(value.c_str(), &endPtr);
                if (*endPtr == '\0') {
                    config.floatParams[key] = floatVal;
                    continue;
                }
                
                // Try bool
                if (value == "true" || value == "True" || value == "TRUE") {
                    config.boolParams[key] = true;
                    continue;
                }
                if (value == "false" || value == "False" || value == "FALSE") {
                    config.boolParams[key] = false;
                    continue;
                }
                
                // Otherwise store as string
                config.stringParams[key] = value;
            }
            
            return createProcessor(config);
        }
};

