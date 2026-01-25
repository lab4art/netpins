#pragma once

#include "SensorProcessorFactory.h"
#include "ProcessorPipeline.h"
#include <map>
#include <string>
#include <memory>
#include <Log.h>
#include <settings.h>
#include <PubSubClient.h>

/**
 * ConfigurablePipelineManager
 * 
 * Manages sensor pipelines created from configuration (Settings).
 * - Loads pipeline configurations from Settings::sensorPipelines
 * - Creates pipelines using SensorProcessorFactory
 * - Routes sensor values through configured pipelines to DMX channels and/or MQTT
 * - Provides integration with existing sensor callback system
 * 
 * Usage:
 *   ConfigurablePipelineManager manager(dmxData, settings, mqtt, "topic/prefix/");
 *   manager.initialize();
 *   
 *   // In sensor callback:
 *   manager.processSensorValue("temperature_1", 25.5);
 */
class ConfigurablePipelineManager {
public:
    struct PipelineEntry {
        std::shared_ptr<ProcessorPipeline> pipeline;
        DmxCfg dmxCfg;
        std::string sensorName;
        std::string mqttTopic;  // Empty = don't publish to MQTT
    };

private:
    std::map<uint16_t, std::array<uint8_t, 512>>& dmxData;
    Settings& settings;
    std::map<std::string, PipelineEntry> pipelines;
    bool initialized = false;
    PubSubClient* mqttClient;
    std::string mqttTopicPrefix;

public:
    /**
     * Constructor
     * @param dmxData Reference to DMX data structure (from DmxManager)
     * @param settings Reference to Settings object containing pipeline configurations
     * @param mqttClient Pointer to MQTT client for publishing (optional, can be nullptr)
     * @param mqttTopicPrefix MQTT topic prefix (e.g., "np/hostname/s/")
     */
    ConfigurablePipelineManager(
        std::map<uint16_t, std::array<uint8_t, 512>>& dmxData,
        Settings& settings,
        PubSubClient* mqttClient = nullptr,
        const std::string& mqttTopicPrefix = ""
    ) : dmxData(dmxData), settings(settings), mqttClient(mqttClient), mqttTopicPrefix(mqttTopicPrefix) {}

    /**
     * Initialize pipelines from settings configuration
     * Parses Settings::sensorPipelines and creates ProcessorPipeline instances
     * @return true if initialization successful, false otherwise
     */
    bool initialize() {
        if (initialized) {
            Log::warning("ConfigurablePipelineManager already initialized");
            return true;
        }

        Log::info("Initializing ConfigurablePipelineManager");
        
        if (settings.sensorPipelines.empty()) {
            Log::info("No sensor pipelines configured");
            initialized = true;
            return true;
        }

        SensorProcessorFactory factory;
        size_t successCount = 0;
        size_t failCount = 0;

        for (const auto& cfg : settings.sensorPipelines) {
            try {
                // Validate sensor name
                if (cfg.sensorName.empty()) {
                    Log::error("Pipeline configuration missing sensor name");
                    failCount++;
                    continue;
                }

                // Validate DMX configuration (only if not using MQTT-only mode)
                if (cfg.dmxCfg.channel == 0 && cfg.mqttTopic.empty()) {
                    Log::error((std::string("Pipeline for sensor '") + cfg.sensorName + 
                               "' has no output configured (no DMX or MQTT)").c_str());
                    failCount++;
                    continue;
                }

                // Create pipeline
                PipelineBuilder builder;
                
                for (const auto& procCfg : cfg.processors) {
                    try {
                        auto processor = factory.createProcessorFromConfig(procCfg);
                        if (processor) {
                            builder.add(processor);
                        } else {
                            Log::error((std::string("Failed to create processor '") + 
                                       procCfg.name + "' for sensor '" + 
                                       cfg.sensorName + "'").c_str());
                            failCount++;
                            continue;
                        }
                    } catch (const std::exception& e) {
                        Log::error((std::string("Exception creating processor '") + 
                                   procCfg.name + "': " + e.what()).c_str());
                        failCount++;
                        continue;
                    }
                }

                auto pipeline = builder.build();
                
                // Store pipeline entry
                PipelineEntry entry;
                entry.pipeline = std::make_shared<ProcessorPipeline>(std::move(pipeline));
                entry.dmxCfg = cfg.dmxCfg;
                entry.sensorName = cfg.sensorName;
                entry.mqttTopic = cfg.mqttTopic;
                
                pipelines[cfg.sensorName] = entry;
                
                std::string outputs = "";
                if (cfg.dmxCfg.channel > 0) {
                    outputs += "DMX " + std::to_string(cfg.dmxCfg.channel) + "@" + 
                              std::to_string(cfg.dmxCfg.universe);
                }
                if (!cfg.mqttTopic.empty()) {
                    if (!outputs.empty()) outputs += " + ";
                    outputs += "MQTT(" + cfg.mqttTopic + ")";
                }
                
                Log::info((std::string("Created pipeline for sensor '") + 
                          cfg.sensorName + "' -> " + outputs +
                          " with " + std::to_string(cfg.processors.size()) + 
                          " processors").c_str());
                successCount++;

            } catch (const std::exception& e) {
                Log::error((std::string("Exception initializing pipeline for '") + 
                           cfg.sensorName + "': " + e.what()).c_str());
                failCount++;
            }
        }

        Log::info((std::string("Pipeline initialization complete: ") + 
                  std::to_string(successCount) + " succeeded, " + 
                  std::to_string(failCount) + " failed").c_str());

        initialized = true;
        return failCount == 0;
    }

