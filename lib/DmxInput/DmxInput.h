#pragma once

#include <esp_dmx.h>
#include <Arduino.h>
#include <Log.h>
#include <scheduler.h>
#include <array>
#include <map>
#include <functional>

/**
 * DmxInput handles DMX reception using MAX485 (RS-485 transceiver)
 * 
 * This class receives DMX512 data from an external DMX source (like a lighting console)
 * and feeds it into the DmxManager for processing. It provides an alternative to
 * ArtNet input for receiving DMX data directly via wired RS-485 connection.
 * 
 * Hardware Requirements:
 * - MAX485 or similar RS-485 transceiver
 * - Connections:
 *   - TX pin: UART transmit pin connected to DI (Driver Input) on MAX485 (can be -1 if not needed)
 *   - RX pin: UART receive pin connected to RO (Receiver Output) on MAX485
 *   - Enable pin: DE/RE pins on MAX485 (Driver Enable/Receiver Enable)
 *   - A and B terminals: Connect to DMX+ and DMX- on XLR cable from DMX source
 */
class DmxInput : public ScheduledTask {
private:
    int dmxPort;           // UART port number (0, 1, or 2)
    int txPin;             // TX pin (can be set to -1 if not used for transmit)
    int rxPin;             // RX pin for DMX reception
    int enablePin;         // DE/RE enable pin for MAX485
    bool enabled;          // Whether DMX input is enabled
    
    // Reference to shared DMX data and which universe to receive
    std::map<uint16_t, std::array<uint8_t, 512>>* dmxDataRef;
    uint16_t inputUniverse;
    
    // Callback for when DMX data is received
    std::function<void()> onDataReceived;
    
public:
    /**
     * Constructor
     * 
     * @param dmxPort UART port number (0, 1, or 2 for ESP32)
     * @param txPin TX pin number (use -1 if only receiving)
     * @param rxPin RX pin number
     * @param enablePin Enable pin for MAX485 DE/RE
     * @param universe Which universe this input represents
     * @param dmxData Reference to shared DMX data map
     * @param onDataReceived Callback function called when DMX data is received
     */
    DmxInput(int dmxPort, int txPin, int rxPin, int enablePin, 
             uint16_t universe, std::map<uint16_t, std::array<uint8_t, 512>>& dmxData,
             std::function<void()> onDataReceived = nullptr);
    
    ~DmxInput();
    
    /**
     * Initialize DMX input
     * 
     * @return true if initialization successful, false otherwise
     */
    bool begin();
    
    /**
     * Stop DMX input and release resources
     */
    void end();
    
    /**
     * Enable or disable DMX input
     * 
     * @param enable true to enable, false to disable
     */
    void setEnabled(bool enable);
    
    /**
     * Check if DMX input is enabled
     * 
     * @return true if enabled, false otherwise
     */
    bool isEnabled() const { return enabled; }
    
    /**
     * Get the universe this input is mapped to
     * 
     * @return universe number
     */
    uint16_t getUniverse() const { return inputUniverse; }
    
    /**
     * ScheduledTask callback - reads DMX data at scheduled intervals
     */
    void callback() override;
};
