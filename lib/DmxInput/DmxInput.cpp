#include "DmxInput.h"

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
    if (dmxPort < 0 || rxPin < 0 || enablePin < 0) {
        Log::errorln("DMX Input: Invalid pin configuration");
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
    // Receive DMX data
    if (dmxDataRef != nullptr) {
        dmx_packet_t packet;
        
        // Try to receive a DMX packet (non-blocking with timeout)
        if (dmx_receive(dmxPort, &packet, DMX_TIMEOUT_TICK)) {
            // Check if we got a valid DMX packet
            if (!packet.err) {
                // Get the data
                uint8_t data[DMX_PACKET_SIZE];
                dmx_read(dmxPort, data, DMX_PACKET_SIZE);
                
                // Copy to our shared dmxData for the specified universe
                auto& universeData = (*dmxDataRef)[inputUniverse];

                // copy data without the first byte (start code)
                memcpy(universeData.data(), &data[1], 512);
                
                // Trigger callback if provided
                if (onDataReceived) {
                    onDataReceived();
                }
                // Log::infoln("DMX Input: Received uni %d, first 3 channels: %d, %d, %d", inputUniverse, data[1], data[2], data[3]);
            } else {
                Log::errorln("DMX Input: Error receiving DMX packet, err code: %d", packet.err);
            }
        }
    }
}

void DmxInput::setEnabled(bool enable) {
    if (enable && !enabled) {
        begin();
    } else if (!enable && enabled) {
        end();
    }
}
