#pragma once

#include <SensorEventProcessor.h>
#include <commonProcessors.h>
#include <settings.h>
#include <Log.h>
#include <memory>

/**
 * Factory for creating sensor event processors from configuration
 */
class ProcessorFactory {
    public:
        /**
         * Create a processor and its configuration from settings
         * Returns nullptr if processor name is empty or invalid
         */
        static std::pair<ProcessorConfig, SensorEventProcessor*> createFromConfig(
            const SensorProcessorCfg& cfg, 
            const std::vector<SensorMappingCfg>& sensorMappings) {
            
            // Build base configuration
            ProcessorConfig config(
                cfg.name.empty() ? 1 : cfg.maxEvents, 
                cfg.name.empty() ? 100 : cfg.maxTimeMs, 
                cfg.name.empty() ? 1 : cfg.minEvents
            );
            
            // If no processor specified, create default direct mapping processor
            if (cfg.name.empty()) {
                Log.traceln("No processor configured, using direct mapping");
                auto* processor = createDirectMappingProcessor(config, sensorMappings);
                return {config, processor};
            }
            
            // Build value range mappings if needed
            std::map<std::string, ValueRange> rangeMappings;
            if (cfg.applyValueMapping) {
                for (const auto& sensorMapping : sensorMappings) {
                    rangeMappings[sensorMapping.sensorName] = sensorMapping.valueRange;
                }
            }

            SensorEventProcessor* processor = nullptr;

            // Create processor based on name
            if (cfg.name == "direct") {
                processor = createDirectMappingProcessor(config, sensorMappings);
                
            } else if (cfg.name == "averaging") {
                processor = createAveragingProcessor(config, cfg.applyValueMapping, rangeMappings);
                
            } else if (cfg.name == "median") {
                processor = createMedianProcessor(config, cfg.applyValueMapping, rangeMappings);
                
            } else if (cfg.name == "peak") {
                int threshold = getIntParam(cfg, "threshold", 500);
                processor = createPeakProcessor(config, threshold, cfg.applyValueMapping, rangeMappings);
                
            } else if (cfg.name == "ema") {
                float alpha = getFloatParam(cfg, "alpha", 0.3f);
                processor = createEMAProcessor(config, alpha, cfg.applyValueMapping, rangeMappings);
                
            } else if (cfg.name == "threshold") {
                int threshold = getIntParam(cfg, "threshold", 500);
                bool above = getBoolParam(cfg, "above", true);
                processor = createThresholdProcessor(config, threshold, above, cfg.applyValueMapping, rangeMappings);
                
            } else if (cfg.name == "change") {
                int delta = getIntParam(cfg, "delta", 50);
                processor = createChangeDetectorProcessor(config, delta, cfg.applyValueMapping, rangeMappings);
                
            } else if (cfg.name == "gesture") {
                std::vector<std::string> sequence = getSequenceParam(cfg, "sequence");
                int threshold = getIntParam(cfg, "threshold", 100);
                unsigned long maxTime = getULongParam(cfg, "max_time_between", 500);
                processor = createGestureProcessor(config, sequence, threshold, maxTime, cfg.applyValueMapping, rangeMappings);
                
            } else if (cfg.name == "passthrough" || cfg.name == "latest") {
                processor = createPassthroughProcessor(config, cfg.applyValueMapping, rangeMappings);
                
            } else {
                Log::errorln("Unknown processor type: %s", cfg.name.c_str());
                return {config, nullptr};
            }

            if (processor) {
                Log::infoln("Created processor: %s (max_events=%d, max_time=%lu, min_events=%d, value_mapping=%s)", 
                    cfg.name.c_str(), cfg.maxEvents, cfg.maxTimeMs, cfg.minEvents,
                    cfg.applyValueMapping ? "yes" : "no");
            }

            return {config, processor};
        }

    private:
        static SensorEventProcessor* createDirectMappingProcessor(
                const ProcessorConfig& config,
                const std::vector<SensorMappingCfg>& sensorMappings) {
            
            auto* processor = new CommonProcessors::DirectMappingProcessor(sensorMappings);
            return new SensorEventProcessor(config,
                [processor](const std::vector<SensorEvent>& events) {
                    return (*processor)(events);
                });
        }

