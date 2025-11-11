#pragma once

#include <mqttUtils.h>

class SensorEvents {
    private:
        MqttUtils* mqtt;
        std::string mqttTopicPreffix;
        
        std::map<uint16_t /*universe*/, std::array<uint8_t, 512> /*data*/>& dmxData;
        std::map<std::string /*sensorName*/, SensorMappingCfg> sensorMappings;
    
    public:
        SensorEvents(
                    MqttUtils* mqtt, 
                    std::string mqttTopicPreffix,
                    std::vector<SensorMappingCfg> sensorMappings,
                    std::map<uint16_t, std::array<uint8_t, 512>>& dmxData):
                mqtt(mqtt), 
                mqttTopicPreffix(mqttTopicPreffix),
                dmxData(dmxData) {
            for (const auto& mapping : sensorMappings) {
                this->sensorMappings[mapping.sensorName] = mapping;
            }
        }

        void publish(std::string sensorName, int value, bool local) {
            std::string topic = mqttTopicPreffix + sensorName;
            mqtt->publish(topic.c_str(), std::to_string(value).c_str());
            if (local) {
                auto it = sensorMappings.find(sensorName);
                if (it != sensorMappings.end()) {
                    SensorMappingCfg& mapping = it->second;
                    uint8_t dmxChannel = mapping.dmxCfg.get0BasedChannel();

                    // map value from sensor range to 0-255
                    int mappedValue = map(value, mapping.valueRange.from, mapping.valueRange.to, 0, 255);
                    if (mappedValue < 0) mappedValue = 0;
                    if (mappedValue > 255) mappedValue = 255;
                    dmxData[mapping.dmxCfg.universe][dmxChannel] = static_cast<uint8_t>(mappedValue);
                    // Log.traceln("Published local sensor %s value %d to DMX %d@%d as value %d", 
                    //     sensorName.c_str(), 
                    //     value, 
                    //     mapping.dmxCfg.channel, 
                    //     mapping.dmxCfg.universe, 
                    //     mappedValue);
                }
            }
        }
};
