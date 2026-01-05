#pragma once

#include <mqttUtils.h>
#include <SensorEventProcessor.h>

class SensorEvents {
    private:
        MqttUtils* mqtt;
        std::string mqttTopicPreffix;
        
        std::map<uint16_t /*universe*/, std::array<uint8_t, 512> /*data*/>& dmxData;
        const std::vector<SensorMappingCfg>& sensorMappings;
        
        // Optional event processor for conditional processing
        SensorEventProcessor* eventProcessor;
    
    public:
        SensorEvents(
                    MqttUtils* mqtt, 
                    std::string mqttTopicPreffix,
                    const std::vector<SensorMappingCfg>& sensorMappings,
                    std::map<uint16_t, std::array<uint8_t, 512>>& dmxData,
                    SensorEventProcessor* processor = nullptr):
                mqtt(mqtt), 
                mqttTopicPreffix(mqttTopicPreffix),
                sensorMappings(sensorMappings),
                dmxData(dmxData),
                eventProcessor(processor) {
        }

        void publish(std::string sensorName, int value, bool local) {
            std::string topic = mqttTopicPreffix + sensorName;
            mqtt->publish(topic.c_str(), std::to_string(value).c_str());
            
            if (local && eventProcessor != nullptr) {
                bool shouldProcess = eventProcessor->addEvent(sensorName, value);
                if (shouldProcess) {
                    ProcessingResult result = eventProcessor->process();
                    if (result.shouldSetDmx) {
                        applyProcessedValues(result.values);
                    }
                }
            }
        }

        /**
         * Force processing of collected events (if using processor)
         */
        void forceProcess() {
            if (eventProcessor != nullptr && eventProcessor->getEventCount() > 0) {
                ProcessingResult result = eventProcessor->process();
                if (result.shouldSetDmx) {
                    applyProcessedValues(result.values);
                }
            }
        }

    private:
        /**
         * Apply processed values to DMX data
         * Processor handles all value transformations
         */
        void applyProcessedValues(const std::map<std::string, int>& processedValues) {
            for (auto it = processedValues.begin(); it != processedValues.end(); ++it) {
                const std::string& sensorName = it->first;
                int dmxValue = it->second;
                
                for (const auto& mapping : sensorMappings) {
                    if (mapping.sensorName == sensorName) {
                        uint8_t dmxChannel = mapping.dmxCfg.get0BasedChannel();
                        
                        // Clamp to DMX range
                        if (dmxValue < 0) dmxValue = 0;
                        if (dmxValue > 255) dmxValue = 255;
                        
                        dmxData[mapping.dmxCfg.universe][dmxChannel] = static_cast<uint8_t>(dmxValue);
                        break;
                    }
                }
            }
        }
};