        static SensorEventProcessor* createAveragingProcessor(
                const ProcessorConfig& config,
                bool applyMapping,
                const std::map<std::string, ValueRange>& rangeMappings) {
            
            auto* processor = new CommonProcessors::AveragingProcessor();
            if (applyMapping) {
                auto* wrapper = new CommonProcessors::ValueRangeMappingProcessor(
                    [processor](const std::vector<SensorEvent>& events) { return (*processor)(events); },
                    rangeMappings);
                return new SensorEventProcessor(config, 
                    [wrapper](const std::vector<SensorEvent>& events) {
                        return (*wrapper)(events);
                    });
            }
            return new SensorEventProcessor(config,
                [processor](const std::vector<SensorEvent>& events) {
                    return (*processor)(events);
                });
        }

        static SensorEventProcessor* createMedianProcessor(
                const ProcessorConfig& config,
                bool applyMapping,
                const std::map<std::string, ValueRange>& rangeMappings) {
            
            auto* processor = new CommonProcessors::MedianProcessor();
            if (applyMapping) {
                auto* wrapper = new CommonProcessors::ValueRangeMappingProcessor(
                    [processor](const std::vector<SensorEvent>& events) { return (*processor)(events); },
                    rangeMappings);
                return new SensorEventProcessor(config, 
                    [wrapper](const std::vector<SensorEvent>& events) {
                        return (*wrapper)(events);
                    });
            }
            return new SensorEventProcessor(config,
                [processor](const std::vector<SensorEvent>& events) {
                    return (*processor)(events);
                });
        }

        static SensorEventProcessor* createPeakProcessor(
                const ProcessorConfig& config, int threshold,
                bool applyMapping,
                const std::map<std::string, ValueRange>& rangeMappings) {
            
            auto* processor = new CommonProcessors::PeakDetector(threshold);
            if (applyMapping) {
                auto* wrapper = new CommonProcessors::ValueRangeMappingProcessor(
                    [processor](const std::vector<SensorEvent>& events) { return (*processor)(events); },
                    rangeMappings);
                return new SensorEventProcessor(config, 
                    [wrapper](const std::vector<SensorEvent>& events) {
                        return (*wrapper)(events);
                    });
            }
            return new SensorEventProcessor(config, 
                [processor](const std::vector<SensorEvent>& events) {
                    return (*processor)(events);
                });
        }

        static SensorEventProcessor* createEMAProcessor(
                const ProcessorConfig& config, float alpha,
                bool applyMapping,
                const std::map<std::string, ValueRange>& rangeMappings) {
            
            auto* ema = new CommonProcessors::ExponentialMovingAverage(alpha);
            if (applyMapping) {
                auto* wrapper = new CommonProcessors::ValueRangeMappingProcessor(
                    [ema](const std::vector<SensorEvent>& events) { return (*ema)(events); },
                    rangeMappings);
                return new SensorEventProcessor(config, 
                    [wrapper](const std::vector<SensorEvent>& events) {
                        return (*wrapper)(events);
                    });
            }
            return new SensorEventProcessor(config, 
                [ema](const std::vector<SensorEvent>& events) {
                    return (*ema)(events);
                });
        }

        static SensorEventProcessor* createThresholdProcessor(
                const ProcessorConfig& config, int threshold, bool above,
                bool applyMapping,
                const std::map<std::string, ValueRange>& rangeMappings) {
            
            auto* processor = new CommonProcessors::ThresholdGate(threshold, above);
            if (applyMapping) {
                auto* wrapper = new CommonProcessors::ValueRangeMappingProcessor(
                    [processor](const std::vector<SensorEvent>& events) { return (*processor)(events); },
                    rangeMappings);
                return new SensorEventProcessor(config, 
                    [wrapper](const std::vector<SensorEvent>& events) {
                        return (*wrapper)(events);
                    });
            }
            return new SensorEventProcessor(config, 
                [processor](const std::vector<SensorEvent>& events) {
                    return (*processor)(events);
                });
        }

