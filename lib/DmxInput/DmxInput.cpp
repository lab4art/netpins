#include "DmxInput.h"
#include <soc/soc_caps.h>

namespace {
constexpr TickType_t DMX_INPUT_WAIT_TICKS = dmx_ms_to_ticks(30);
constexpr TickType_t DMX_INPUT_DRAIN_TICKS = 0;
constexpr size_t DMX_INPUT_CHANNELS_TO_READ = 256;
constexpr size_t DMX_INPUT_BYTES_TO_READ = DMX_INPUT_CHANNELS_TO_READ + 1;  // Start code + 256 channels.
constexpr unsigned long DMX_INPUT_CALLBACK_MIN_INTERVAL_MS = 20;
}

DmxInput::DmxInput(int dmxPort, int txPin, int rxPin, int enablePin,
                   uint16_t universe, std::map<uint16_t, std::array<uint8_t, 512>>& dmxData,
                   std::function<void()> onDataReceived)
    : ScheduledTask(0, "DmxInput"),  // 0 = always run, it's non-blocking
      dmxPort(dmxPort),
      txPin(txPin),
      rxPin(rxPin),
      enablePin(enablePin),
      enabled(false),
      dmxDataRef(&dmxData),
      inputUniverse(universe),
      onDataReceived(onDataReceived) {
}

DmxInput::~DmxInput() {
    end();
}

bool DmxInput::begin() {
    if (dmxPort < 0 || rxPin < 0) {
        Log::errorln("DMX Input: Invalid pin configuration");
        return false;
    }

    if (enablePin < 0 && enablePin != DMX_PIN_NO_CHANGE) {
        Log::errorln("DMX Input: Invalid enable pin %d (use -1 for no direction control)", enablePin);
        return false;
    }

    if (dmxPort >= SOC_UART_NUM) {
        Log::errorln("DMX Input: Invalid UART%d for this board (available: 0-%d)",
                     dmxPort, SOC_UART_NUM - 1);
        return false;
    }
    
    Log::infoln("DMX Input: Initializing on UART%d (TX: %d, RX: %d, EN: %d)", 
                dmxPort, txPin, rxPin, enablePin);
    
    // Configure DMX UART
    dmx_config_t config = DMX_CONFIG_DEFAULT;
    
    // Set pins
    dmx_personality_t personalities[] = {};
    int personality_count = 0;
    
    // Install DMX driver
    if (!dmx_driver_install(dmxPort, &config, personalities, personality_count)) {
        Log::errorln("DMX Input: Failed to install driver");
        return false;
    }
    
    // Set communication pins
    if (!dmx_set_pin(dmxPort, txPin, rxPin, enablePin)) {
        Log::errorln("DMX Input: Failed to set pins");
        dmx_driver_delete(dmxPort);
        return false;
    }
    
    Log::infoln("DMX Input: Driver installed successfully for universe %d", inputUniverse);
    enabled = true;
    return true;
}

void DmxInput::end() {
    if (enabled) {
        dmx_driver_delete(dmxPort);
        enabled = false;
        Log::infoln("DMX Input: Driver deleted");
    }
}

void DmxInput::callback() {
    if (!enabled) {
        Log::warningln("DmxInput callback: enabled is FALSE, returning early");
        return;
    }
    bool hasNewData = false;
    dmx_packet_t packet;

    bool hasPacket = dmx_receive_num(dmxPort, &packet, DMX_INPUT_BYTES_TO_READ, DMX_INPUT_WAIT_TICKS);
    while (hasPacket) {
        if (!packet.err) {
            // Read fixed payload: start code + first 256 channels.
            uint8_t data[DMX_PACKET_SIZE] = {0};
            size_t bytesToRead = DMX_INPUT_BYTES_TO_READ;
            size_t bytesRead = dmx_read(dmxPort, data, bytesToRead);

            // Restore shared DMX state updates (start code is data[0]).
            auto& universeData = (*dmxDataRef)[inputUniverse];
            memset(universeData.data(), 0, universeData.size());
            if (bytesRead > 1) {
                size_t slotsToCopy = bytesRead - 1;
                if (slotsToCopy > universeData.size()) {
                    slotsToCopy = universeData.size();
                }
                memcpy(universeData.data(), &data[1], slotsToCopy);
            }

            lastCh1 = (bytesRead > 1) ? data[1] : 0;
            lastCh2 = (bytesRead > 2) ? data[2] : 0;
            lastCh3 = (bytesRead > 3) ? data[3] : 0;
            ++packetsSinceLastLog;
            hasNewData = true;
        } else {
            ++errorsSinceLastLog;
        }

        hasPacket = dmx_receive_num(dmxPort, &packet, DMX_INPUT_BYTES_TO_READ, DMX_INPUT_DRAIN_TICKS);
    }

    // Notify activity, but throttle callback to avoid excessive loop overhead.
    unsigned long now = millis();
    if (hasNewData && onDataReceived &&
        (now - lastOnDataCallbackMs >= DMX_INPUT_CALLBACK_MIN_INTERVAL_MS)) {
        onDataReceived();
        lastOnDataCallbackMs = now;
    }

    // Rate-limit diagnostics to avoid slowing down packet processing.
    if (Log::getLogLevel() >= LOG_LEVEL_TRACE && now - lastStatsLogMs >= 1000 &&
        (packetsSinceLastLog > 0 || errorsSinceLastLog > 0)) {
        Log::traceln("DMX Input: uni %d, rx=%u pkt/s, err=%u/s, ch1..3=%d,%d,%d",
                     inputUniverse, packetsSinceLastLog, errorsSinceLastLog,
                     lastCh1, lastCh2, lastCh3);
        packetsSinceLastLog = 0;
        errorsSinceLastLog = 0;
        lastStatsLogMs = now;
    }
}

void DmxInput::setEnabled(bool enable) {
    if (enable && !enabled) {
        begin();
    } else if (!enable && enabled) {
        end();
    }
}
