#include "DmxOutput.h"

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
    if (dmxPort < 0 || txPin < 0 || enablePin < 0) {
        Log::errorln("DMX Output: Invalid pin configuration");
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
            dmx_write(dmxPort, it->second.data(), DMX_PACKET_SIZE);
            dmx_send(dmxPort);
            dmx_wait_sent(dmxPort, DMX_TIMEOUT_TICK);
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
