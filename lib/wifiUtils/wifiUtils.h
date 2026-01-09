#ifndef WIFI_UTILS_H
#define WIFI_UTILS_H

#include <esp_wifi.h>
#include <WiFi.h>
#include <string>
#include <functional>

#define ON_WIFI_EXECUTION_CALLBACK_SIGNATURE std::function<void(std::string)> wifiExecutionCallback

struct static_ip_t {
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1;
};

class WifiUtils {
    private:
        std::string ssidString;
        unsigned long previousMillis = 0;
        bool connectedCallbackCalled = false;
        unsigned long reconnectInterval = 0;
        // prevent all devices to connect at the same time at boot
        unsigned long reconnectDelay = 0;
        unsigned int rebootAfterWiFiFailed;
        unsigned int connectAttempt = 0;
        bool disableReconnect = false;
        std::function<void()> beforeWiFiReboot;

        void resetReconnectDelay();

    public:
        static std::string macAddress;

        WifiUtils(const char* ssid, const char* password, static_ip_t staticIp, 
                  unsigned long reconnectInterval, unsigned int rebootAfterWiFiFailed, 
                  std::function<void()> beforeWiFiReboot = nullptr, 
                  const char* hostname = "", const char* hostnamePrefix = "netpins-",
                  bool disableReconnect = false);

        void tryReconnect(ON_WIFI_EXECUTION_CALLBACK_SIGNATURE);

        static std::string getHostname(const char* hostname, std::string hostnamePrefix = "netpins-");
};

#endif // WIFI_UTILS_H
