#pragma once

#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_wifi.h>


class MqttUtils {
    private:
        String clientId;
        String topic;
        String user;
        String pass;
        unsigned long previousMillis = 0;
        WiFiClient espClient;
        PubSubClient* mqttClient;

    public:
        MqttUtils(const char* mqtt_server, const char* user, const char* pass, const char* topic, MQTT_CALLBACK_SIGNATURE):
                topic(topic),
                user(user),
                pass(pass) {
            mqttClient = new PubSubClient(espClient);

            mqttClient->setServer(mqtt_server, 1883);
            mqttClient->setCallback(callback);
            clientId = String("ESP-") + WifiUtils::macAddress; //TODO use hostname for clietnId
            // serialPrintln("MQTT server: %s.", mqtt_server);
            // Serial.println(String("Topic: ") + topic);
            // Serial.println(String("this->topic: ") + this->topic);
            // Serial.println(String("MQTT server: ") + mqtt_server);
            // Serial.println(String("MQTT client id: ") + clientId);
        }

        void loop() {
            mqttClient->loop();
        }

        // void reconnectBlocking() {
        // // Loop until we're reconnected
        //     while (!mqttClient->connected()) {
        //         serialPrintln("Attempting blocking MQTT connection as %s ...", clientId.c_str());
        //         // Attempt to connect
        //         if (mqttClient->connect(clientId.c_str(), user.c_str(), pass.c_str())) {
        //             serialPrintln("connected.");
        //             // ... and resubscribe
        //             if (topic != NULL) {
        //                 mqttClient->subscribe(topic.c_str(), /*QoS:1 = at least once */ 1);
        //             }
        //         } else {
        //             serialPrintln("Failed, rc=%s.", mqttClient->state());
        //             // Wait 2 seconds before retrying
        //             delay(2000);
        //         }
        //     }
        // }

        void tryReconnect(unsigned long interval = 10000) {
            unsigned long currentMillis = millis();
            // if MQTT is down, try reconnecting every interval seconds
            if ((!mqttClient->connected()) && (currentMillis - previousMillis >=interval)) {
                // if WiFi is down skip mqtt reconnect
                if (WiFi.status() == WL_CONNECTED) {
                    // serialPrintln("Attempting MQTT connection as %s ...", clientId.c_str());
                    // Attempt to connect
                    if (mqttClient->connect(clientId.c_str(), user.c_str(), pass.c_str())) {
                        // serialPrintln("Connected.");
                        // ... and resubscribe
                        if (topic != NULL) {
                            // serialPrintln("Subscribing to topic: %s", topic.c_str());
                            if (mqttClient->subscribe(topic.c_str(), /*QoS:1 = at least once */ 1)) {
                                // serialPrintln("Subscribed to topic: %s", topic.c_str());
                            } else {
                                // serialPrintln("Failed to subscribe to topic: %s", topic.c_str());
                            }
                            // auto sysTopic = "/system/" + WifiUtils::macAddress + "/#";
                            // serialPrintln("Subscribing to topic: %s", sysTopic.c_str());
                            // if (mqttClient->subscribe(sysTopic.c_str(), /*QoS:1 = at least once */ 1)) { //TODO
                            if (mqttClient->subscribe("sysTopic.c_str()", /*QoS:1 = at least once */ 1)) {
                                // serialPrintln("Subscribed to topic: %s", sysTopic.c_str());
                            } else {
                                // serialPrintln("Failed to subscribe to topic: %s", sysTopic.c_str());
                            }
                        }
                    } else {
                        // serialPrintln("Failed, rc= %s.", mqttClient->state());
                    }
                } else {
                    // serialPrintln("WiFi is down, skipping MQTT reconnect");
                }
                previousMillis = currentMillis;
            }
        }

        void publish(const char* topic, const char* payload) {
            if (mqttClient->connected()) {
                //Log.traceln("Publishing to topic: %s, payload: %s", topic, payload);
                mqttClient->publish(topic, payload);
            } else {
                //Log.traceln("Mqtt NOT connected, dropping: topic: %s, payload: %s", topic, payload);
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
