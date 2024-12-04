#pragma once

#include <esp_wifi.h>
#include <WiFi.h>
#include <ArduinoLog.h>


#define ON_WIFI_EXECUTION_CALLBACK_SIGNATURE std::function<void(String)> wifiExecutionCallback

class WifiUtils {
    private:
        const char* ssid;
        const char* password;
        unsigned long previousMillis = 0;
        boolean connectedCallbackCalled = false;
        unsigned long reconnectInterval = 0;
        // prevent all devices to connect at the same time at boot
        unsigned long reconnectDelay = 0;
        unsigned int connectAttempt = 0;

    public:
        static String macAddress;

        WifiUtils(const char* ssid, const char* password, unsigned long reconnectInterval = 5000, const char* hostname = "", const char* hostnamePrefix = "esp-"): 
              ssid(ssid), 
              password(password),
              reconnectInterval(reconnectInterval) {

            uint8_t mac[6];
            WiFi.macAddress(mac);
            char macBuffer[18]; // "XX:XX:XX:XX:XX:XX\0" requires 18 characters
            sprintf(macBuffer, "%02X-%02X-%02X-%02X-%02X-%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            WifiUtils::macAddress = String(macBuffer);

            if (hostname != "") {
              WiFi.setHostname(hostname);
            } else {
              // mac based hostname
              String hostname = hostnamePrefix + WifiUtils::macAddress;
              WiFi.setHostname(hostname.c_str());
            }
            if (ssid == nullptr || strlen(ssid) == 0 || ssid == "null") {
              Log.noticeln("Starting WiFi in AP mode.");
              WiFi.mode(WIFI_AP);
              String apSsid = "netpins-" + WifiUtils::macAddress;
              WiFi.softAP(apSsid);
              Log.noticeln("AP IP address: %s", WiFi.softAPIP().toString().c_str()); // default IP is 192.168.4.1
            } else {
              WiFi.mode(WIFI_STA);
              WiFi.begin(ssid, password);
            }

            randomSeed(micros());
            reconnectDelay = random(0, 2 * reconnectInterval);
        }

        void tryReconnect(ON_WIFI_EXECUTION_CALLBACK_SIGNATURE) {
            // skip if WiFi is in AP mode
            if (WiFi.getMode() == WIFI_AP) {
              return;
            }

            unsigned long currentMillis = millis();
            // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
            if (currentMillis - previousMillis >= reconnectDelay) {
                if (WiFi.status() != WL_CONNECTED) {
                    reconnectDelay = random(0, reconnectInterval) + reconnectInterval * connectAttempt; // progressive back-off
                    if (reconnectDelay > 120000) { // 2 min
                      reconnectDelay = 120000;
                    }
                    Serial.print(millis());
                    Serial.println(" Reconnecting to WiFi ...");
                    WiFi.reconnect();
                    connectedCallbackCalled = false;
                    connectAttempt++;
                    if (connectAttempt > 15) {
                      ESP.restart();
                    }
                } else {
                  connectAttempt = 0;
                }
                if (!connectedCallbackCalled && WiFi.status() == WL_CONNECTED) {
                  Serial.println("WiFi connected: " + WiFi.localIP().toString());
                  wifiExecutionCallback(WiFi.localIP().toString());
                  connectedCallbackCalled = true;
                }
                previousMillis = currentMillis;
            }
        }
};

String WifiUtils::macAddress = "";
