#pragma once

#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_wifi.h>
#include <Log.h>

class MqttUtils {
    private:
        String host;
        uint16_t port;
        String clientId;
        String topic;
        String user;
        String pass;
        unsigned long previousMillis = 0;
        WiFiClient espClient;
        PubSubClient* mqttClient;

    public:
        MqttUtils(String mqttHost, uint16_t mqttPort, String user, String pass, String subscribeTopic, String clientId, MQTT_CALLBACK_SIGNATURE):
                host(mqttHost),
                port(mqttPort),
                topic(subscribeTopic),
                user(user),
                pass(pass),
                clientId(clientId) {
            if (mqttHost == nullptr || mqttHost.isEmpty()) {
                Log::info("MQTT DISABLED, host is not defined.");
                mqttClient = nullptr;
            } else {
                Log::infoln("Creating MQTT client with host: %s, port: %d, user: %s, topic: %s, clientId: %s", mqttHost.c_str(), mqttPort, user.c_str(), subscribeTopic.c_str(), clientId.c_str());
                mqttClient = new PubSubClient(espClient);
                mqttClient->setServer(host.c_str(), port);
                mqttClient->setCallback(callback);
                Log::info("MQTT client created.");
            }
        }

        void loop() {
            if (mqttClient != nullptr) {
                mqttClient->loop();
            }
        }

        void tryReconnect(unsigned long interval = 10000) {
            if (mqttClient == nullptr) {
                return;
            }
            unsigned long currentMillis = millis();
            // if MQTT is down, try reconnecting every interval seconds
            if (currentMillis - previousMillis >=interval) {
                if (WiFi.status() == WL_CONNECTED && WiFi.getMode() == WIFI_STA) {
                    if ((!mqttClient->connected())) {
                        Log::infoln("Attempting MQTT connection as %s ...", clientId.c_str());
                        // Attempt to connect
                        if (mqttClient->connect(clientId.c_str(), user.c_str(), pass.c_str())) {
                            Log::infoln("MQTT connected.");
                            // ... and resubscribe
                            if (topic != nullptr) {
                                // Log::infoln("Subscribing to topic: %s", topic.c_str());
                                if (mqttClient->subscribe(topic.c_str(), /*QoS:1 = at least once */ 1)) {
                                    Log::infoln("Subscribed to topic: %s", topic.c_str());
                                } else {
                                    Log::errorln("Failed to subscribe to topic: %s", topic.c_str());
                                }
                            }
                        } else {
                            Log::errorln("Failed, rc= %s.", mqttClient->state());
                        }
                    }
                } else {
                    // serialPrintln("WiFi is down, skipping MQTT reconnect");
                }
                previousMillis = currentMillis;
            }
        }

        void publish(const char* topic, const char* payload) {
            if (mqttClient != nullptr) {
                mqttClient->publish(topic, payload);
            }
        }
        
        PubSubClient* getClient() {
            return mqttClient;
        }
};

// class MqttLogTarget : public Print {

//   private:
//     String buffer = "";
//     MqttUtils* mqtt;
//     const char* topic;
//     bool logToMqtt = false;
  
//   public:
//     MqttLogTarget(
//       MqttUtils* mqtt,
//       const char* topic):
//       mqtt(mqtt), topic(topic) {
//     }

//     size_t write(uint8_t character) {
//       if (logToMqtt) {
//         buffer += (char)character;
//         if (character == '\n') {
//           this->mqtt->publish(topic, buffer.c_str());
//           buffer = "";
//         }
//       }
//       return Serial.write(character);
//     }

//     void enableMqttLogging() {
//       logToMqtt = true;
//     }

//     void disableMqttLogging() {
//       logToMqtt = false;
//     }
// };
