#include "SystemManager.h"
#include <Log.h>
#include <Preferences.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <factoryReset.h>
#include <firmware.h>

SystemManager::SystemManager(SettingsManager<Settings>* settingsManager)
    : settingsManager(settingsManager), 
      uptimeOffset(0), lastCommandReceivedAt(0), maxIdleMillis(0) {
    loadUptimeOffset();
}

SystemManager::~SystemManager() {
}

void SystemManager::initialize(bool forceReset, int factoryResetPin, const char* wifiSsid, const char* wifiPass) {
    FactoryReset::getInstance().evaluate(factoryResetPin);
    
    if (forceReset || FactoryReset::getInstance().shouldReset()) {
        Log::infoln("Factory reset requested, setting defaults ...");
        eraseAllPreferences();
        settingsManager->setDefaults();
        // Set WiFi credentials from config
        settingsManager->getSettings().wifiSsid = wifiSsid;
        settingsManager->getSettings().wifiPass = wifiPass;
        settingsManager->save();
    } else {
        // load settings
        settingsManager->load();
    }

    if (settingsManager->getSettings().wifiSsid.empty() || settingsManager->getSettings().wifiSsid == "null") {
        Log::infoln("Empty settings, setting defaults ...");
        settingsManager->setDefaults();
        // Set WiFi credentials from config
        settingsManager->getSettings().wifiSsid = wifiSsid;
        settingsManager->getSettings().wifiPass = wifiPass;
        settingsManager->save();
    }
    
    Settings settings = settingsManager->getSettings();
    Log::info((std::string("Loaded settings: ") + settings.asJson()).c_str());
    
    // Apply log level from settings (only if >= 0)
    if (settings.logLevel >= 0) {
        Log::setLogLevel(settings.logLevel);
        Log::infoln("Log level set to %d", settings.logLevel);
    } else {
        Log::infoln("Using default log level %d", Log::getLogLevel());
    }
    
    if (settings.maxIdle > 0) {
        maxIdleMillis = settings.maxIdle * 60000;
    }
}

void SystemManager::loadUptimeOffset() {
    Preferences preferences;
    preferences.begin("sys", false);
    uptimeOffset = preferences.getULong("reboot-wifi", 0L);
    if (uptimeOffset != 0) {
        // erase to not count the same offset on reboots
        preferences.putULong("reboot-wifi", 0L);
    }
    preferences.end();
}

void SystemManager::saveUptimeBeforeReboot() {
    Preferences preferences;
    preferences.begin("sys", false);
    preferences.putULong("reboot-wifi", millis() + uptimeOffset);
    preferences.end();
    
    if (onRebootCallback) {
        onRebootCallback();
    }
}

void SystemManager::setMaxIdle(unsigned long minutes) {
    if (minutes > 0) {
        maxIdleMillis = minutes * 60000;
    }
}

bool SystemManager::shouldSleep() const {
    if (maxIdleMillis == 0) {
        return false;
    }
    auto virtualUptime = uptimeOffset + millis();
    return virtualUptime - lastCommandReceivedAt > maxIdleMillis;
}

void SystemManager::eraseAllPreferences() {
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        Log::errorln("Failed to erase NVS: %s\n", esp_err_to_name(err));
    } else {
        Log::infoln("NVS erased successfully.");
        err = nvs_flash_init();
        if (err != ESP_OK) {
            Log::errorln("Failed to initialize NVS: %s", esp_err_to_name(err));
        } else {
            Log::infoln("NVS initialized successfully.");
        }        
    }
}
