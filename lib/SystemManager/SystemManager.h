#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <string>
#include <functional>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <settings.h>

class SystemManager {
private:
    SettingsManager<Settings>* settingsManager;
    unsigned long uptimeOffset;
    unsigned long lastCommandReceivedAt;
    unsigned long maxIdleMillis;
    
    std::function<void()> onRebootCallback;
    
    void loadUptimeOffset();
    
public:
    
    SystemManager(SettingsManager<Settings>* settingsManager);
    ~SystemManager();
    
    // Initialization
    void initialize(bool forceReset, int factoryResetPin, const char* wifiSsid, const char* wifiPass);
    
    // Uptime management
    void saveUptimeBeforeReboot();
    unsigned long getVirtualUptime() const { return uptimeOffset + millis(); }
    unsigned long getUptimeOffset() const { return uptimeOffset; }
    
    // Command tracking
    void markCommandReceived() { lastCommandReceivedAt = millis(); }
    unsigned long getLastCommandReceivedAt() const { return lastCommandReceivedAt; }
    unsigned long* getLastCommandReceivedAtPtr() { return &lastCommandReceivedAt; }
    
    // Idle timeout
    void setMaxIdle(unsigned long minutes);
    bool shouldSleep() const;
    unsigned long getMaxIdleMillis() const { return maxIdleMillis; }
    
    // Preferences management
    static void eraseAllPreferences();
    
    // Callbacks
    void setOnRebootCallback(std::function<void()> callback) { onRebootCallback = callback; }
};

#endif // SYSTEM_MANAGER_H
