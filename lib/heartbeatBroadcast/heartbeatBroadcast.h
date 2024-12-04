#pragma once

#include <TaskScheduler.h>

class HeartbeatBroadcast: public Task {

  private:
    WiFiUDP* udp;
    IPAddress broadcastIp;
    u_int16_t port;
    const char* firmwareVersion;

  public:
    HeartbeatBroadcast(
        WiFiUDP* udp,
        IPAddress broadcastIp,
        u_int16_t port,
        Scheduler* aScheduler,
        const char* firmwareVersion,
        unsigned long interval = 10000):
      udp(udp),
      broadcastIp(broadcastIp),
      port(port),
      firmwareVersion(firmwareVersion),
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
      doc["ip"] = WiFi.localIP().toString();
      doc["mac"] = WifiUtils::macAddress;
      serializeJson(doc, output);

      udp->beginPacket(broadcastIp, port);
      udp->print(output.c_str());
      udp->endPacket();
      // Log.noticeln("Broadcast message sent %s.", output.c_str());
      return true;
    }
};
