#include "DmxInput.h"

DmxInput::DmxInput(int dmxPort, int txPin, int rxPin, int enablePin,
                   uint16_t universe, std::map<uint16_t, std::array<uint8_t, 512>>& dmxData,
                   std::function<void()> onDataReceived)
    : ScheduledTask(25, "DmxInput"),  // 25ms = 40Hz refresh rate
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
                memcpy(universeData.data(), data, DMX_PACKET_SIZE);
                
                // Trigger callback if provided
                if (onDataReceived) {
                    onDataReceived();
                }
                // Log::traceln("DMX Input: Received packet for universe %d, first 3 channels: %d, %d, %d",
                //             inputUniverse, data[0], data[1], data[2]);
                // }
            } else {
                // Log error occasionally
                static int errorLogCounter = 0;
                if (++errorLogCounter >= 100) {
                    Log::warningln("DMX Input: Received packet with error");
                    errorLogCounter = 0;
                }
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
