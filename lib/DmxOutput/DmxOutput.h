#pragma once

#include <esp_dmx.h>
#include <Arduino.h>
#include <Log.h>
#include <scheduler.h>
#include <array>
#include <map>

/**
 * DmxOutput handles DMX transmission using MAX485 (RS-485 transceiver)
 * 
 * 
 * Hardware Requirements:
 * - MAX485 or similar RS-485 transceiver
 * - Connections:
 *   - TX pin: UART transmit pin connected to DI (Driver Input) on MAX485
 *   - RX pin: UART receive pin connected to RO (Receiver Output) on MAX485
 *   - Enable pin: DE/RE pins on MAX485 (Driver Enable/Receiver Enable)
 *   - A and B terminals: Connect to DMX+ and DMX- on XLR cable
 */
class DmxOutput : public ScheduledTask {
private:
    int dmxPort;           // UART port number (0, 1, or 2)
    int txPin;             // TX pin for DMX transmission
    int rxPin;             // RX pin (can be set to -1 if not used for receive)
    int enablePin;         // DE/RE enable pin for MAX485
    bool enabled;          // Whether DMX output is enabled
    
    // Reference to shared DMX data and which universe to output
    std::map<uint16_t, std::array<uint8_t, 512>>* dmxDataRef;
    uint16_t outputUniverse;
    
public:
    /**
     * Constructor
     * 
     * @param dmxPort UART port number (0, 1, or 2 for ESP32)
     * @param txPin TX pin number
     * @param rxPin RX pin number (use -1 if only transmitting)
     * @param enablePin Enable pin for MAX485 DE/RE
     * @param universe Which universe to output
     * @param dmxData Reference to shared DMX data map
     */
    DmxOutput(int dmxPort, int txPin, int rxPin, int enablePin, 
              uint16_t universe, std::map<uint16_t, std::array<uint8_t, 512>>& dmxData);
    
    ~DmxOutput();
    
    /**
     * Initialize DMX output
     * 
     * @return true if initialization successful, false otherwise
     */
    bool begin();
    
    /**
     * Stop DMX output and release resources
     */
    void end();
    
    /**
     * Enable or disable DMX output
     * 
     * @param enable true to enable, false to disable
     */
    void setEnabled(bool enable);
    
    /**
     * Check if DMX output is enabled
     * 
     * @return true if enabled, false otherwise
     */
    bool isEnabled() const { return enabled; }
    
    /**
     * ScheduledTask callback - transmits DMX data at scheduled intervals
     */
    void callback() override;
};