    /**
     * Process a sensor value through its configured pipeline
     * @param sensorName Name of the sensor (must match configuration)
     * @param rawValue Raw sensor value to process
     * @return true if processed successfully, false if sensor not configured
     */
    bool processSensorValue(const std::string& sensorName, float rawValue) {
        auto it = pipelines.find(sensorName);
        if (it == pipelines.end()) {
            // Sensor not configured for pipeline processing
            return false;
        }

        PipelineEntry& entry = it->second;
        
        // Create sensor data context
        SensorData data;
        data.value = rawValue;
        data.timestamp = millis();

        // Process through pipeline
        SensorData processed = entry.pipeline->process(data);

        // Map to DMX range (0-1 -> 0-255)
        uint8_t dmxValue = static_cast<uint8_t>(
            clampValue(processed.value, 0.0f, 255.0f)
        );

        // Log::traceln("Sensor '%s': raw=%.3f processed=%.3f, dmx-ch=%d@%d, value=%d",
        //            sensorName.c_str(), rawValue, processed.value, entry.dmxCfg.channel, entry.dmxCfg.universe, dmxValue);
        // Set DMX value if configured
        if (entry.dmxCfg.channel > 0) {
            uint16_t dmxChannel = entry.dmxCfg.get0BasedChannel();
            dmxData[entry.dmxCfg.universe][dmxChannel] = dmxValue;
        }
        
        // Publish to MQTT if configured
        if (!entry.mqttTopic.empty() && mqttClient && mqttClient->connected()) {
            std::string topic = mqttTopicPrefix + entry.mqttTopic;
            std::string payload = std::to_string(processed.value);
            mqttClient->publish(topic.c_str(), payload.c_str());
        }

        return true;
    }

    /**
     * Process a sensor value and get the DMX value without setting it
     * Useful for testing or previewing pipeline output
     * @param sensorName Name of the sensor
     * @param rawValue Raw sensor value
     * @param dmxValue Output parameter for DMX value
     * @return true if processed successfully
     */
    bool processSensorValuePreview(const std::string& sensorName, float rawValue, uint8_t& dmxValue) {
        auto it = pipelines.find(sensorName);
        if (it == pipelines.end()) {
            return false;
        }

        PipelineEntry& entry = it->second;
        
        SensorData data;
        data.value = rawValue;
        data.timestamp = millis();

        SensorData processed = entry.pipeline->process(data);

        dmxValue = static_cast<uint8_t>(
            clampValue(processed.value * 255.0f, 0.0f, 255.0f)
        );

        return true;
    }

    /**
     * Check if a sensor has a configured pipeline
     * @param sensorName Sensor name to check
     * @return true if pipeline exists for this sensor
     */
    bool hasPipeline(const std::string& sensorName) const {
        return pipelines.find(sensorName) != pipelines.end();
    }

    /**
     * Get pipeline configuration for a sensor
     * @param sensorName Sensor name
     * @return Pointer to pipeline entry, or nullptr if not found
     */
    const PipelineEntry* getPipeline(const std::string& sensorName) const {
        auto it = pipelines.find(sensorName);
        return (it != pipelines.end()) ? &(it->second) : nullptr;
    }

    /**
     * Reset all pipelines to initial state
     */
    void resetAll() {
        for (auto& pair : pipelines) {
            pair.second.pipeline->reset();
        }
        Log::info("All pipelines reset");
    }

    /**
     * Reset a specific pipeline
     * @param sensorName Sensor name
     * @return true if pipeline found and reset
     */
    bool reset(const std::string& sensorName) {
        auto it = pipelines.find(sensorName);
        if (it != pipelines.end()) {
            it->second.pipeline->reset();
            return true;
        }
        return false;
    }

    /**
     * Enable/disable a specific pipeline
     * @param sensorName Sensor name
     * @param enabled true to enable, false to disable
     * @return true if pipeline found
     */
    bool setEnabled(const std::string& sensorName, bool enabled) {
        auto it = pipelines.find(sensorName);
        if (it != pipelines.end()) {
            it->second.pipeline->setEnabled(enabled);
            return true;
        }
        return false;
    }

    /**
     * Get count of configured pipelines
     * @return Number of pipelines
     */
    size_t getPipelineCount() const {
        return pipelines.size();
    }

    /**
     * Get all configured sensor names
     * @return Vector of sensor names with configured pipelines
     */
    std::vector<std::string> getSensorNames() const {
        std::vector<std::string> names;
        names.reserve(pipelines.size());
        for (const auto& pair : pipelines) {
            names.push_back(pair.first);
        }
        return names;
    }

private:
    /**
     * Map value from one range to another
     */
    float mapValueRange(float value, float inMin, float inMax, float outMin, float outMax) const {
        if (inMax == inMin) {
            return outMin; // Avoid division by zero
        }
        
        // Clamp input value to input range
        float clampedValue = clampValue(value, inMin, inMax);
        
        // Map to output range
        return outMin + (clampedValue - inMin) * (outMax - outMin) / (inMax - inMin);
    }

    float clampValue(float value, float min, float max) const {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }
};