        static SensorEventProcessor* createChangeDetectorProcessor(
                const ProcessorConfig& config, int delta,
                bool applyMapping,
                const std::map<std::string, ValueRange>& rangeMappings) {
            
            auto* detector = new CommonProcessors::ChangeDetector(delta);
            if (applyMapping) {
                auto* wrapper = new CommonProcessors::ValueRangeMappingProcessor(
                    [detector](const std::vector<SensorEvent>& events) { return (*detector)(events); },
                    rangeMappings);
                return new SensorEventProcessor(config, 
                    [wrapper](const std::vector<SensorEvent>& events) {
                        return (*wrapper)(events);
                    });
            }
            return new SensorEventProcessor(config, 
                [detector](const std::vector<SensorEvent>& events) {
                    return (*detector)(events);
                });
        }

        static SensorEventProcessor* createGestureProcessor(
                const ProcessorConfig& config,
                const std::vector<std::string>& sequence,
                int threshold, unsigned long maxTime,
                bool applyMapping,
                const std::map<std::string, ValueRange>& rangeMappings) {
            
            auto* detector = new CommonProcessors::GestureDetector(sequence, threshold, maxTime);
            if (applyMapping) {
                auto* wrapper = new CommonProcessors::ValueRangeMappingProcessor(
                    [detector](const std::vector<SensorEvent>& events) { return (*detector)(events); },
                    rangeMappings);
                return new SensorEventProcessor(config, 
                    [wrapper](const std::vector<SensorEvent>& events) {
                        return (*wrapper)(events);
                    });
            }
            return new SensorEventProcessor(config, 
                [detector](const std::vector<SensorEvent>& events) {
                    return (*detector)(events);
                });
        }

        static SensorEventProcessor* createPassthroughProcessor(
                const ProcessorConfig& config,
                bool applyMapping,
                const std::map<std::string, ValueRange>& rangeMappings) {
            
            auto* processor = new CommonProcessors::PassThroughProcessor();
            if (applyMapping) {
                auto* wrapper = new CommonProcessors::ValueRangeMappingProcessor(
                    [processor](const std::vector<SensorEvent>& events) { return (*processor)(events); },
                    rangeMappings);
                return new SensorEventProcessor(config, 
                    [wrapper](const std::vector<SensorEvent>& events) {
                        return (*wrapper)(events);
                    });
            }
            return new SensorEventProcessor(config,
                [processor](const std::vector<SensorEvent>& events) {
                    return (*processor)(events);
                });
        }

        // Helper functions to extract parameters
        static int getIntParam(const SensorProcessorCfg& cfg, const std::string& key, int defaultValue) {
            auto it = cfg.params.find(key);
            if (it != cfg.params.end()) {
                return atoi(it->second.c_str());
            }
            return defaultValue;
        }

        static float getFloatParam(const SensorProcessorCfg& cfg, const std::string& key, float defaultValue) {
            auto it = cfg.params.find(key);
            if (it != cfg.params.end()) {
                return atof(it->second.c_str());
            }
            return defaultValue;
        }

        static unsigned long getULongParam(const SensorProcessorCfg& cfg, const std::string& key, 
                                           unsigned long defaultValue) {
            auto it = cfg.params.find(key);
            if (it != cfg.params.end()) {
                return strtoul(it->second.c_str(), nullptr, 10);
            }
            return defaultValue;
        }

        static bool getBoolParam(const SensorProcessorCfg& cfg, const std::string& key, bool defaultValue) {
            auto it = cfg.params.find(key);
            if (it != cfg.params.end()) {
                return it->second == "true" || it->second == "1";
            }
            return defaultValue;
        }

        static std::vector<std::string> getSequenceParam(const SensorProcessorCfg& cfg, 
                                                         const std::string& key) {
            std::vector<std::string> result;
            auto it = cfg.params.find(key);
            if (it != cfg.params.end()) {
                // Parse JSON array string
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, it->second);
                if (!error && doc.is<JsonArray>()) {
                    JsonArray arr = doc.as<JsonArray>();
                    for (JsonVariant v : arr) {
                        result.push_back(v.as<std::string>());
                    }
                }
            }
            return result;
        }
};
