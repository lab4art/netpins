#pragma once

#include "DmxProcessor.h"
#include "DmxCueListProcessor.h"
#include "settings.h"
#include "scheduler.h"
#include "DmxManager.h"
#include <ArduinoJson.h>
#include <memory>
#include <map>
#include <string>

/**
 * Factory for creating DMX processors from configuration
 * 
 * Similar to SensorProcessorFactory, this creates DMX effect processors
 * from YAML/JSON configuration.
 */
class DmxProcessorFactory {
public:
    /**
     * Create a DMX processor from configuration
     * 
     * @param config The processor configuration
     * @param scheduler Scheduler instance for animation tasks
     * @param dmxManager DmxManager instance for accessing DMX data
     * @return Pointer to created processor, or nullptr if type unknown
     */
    static DmxProcessor* createProcessor(const DmxProcessorCfg& config, Scheduler* scheduler, DmxManager* dmxManager) {
        const std::string& type = config.type;
        
        if (type == "cue_list") {
            return createCueListProcessor(config, scheduler, dmxManager);
        }
        
        // Unknown processor type
        return nullptr;
    }
    
private:
    /**
     * Create a cue list processor from configuration
     */
    static DmxProcessor* createCueListProcessor(const DmxProcessorCfg& config, Scheduler* scheduler, DmxManager* dmxManager) {
        auto processor = new DmxCueListProcessor(scheduler, dmxManager);
              
        // Parse loop
        auto loopIt = config.params.find("loop");
        if (loopIt != config.params.end()) {
            processor->setLoop(loopIt->second == "true");
        }
        
        // Parse scenes array
        auto scenesIt = config.params.find("scenes");
        if (scenesIt != config.params.end()) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, scenesIt->second);
            if (!error && doc.is<JsonArray>()) {
                JsonArray scenesArray = doc.as<JsonArray>();
                
                for (JsonVariant sceneVariant : scenesArray) {
                    if (!sceneVariant.is<JsonObject>()) continue;
                    JsonObject sceneObj = sceneVariant.as<JsonObject>();
                    
                    // Parse scene parameters
                    unsigned long fadeInMs = sceneObj["fade_in_ms"] | 1000;
                    unsigned long holdMs = sceneObj["hold_ms"] | 5000;
                    
                    // Parse channels map
                    std::map<DmxCfg, uint8_t> channels;
                    if (sceneObj.containsKey("channels")) {
                        JsonObject channelsObj = sceneObj["channels"].as<JsonObject>();
                        for (JsonPair kv : channelsObj) {
                            // Key format: "channel@universe" (e.g., "1@0")
                            std::string key = kv.key().c_str();
                            DmxCfg dmxCfg = DmxCfg::deserialize(key);
                            uint8_t value = kv.value().as<uint8_t>();
                            channels[dmxCfg] = value;
                        }
                    }
                    
                    // Add scene to processor
                    if (!channels.empty()) {
                        processor->addScene(channels, fadeInMs, holdMs);
                    }
                }
            }
        }
        
        return processor;
    }
    
    /**
     * Helper: Parse string parameter as int
     * TODO move to common utility class
     */
    static int parseInt(const std::map<std::string, std::string>& params, 
                        const std::string& key, 
                        int defaultValue = 0) {
        auto it = params.find(key);
        if (it != params.end()) {
            return std::stoi(it->second);
        }
        return defaultValue;
    }
    
    /**
     * Helper: Parse string parameter as float
     */
    static float parseFloat(const std::map<std::string, std::string>& params,
                           const std::string& key,
                           float defaultValue = 0.0f) {
        auto it = params.find(key);
        if (it != params.end()) {
            return std::stof(it->second);
        }
        return defaultValue;
    }
    
    /**
     * Helper: Parse string parameter as bool
     */
    static bool parseBool(const std::map<std::string, std::string>& params,
                         const std::string& key,
                         bool defaultValue = false) {
        auto it = params.find(key);
        if (it != params.end()) {
            return it->second == "true" || it->second == "1";
        }
        return defaultValue;
    }
};
