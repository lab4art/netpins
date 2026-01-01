#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
#include <WiFiUdp.h>
#include <set>
#include <ArtnetWiFi.h>
#include <mqttUtils.h>
#include <settings.h>
#include <scheduler.h>
#include <wifiUtils.h>
#include "heartbeatBroadcast.h"
#include <functional>

struct static_ip_config_t {
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1;
};

class NetworkManager {
private:
    WiFiUDP* udp;
    WifiUtils* wifi;
    ArtnetWiFiReceiver* artnet;
    MqttUtils* mqtt;
    HeartbeatBroadcast* heartbeatBroadcast;
    
    Settings* settings;
    Scheduler* scheduler;
    String firmwareVersion;
    
    std::function<void()> beforeWiFiRebootCallback;
    std::function<void(char*, byte*, unsigned int)> onMqttMessageCallback;
    
    void initializeHeartbeat();
    void configureArtnetReply(const String& hostname, const String& firmwareVersion, const std::set<uint16_t>& universes);
    void tryReconnect();
    
public:
    NetworkManager(Settings* settings, Scheduler* scheduler, String firmwareVersion);
    ~NetworkManager();
    
    // Initialization
    void initializeWiFi(static_ip_config_t staticIpConfig, std::function<void()> beforeReboot);
    void initializeArtnet(std::function<void(const uint8_t*, uint16_t, const ArtDmxMetadata&, const ArtNetRemoteInfo&)> onDmxFrame, const String& hostname, const std::set<uint16_t>& universes);
    void initializeMqtt(String hostName, std::function<void(char*, byte*, unsigned int)> onMessage);
    
    // Loop and reconnection
    void loop();
    
    // Accessors
    MqttUtils* getMqtt() { return mqtt; }
    ArtnetWiFiReceiver* getArtnet() { return artnet; }
    WifiUtils* getWifi() { return wifi; }
    
    // Shutdown
    void shutdown();
};

#endif // NETWORK_MANAGER_H
