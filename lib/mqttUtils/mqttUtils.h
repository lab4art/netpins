#pragma once

#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_wifi.h>

void serialPrintln(const char* templateString, ...) {
    va_list args;
    va_start(args, templateString);
    char buffer[256];
    vsnprintf(buffer, 256, templateString, args);
    va_end(args);
    Serial.println(buffer);
}

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
                Serial.println("MQTT DISABLED, host is not defined.");
            } else {
                serialPrintln("Creating MQTT client with host: %s, port: %d, user: %s, topic: %s, clientId: %s", mqttHost.c_str(), mqttPort, user.c_str(), subscribeTopic.c_str(), clientId.c_str());
                mqttClient = new PubSubClient(espClient);
                mqttClient->setServer(host.c_str(), port);
                mqttClient->setCallback(callback);
                Serial.println("MQTT client created.");
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
                        serialPrintln("Attempting MQTT connection as %s ...", clientId.c_str());
                        // Attempt to connect
                        if (mqttClient->connect(clientId.c_str(), user.c_str(), pass.c_str())) {
                            serialPrintln("MQTT connected.");
                            // ... and resubscribe
                            if (topic != nullptr) {
                                // serialPrintln("Subscribing to topic: %s", topic.c_str());
                                if (mqttClient->subscribe(topic.c_str(), /*QoS:1 = at least once */ 1)) {
                                    serialPrintln("Subscribed to topic: %s", topic.c_str());
                                } else {
                                    serialPrintln("Failed to subscribe to topic: %s", topic.c_str());
                                }
                            }
                        } else {
                            serialPrintln("Failed, rc= %s.", mqttClient->state());
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
