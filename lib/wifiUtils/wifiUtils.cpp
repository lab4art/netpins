#pragma once

#include <esp_wifi.h>
#include <WiFi.h>
#include <ArduinoLog.h>
#include <Preferences.h>


#define ON_WIFI_EXECUTION_CALLBACK_SIGNATURE std::function<void(String)> wifiExecutionCallback

struct static_ip_t {
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1;
};
class WifiUtils {
    private:
        const char* ssid;
        const char* password;
        unsigned long previousMillis = 0;
        boolean connectedCallbackCalled = false;
        unsigned long reconnectInterval = 0;
        // prevent all devices to connect at the same time at boot
        unsigned long reconnectDelay = 0;
        unsigned int rebootAfterWiFiFailed;
        unsigned int connectAttempt = 0;
        unsigned long uptimeOffset;

    public:
        static String macAddress;

        WifiUtils(const char* ssid, const char* password, static_ip_t staticIp, unsigned long reconnectInterval, unsigned int rebootAfterWiFiFailed, unsigned long uptimeOffset, const char* hostname = "", const char* hostnamePrefix = "esp-") : 
              ssid(ssid), 
              password(password),
              reconnectInterval(reconnectInterval),
              rebootAfterWiFiFailed(rebootAfterWiFiFailed),
              uptimeOffset(uptimeOffset) {

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
                    Log.noticeln("Reconnecting to WiFi ...");
                    WiFi.reconnect();
                    connectedCallbackCalled = false;
                    connectAttempt++;
                    if (rebootAfterWiFiFailed > 0 && connectAttempt >= rebootAfterWiFiFailed) { // keep enough reties to not trigger factory reset
                      // store the uptime to preferences
                      Preferences preferences;
                      preferences.begin("sys", false);
                      preferences.putULong("reboot-wifi", millis() + uptimeOffset);
                      preferences.end();
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
