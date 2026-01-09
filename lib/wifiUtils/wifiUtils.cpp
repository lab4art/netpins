#include "wifiUtils.h"
#include <Log.h>
#include <factoryReset.h>

std::string WifiUtils::macAddress = "";

void WifiUtils::resetReconnectDelay() {
    connectAttempt = 0;
    reconnectDelay = random(reconnectInterval, 2 * reconnectInterval);
}

WifiUtils::WifiUtils(
        const char* ssid, 
        const char* password, 
        static_ip_t staticIp, 
        unsigned long reconnectInterval, 
        unsigned int rebootAfterWiFiFailed, 
        std::function<void()> beforeWiFiReboot, 
        const char* hostname, 
        const char* hostnamePrefix, 
        bool disableReconnect) :
    reconnectInterval(reconnectInterval),
    rebootAfterWiFiFailed(rebootAfterWiFiFailed),
    disableReconnect(disableReconnect),
    beforeWiFiReboot(beforeWiFiReboot) {

    ssidString = ssid;
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macBuffer[18]; // "XX:XX:XX:XX:XX:XX\0" requires 18 characters
    sprintf(macBuffer, "%02X-%02X-%02X-%02X-%02X-%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    WifiUtils::macAddress = std::string(macBuffer);

    WiFi.setHostname(getHostname(hostname).c_str());

    if (ssid == nullptr || strlen(ssid) == 0 || ssid == "null") {
        Log::infoln("Starting WiFi in AP mode.");
        WiFi.mode(WIFI_AP);
        std::string apSsid = "netpins-" + WifiUtils::macAddress;
        WiFi.softAP(apSsid.c_str());
        Log::infoln("AP IP address: %s", WiFi.softAPIP().toString().c_str()); // default IP is 192.168.4.1
    } else {
        WiFi.mode(WIFI_STA);
        if (staticIp.ip != IPAddress(0, 0, 0, 0)) {
            WiFi.config(staticIp.ip, staticIp.gateway, staticIp.subnet, staticIp.dns1);
        }
        WiFi.begin(ssid, password);
    }
    randomSeed(micros());
    resetReconnectDelay();
}

void WifiUtils::tryReconnect(ON_WIFI_EXECUTION_CALLBACK_SIGNATURE) {
    unsigned long currentMillis = millis();
    // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
    if (currentMillis - previousMillis >= reconnectDelay) {
        // no reconnect if AP mode
        if (WiFi.getMode() == WIFI_STA && WiFi.status() != WL_CONNECTED) {
            // Only reconnect if not disabled or if this is the first attempt. Note that the attempts are reset on successful connection.
            if (!disableReconnect || connectAttempt == 0) {
                reconnectDelay = reconnectInterval + reconnectInterval * connectAttempt; // progressive back-off
                if (reconnectDelay > reconnectInterval * 10) {
                    reconnectDelay = reconnectInterval * 10;
                }
                reconnectDelay = reconnectDelay + random(0, reconnectInterval);
                Log::infoln("Reconnecting to WiFi %s ...", ssidString.c_str());
                WiFi.reconnect();
                connectedCallbackCalled = false;
                connectAttempt++;
                if (rebootAfterWiFiFailed > 0 && connectAttempt >= rebootAfterWiFiFailed) {
                    // prevent factory reset by WiFi failure, reset the counter
                    FactoryReset::getInstance().resetCounter(true);
                    if (beforeWiFiReboot != nullptr) {
                        beforeWiFiReboot();
                    }
                    Log::errorln("Too many failed attempts to connect to WiFi. Restarting ...");
                    ESP.restart();
                }
            }
        } else {
            resetReconnectDelay();
        }
        if (!connectedCallbackCalled && WiFi.status() == WL_CONNECTED) {
            IPAddress ip = WiFi.localIP();
            char ipStr[16];
            snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            wifiExecutionCallback(std::string(ipStr));
            connectedCallbackCalled = true;
            resetReconnectDelay();                
        }
        previousMillis = currentMillis;
    }
}

std::string WifiUtils::getHostname(const char* hostname, std::string hostnamePrefix) {
    if (hostname != nullptr && strlen(hostname) > 0) {
        std::string hname = hostname;
        // replace non alpha-numeric characters with '-'
        for (size_t i = 0; i < hname.length(); i++) {
            if (!isalnum(hname[i])) {
                hname[i] = '-';
            }
        }              
        if (hname.length() > 32) {
            hname = hname.substr(0, 32);
        }
        return hname;
    } else {
        // mac based hostname
        return hostnamePrefix + WifiUtils::macAddress;
    }
}
