#pragma once

#include <scheduler.h>

class HeartbeatBroadcast: public ScheduledTask {

    private:
        WiFiUDP* udp;
        IPAddress broadcastIp;
        u_int16_t port;
        const char* firmwareVersion;
        std::string hostName;
        unsigned long interval;
        unsigned long lastExecution = 0;

        void send() {
            String output;
            JsonDocument doc;
            doc["uptime"] = millis();
            doc["firmwareVersion"] = firmwareVersion;
            doc["mac"] = WifiUtils::macAddress;
            doc["ip"] = WiFi.localIP().toString();
            doc["hostname"] = this->hostName;
            serializeJson(doc, output);

            udp->beginPacket(broadcastIp, port);
            udp->print(output.c_str());
            udp->endPacket();
            // Log.noticeln("Broadcast message sent %s.", output.c_str());
        }

    public:
        HeartbeatBroadcast(
                WiFiUDP* udp,
                IPAddress broadcastIp,
                u_int16_t port,
                const char* firmwareVersion,
                std::string hostName,
                long interval = 10000):
            udp(udp),
            broadcastIp(broadcastIp),
            port(port),
            firmwareVersion(firmwareVersion),
            hostName(hostName),
            interval(interval),
            ScheduledTask(interval, "HeartbeatBroadcast") {
        }

        void callback() override {
            this->send();
        }

};
