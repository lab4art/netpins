#pragma once

/**
 * Example integration of sensor processor pipelines with the existing sensor system
 * 
 * This shows how to use the new pipeline system with existing sensors
 */

#include <sensorProcessors.h>
#include <sensors.h>
#include <map>
#include <memory>

/**
 * Enhanced sensor mapping that includes a processing pipeline
 */
struct SensorPipelineMapping {
    std::string sensorName;
    uint16_t universe;
    uint8_t channel;
    std::unique_ptr<ProcessorPipeline> pipeline;
    
    SensorPipelineMapping(const std::string& name, uint16_t univ, uint8_t chan, 
                         ProcessorPipeline* pipe = nullptr)
        : sensorName(name), universe(univ), channel(chan), pipeline(pipe) {}
};

/**
 * Sensor manager with integrated pipeline processing
 */
class SensorPipelineManager {
    private:
        std::map<std::string, SensorPipelineMapping> mappings;
        std::map<uint16_t, std::array<uint8_t, 512>>& dmxData;
        
    public:
        SensorPipelineManager(std::map<uint16_t, std::array<uint8_t, 512>>& dmxDataRef)
            : dmxData(dmxDataRef) {}
        
        /**
         * Register a sensor with its pipeline
         */
        void registerSensor(const std::string& sensorName, uint16_t universe, 
                          uint8_t channel, ProcessorPipeline* pipeline) {
            mappings.emplace(sensorName, 
                SensorPipelineMapping(sensorName, universe, channel, pipeline));
            Log::infoln("Registered sensor '%s' -> DMX %d@%d with pipeline", 
                       sensorName.c_str(), universe, channel);
        }
        
        /**
         * Process a sensor reading through its pipeline and update DMX
         */
        void processSensorValue(const std::string& sensorName, float rawValue) {
            auto it = mappings.find(sensorName);
            if (it == mappings.end()) {
                Log::traceln("Unknown sensor: %s", sensorName.c_str());
                return;
            }
            
            SensorPipelineMapping& mapping = it->second;
            
            // Process through pipeline if available
            float finalValue;
            if (mapping.pipeline) {
                SensorData result = mapping.pipeline->process(rawValue, millis());
                finalValue = result.value;
            } else {
                // No pipeline - use raw value
                finalValue = rawValue;
            }
            
            // Clamp to DMX range and set channel
            uint8_t dmxValue = (uint8_t)std::max(0.0f, std::min(255.0f, finalValue));
            dmxData[mapping.universe][mapping.channel] = dmxValue;
            
            Log::traceln("Sensor '%s': %.2f -> %.2f -> DMX %d", 
                        sensorName.c_str(), rawValue, finalValue, dmxValue);
        }
        
        /**
         * Reset all pipelines (clear state)
         */
        void resetAllPipelines() {
            for (auto& pair : mappings) {
                if (pair.second.pipeline) {
                    pair.second.pipeline->reset();
                }
            }
            Log::infoln("Reset all sensor pipelines");
        }
        
        /**
         * Enable/disable a sensor pipeline
         */
        void setSensorEnabled(const std::string& sensorName, bool enabled) {
            auto it = mappings.find(sensorName);
            if (it != mappings.end() && it->second.pipeline) {
                it->second.pipeline->setEnabled(enabled);
                Log::infoln("Sensor '%s' pipeline: %s", 
                           sensorName.c_str(), enabled ? "enabled" : "disabled");
            }
        }
        
        /**
         * Print status of all registered sensors
         */
        void printStatus() {
            Log::infoln("Sensor Pipeline Manager - %d sensors registered:", mappings.size());
            for (const auto& pair : mappings) {
                const SensorPipelineMapping& mapping = pair.second;
                Log::infoln("  '%s' -> DMX %d@%d (%d processors, %s)",
                           mapping.sensorName.c_str(),
                           mapping.universe,
                           mapping.channel,
                           mapping.pipeline ? mapping.pipeline->size() : 0,
                           (mapping.pipeline && mapping.pipeline->isEnabled()) ? "enabled" : "disabled");
            }
        }
};

/**
 * Example: Setup function showing how to configure various sensors
 */
void setupSensorPipelines(SensorPipelineManager& manager) {
    // Example 1: Temperature sensor with averaging and mapping
    auto* tempPipeline = PipelineBuilder("temperature")
        .add<MedianFilterProcessor>(5)
        .add<ExponentialMovingAverageProcessor>(0.3f)
        .add<RangeMappingProcessor>(-40.0f, 100.0f, 0.0f, 255.0f)
        .buildPtr();
    manager.registerSensor("dht_temperature", 1, 0, tempPipeline);
    
    // Example 2: Motion sensor with persistence and timeout
    auto* motionPipeline = PipelineBuilder("motion")
        .add<DebounceProcessor>(100)
        .add<ScaleProcessor>(255.0f, 0.0f)
        .add<PersistenceProcessor>(10000, 255.0f, 0.0f)
        .add<TimeoutProcessor>(60000, 0.0f, 0.0f)
        .buildPtr();
    manager.registerSensor("pir_motion", 1, 1, motionPipeline);
    
    // Example 3: Light sensor with smoothing and hysteresis
    auto* lightPipeline = PipelineBuilder("light")
        .add<LowPassFilterProcessor>(0.1f)
        .add<HysteresisProcessor>(500.0f, 400.0f, 255.0f, 0.0f)
        .buildPtr();
    manager.registerSensor("light_sensor", 1, 2, lightPipeline);
    
    // Example 4: Touch sensor with simple debounce and scaling
    auto* touchPipeline = PipelineBuilder("touch")
        .add<DebounceProcessor>(50)
        .add<ScaleProcessor>(255.0f, 0.0f)
        .buildPtr();
    manager.registerSensor("touch_sensor", 1, 3, touchPipeline);
    
    // Example 5: Distance sensor with Kalman filter and inverted mapping
    auto* distancePipeline = PipelineBuilder("distance")
        .add<KalmanFilterProcessor>(0.01f, 0.5f)
        .add<RangeMappingProcessor>(0.0f, 400.0f, 0.0f, 255.0f)
        .add<InvertProcessor>(0.0f, 255.0f)  // Closer = brighter
        .buildPtr();
    manager.registerSensor("vl53l1x_distance", 1, 4, distancePipeline);
    
    manager.printStatus();
}

/**
 * Example: Integration with existing sensor classes
 */
template<typename T>
class SensorWithPipeline : public SensorBase<T> {
    private:
        SensorPipelineManager& pipelineManager;
        std::string sensorName;
        
    protected:
        bool doRead() override {
            // Let derived class handle reading
            return false;
        }
        
    public:
        SensorWithPipeline(const std::string& name, uint8_t pin, unsigned long pullMillis,
                          SensorPipelineManager& manager)
            : SensorBase<T>(pin, pullMillis),
              pipelineManager(manager),
              sensorName(name) {}
        
        void processAndPublish(T value) {
            // Convert value to float for pipeline processing
            float floatValue = static_cast<float>(value);
            pipelineManager.processSensorValue(sensorName, floatValue);
        }
};

/**
 * Example: Using with callbacks
 */
void setupSensorWithCallback(SensorPipelineManager& manager, 
                            AnalogReadSensor& sensor,
                            const std::string& sensorName) {
    // Add callback to process readings through pipeline
    sensor.addOnChangeListener([&manager, sensorName](uint16_t value) {
        manager.processSensorValue(sensorName, static_cast<float>(value));
    });
}

#endif // SENSORPIPELINEINTEGRATION_H
