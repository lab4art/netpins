#include "NetworkManager.h"
#include <Log.h>
#include <esp_wifi.h>

NetworkManager::NetworkManager(Settings* settings, Scheduler* scheduler, String firmwareVersion) 
    : udp(nullptr), wifi(nullptr), artnet(nullptr), mqtt(nullptr), 
      heartbeatBroadcast(nullptr), settings(settings), scheduler(scheduler), firmwareVersion(firmwareVersion) {
}

NetworkManager::~NetworkManager() {
    shutdown();
    
    if (udp != nullptr) {
        delete udp;
    }
    if (wifi != nullptr) {
        delete wifi;
    }
    if (artnet != nullptr) {
        delete artnet;
    }
    if (mqtt != nullptr) {
        delete mqtt;
    }
    if (heartbeatBroadcast != nullptr) {
        delete heartbeatBroadcast;
    }
}

void NetworkManager::initializeWiFi(static_ip_config_t staticIpConfig, std::function<void()> beforeReboot) {
    beforeWiFiRebootCallback = beforeReboot;
    
    Log::info((std::string("Using ssid: ") + settings->wifiSsid).c_str());
    
    static_ip_t staticIp = { staticIpConfig.ip, staticIpConfig.gateway, staticIpConfig.subnet, staticIpConfig.dns1 };
    
    wifi = new WifiUtils(
        settings->wifiSsid.c_str(), 
        settings->wifiPass.c_str(), 
        staticIp, 
        5000,
        settings->rebootAfterWifiFailed,
        beforeReboot,
        settings->hostname.c_str());
    
    Log::info((std::string("Wifi MAC: ") + WifiUtils::macAddress).c_str());
}

void NetworkManager::initializeArtnet(std::function<void(const uint8_t*, uint16_t, const ArtDmxMetadata&, const ArtNetRemoteInfo&)> onDmxFrame, const String& hostname, const std::set<uint16_t>& universes) {
    if (settings->disableArtnet) {
        Log::infoln("Artnet is disabled.");
        return;
    }
    
    artnet = new ArtnetWiFiReceiver();
    artnet->begin();
    artnet->subscribeArtDmx(onDmxFrame);
    artnet->setArtPollReplyConfigShortName("NetPins");
    
    configureArtnetReply(hostname, firmwareVersion, universes);
}

void NetworkManager::configureArtnetReply(const String& hostname, const String& firmwareVersion, const std::set<uint16_t>& universes) {
    if (artnet == nullptr) {
        return;
    }
    
    String universeStr = "";
    bool first = true;
    for (auto universe : universes) {
        if (!first) {
            universeStr += ",";
        }
        universeStr += String(universe);
        first = false;
    }
    
    artnet->setArtPollReplyConfigLongName(
        hostname + " - @" + universeStr + " - " + firmwareVersion
    );
}

void NetworkManager::initializeMqtt(String hostName, std::function<void(char*, byte*, unsigned int)> onMessage) {
    onMqttMessageCallback = onMessage;
    
    mqtt = new MqttUtils(
        settings->mqtt.server.c_str(),
        settings->mqtt.port,
        settings->mqtt.user.c_str(),
        settings->mqtt.password.c_str(),
        (String("np/") + hostName + "/c/#").c_str(),
        hostName,
        onMessage
    );
}

void NetworkManager::initializeHeartbeat() {
    if (settings->udpPort > 0 && settings->hbInt > 0) {
        if (heartbeatBroadcast == nullptr) {
            auto ip = WiFi.localIP();
            IPAddress broadcastIp = IPAddress(ip[0], ip[1], ip[2], 255);
            Log::infoln("Starting UDP heartbeat on %d.%d.%d.255 ...", ip[0], ip[1], ip[2]);
            udp = new WiFiUDP();
            heartbeatBroadcast = new HeartbeatBroadcast(
                udp, 
                broadcastIp, 
                settings->udpPort, 
                firmwareVersion.c_str(), 
                settings->hostname,
                settings->hbInt);
            scheduler->addTask(heartbeatBroadcast);
        }
    }
}

void NetworkManager::loop() {
    tryReconnect();
    
    if (artnet != nullptr) {
        artnet->parse();
    }
    
    if (mqtt != nullptr) {
        mqtt->tryReconnect();
        mqtt->loop();
    }
}

void NetworkManager::tryReconnect() {
    if (wifi != nullptr) {
        wifi->tryReconnect([this](std::string ip) {
            Log::infoln("WiFi connected, IP address: %s.", ip.c_str());
            
            if (settings->disableWifiPowerSave) {
                esp_wifi_set_ps(WIFI_PS_NONE);
            }
            
            initializeHeartbeat();
        });
    }
}

void NetworkManager::shutdown() {
    WiFi.mode(WIFI_OFF);
}
