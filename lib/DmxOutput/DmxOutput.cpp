#include "DmxOutput.h"
#include <soc/soc_caps.h>
#include <cstring>

namespace {
constexpr TickType_t DMX_OUTPUT_WAIT_TICKS = dmx_ms_to_ticks(30);
}

DmxOutput::DmxOutput(int dmxPort, int txPin, int rxPin, int enablePin,
                     uint16_t universe, std::map<uint16_t, std::array<uint8_t, 512>>& dmxData)
    : ScheduledTask(25, "DmxOutput"),  // 25ms = 40Hz refresh rate
      dmxPort(dmxPort),
      txPin(txPin),
      rxPin(rxPin),
      enablePin(enablePin),
      enabled(false),
      dmxDataRef(&dmxData),
      outputUniverse(universe) {
}

DmxOutput::~DmxOutput() {
    end();
}

bool DmxOutput::begin() {
    if (dmxPort < 0 || txPin < 0) {
        Log::errorln("DMX Output: Invalid pin configuration");
        return false;
    }

    if (enablePin < 0 && enablePin != DMX_PIN_NO_CHANGE) {
        Log::errorln("DMX Output: Invalid enable pin %d (use -1 for no direction control)", enablePin);
        return false;
    }

    if (dmxPort >= SOC_UART_NUM) {
        Log::errorln("DMX Output: Invalid UART%d for this board (available: 0-%d)",
                     dmxPort, SOC_UART_NUM - 1);
        return false;
    }
    
    Log::infoln("DMX Output: Initializing on UART%d (TX: %d, RX: %d, EN: %d)", 
                dmxPort, txPin, rxPin, enablePin);
    
    // Configure DMX UART
    dmx_config_t config = DMX_CONFIG_DEFAULT;
    
    // Set pins
    dmx_personality_t personalities[] = {};
    int personality_count = 0;
    
    // Install DMX driver
    if (!dmx_driver_install(dmxPort, &config, personalities, personality_count)) {
        Log::errorln("DMX Output: Failed to install driver");
        return false;
    }
    
    // Set communication pins
    if (!dmx_set_pin(dmxPort, txPin, rxPin, enablePin)) {
        Log::errorln("DMX Output: Failed to set pins");
        dmx_driver_delete(dmxPort);
        return false;
    }
    
    Log::infoln("DMX Output: Driver installed successfully");
    enabled = true;
    return true;
}

void DmxOutput::end() {
    if (enabled) {
        dmx_driver_delete(dmxPort);
        enabled = false;
        Log::infoln("DMX Output: Driver deleted");
    }
}

void DmxOutput::callback() {
    if (!enabled) {
        return;
    }
    
    // Get data from shared dmxData for our universe and transmit
    if (dmxDataRef != nullptr) {
        auto it = dmxDataRef->find(outputUniverse);
        if (it != dmxDataRef->end()) {
            // Build a proper DMX frame with start code at index 0.
            uint8_t frame[DMX_PACKET_SIZE] = {0};
            frame[0] = 0;  // Default DMX start code.
            std::memcpy(&frame[1], it->second.data(), it->second.size());

            size_t written = dmx_write(dmxPort, frame, DMX_PACKET_SIZE);
            bool sendOk = dmx_send(dmxPort) > 0;
            bool waitOk = dmx_wait_sent(dmxPort, DMX_OUTPUT_WAIT_TICKS);

            lastCh1 = it->second[0];
            lastCh2 = it->second[1];
            lastCh3 = it->second[2];

            if (written == DMX_PACKET_SIZE && sendOk && waitOk) {
                ++framesSentSinceLastLog;
            } else {
                ++sendErrorsSinceLastLog;
            }

            unsigned long now = millis();
            bool debugEnabled = Log::getLogLevel() >= LOG_LEVEL_TRACE;
            if (debugEnabled && now - lastStatsLogMs >= 1000 &&
                (framesSentSinceLastLog > 0 || sendErrorsSinceLastLog > 0)) {
                Log::traceln("DMX Output: uni %d, tx=%u pkt/s, err=%u/s, ch1..3=%d,%d,%d",
                             outputUniverse,
                             framesSentSinceLastLog,
                             sendErrorsSinceLastLog,
                             lastCh1, lastCh2, lastCh3);
                framesSentSinceLastLog = 0;
                sendErrorsSinceLastLog = 0;
                lastStatsLogMs = now;
            }
        }
    }
}

void DmxOutput::setEnabled(bool enable) {
    if (enable && !enabled) {
        begin();
    } else if (!enable && enabled) {
        end();
    }
}
