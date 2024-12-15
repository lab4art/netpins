#pragma once

#include <TaskScheduler.h>

class HeartbeatBroadcast: public Task {

  private:
    WiFiUDP* udp;
    IPAddress broadcastIp;
    u_int16_t port;
    const char* firmwareVersion;
    std::string hostName;

  public:
    HeartbeatBroadcast(
        WiFiUDP* udp,
        IPAddress broadcastIp,
        u_int16_t port,
        Scheduler* aScheduler,
        const char* firmwareVersion,
        std::string hostName,
        unsigned long interval = 10000):
      udp(udp),
      broadcastIp(broadcastIp),
      port(port),
      firmwareVersion(firmwareVersion),
      hostName(hostName),
      Task(interval, TASK_FOREVER, aScheduler, false) {
        if (interval > 0) {
          this->enable();
        }
    }

    bool Callback() {
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
      return true;
    }
};
