#pragma once

#include <esp_wifi.h>
#include <WiFi.h>
#include <ArduinoLog.h>
#include <factoryReset.h>


#define ON_WIFI_EXECUTION_CALLBACK_SIGNATURE std::function<void(String)> wifiExecutionCallback

struct static_ip_t {
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1;
};
class WifiUtils {
    private:
        String ssidString;
        unsigned long previousMillis = 0;
        boolean connectedCallbackCalled = false;
        unsigned long reconnectInterval = 0;
        // prevent all devices to connect at the same time at boot
        unsigned long reconnectDelay = 0;
        unsigned int rebootAfterWiFiFailed;
        unsigned int connectAttempt = 0;
        std::function<void()> beforeWiFiReboot;

    public:
        static String macAddress;

        WifiUtils(const char* ssid, const char* password, static_ip_t staticIp, unsigned long reconnectInterval, unsigned int rebootAfterWiFiFailed, std::function<void()> beforeWiFiReboot = nullptr, const char* hostname = "", const char* hostnamePrefix = "esp-") : 
                reconnectInterval(reconnectInterval),
                rebootAfterWiFiFailed(rebootAfterWiFiFailed),
                beforeWiFiReboot(beforeWiFiReboot) {

            ssidString = ssid;
            uint8_t mac[6];
            WiFi.macAddress(mac);
            char macBuffer[18]; // "XX:XX:XX:XX:XX:XX\0" requires 18 characters
            sprintf(macBuffer, "%02X-%02X-%02X-%02X-%02X-%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            WifiUtils::macAddress = String(macBuffer);

            if (hostname != "") {
              String hname = hostname;
              // replace non alpha-numeric characters with '-'
              for (int i = 0; i < hname.length(); i++) {
                  if (!isalnum(hname[i])) {
                      hname[i] = '-';
                  }
              }              
              hname = hname.substring(0, 32);
              WiFi.setHostname(hname.c_str());
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

              if (staticIp.ip != IPAddress(0, 0, 0, 0)) {
                WiFi.config(staticIp.ip, staticIp.gateway, staticIp.subnet, staticIp.dns1);
              }

              WiFi.begin(ssid, password);
            }

            randomSeed(micros());
            reconnectDelay = random(reconnectInterval, 2 * reconnectInterval);
        }

        void tryReconnect(ON_WIFI_EXECUTION_CALLBACK_SIGNATURE) {
            unsigned long currentMillis = millis();
            // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
            if (currentMillis - previousMillis >= reconnectDelay) {
                // no reconnect if AP mode
                if (WiFi.getMode() == WIFI_STA && WiFi.status() != WL_CONNECTED) {
                    reconnectDelay = reconnectInterval + reconnectInterval * connectAttempt; // progressive back-off
                    if (reconnectDelay > reconnectInterval * 10) {
                        reconnectDelay = reconnectInterval * 10;
                    }
                    reconnectDelay = reconnectDelay + random(0, reconnectInterval);
                    Log.noticeln("Reconnecting to WiFi %s ...", ssidString.c_str());
                    WiFi.reconnect();
                    connectedCallbackCalled = false;
                    connectAttempt++;
                    if (rebootAfterWiFiFailed > 0 && connectAttempt >= rebootAfterWiFiFailed) {
                      // prevent factory reset by WiFi failure, reset the counter
                      FactoryReset::getInstance().resetCounter(true);
                      if (beforeWiFiReboot != nullptr) {
                        beforeWiFiReboot();
                      }
                      Log.errorln("Too many failed attempts to connect to WiFi. Restarting ...");
                      ESP.restart();
                    }
                } else {
                  connectAttempt = 0;
                }
                if (!connectedCallbackCalled && WiFi.status() == WL_CONNECTED) {
                  wifiExecutionCallback(WiFi.localIP().toString());
                  connectedCallbackCalled = true;
                }
                previousMillis = currentMillis;
            }
        }
};

String WifiUtils::macAddress = "";
