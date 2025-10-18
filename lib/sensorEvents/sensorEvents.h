#pragma once

#include <mqttUtils.h>

class SensorEvents {
    private:
        MqttUtils* mqtt;
        String mqttTopicPreffix;
        std::map<String /* sensor id */, DmxCfg /* dmx universe/channel */> sensorDmxMapping;
        std::map<uint16_t /*universe*/, std::array<uint8_t, 512> /*data*/> dmxData;
    
    public:
        // Constructor takes mqtt instance and dmx array as reference
        SensorEvents(
                MqttUtils* mqtt, 
                String mqttTopicPreffix
                // std::map<String, DmxCfg> sensorDmxMapping,
                // std::map<uint16_t, std::array<uint8_t, 512>>& dmxData)
        ):
            mqtt(mqtt), 
            mqttTopicPreffix(mqttTopicPreffix)
            // sensorDmxMapping(sensorDmxMapping), 
            // dmxData(dmxData) 
            {
        }

        void publish(const String sensorId, int value, bool local) {
            String topic = mqttTopicPreffix + sensorId;
            mqtt->publish(topic.c_str(), String(value).c_str());
            // if (sensorDmxMapping.find(sensorId) != sensorDmxMapping.end()) {
            //     DmxCfg dmxCfg = sensorDmxMapping[sensorId];
            //     // Log.traceln("Publishing sensor %s value %d to DMX universe %d channel %d", sensorId.c_str(), value, dmxCfg.universe, dmxCfg.channel);
            //     dmxData[dmxCfg.universe][dmxCfg.get0BasedChannel()] = (uint8_t)value; // TODO map sensor value to dmx value
            // }
        }
};
