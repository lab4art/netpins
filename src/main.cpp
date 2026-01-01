#define _ENABLE_UDP_BROADCAST true
#define _ENABLE_WEBSERVER true

#include <config.h>

#include <NeoPixelBus.h>
#include <ArtnetWiFi.h>

#include <map>
#include <set>
#include <list>
#include <Arduino.h>
#include <Log.h>
#include <HardwareManager.h>
#include <NetworkManager.h>
#include <DmxManager.h>
#include <SystemManager.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <Preferences.h>
#include "firmware.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <uri/UriBraces.h>
#include <settings.h>
#include "heartbeatBroadcast.h"
#include <GeneralUtils.h>
#include <LittleFS.h>
#include <DmxListener.h>
#include <webadmin.h>
#include <factoryReset.h>
#include <Things.h>
#include <sensors.h>
#include <mqttUtils.h>
#include <sensorEvents.h>
#include <pluginFactory.h>
#include <scheduler.h>

HardwareManager* hardwareManager;
NetworkManager* networkManager;
DmxManager* dmxManager;
SystemManager* systemManager;

SettingsManager<Settings>* settingsManager;

SensorEvents* sensorEvents;
WebAdmin* webAdmin;

std::vector<Switchabe*> switchables;

Scheduler* scheduler = new Scheduler();

WebAdmin::CommandResult onSystemCommand(JsonVariant &jsonVariant) {
    systemManager->markCommandReceived();
    FactoryReset::getInstance().resetCounter(true);

    std::string command = jsonVariant["command"].as<std::string>();

    if (command == "sys-config") {
        settingsManager->fromJson(jsonVariant["data"].as<std::string>());
        if (settingsManager->isDirty()) {
            settingsManager->save();
            // Apply log level immediately before reboot
            Log::setLogLevel(settingsManager->getSettings().logLevel);
            return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK_REBOOT, "Saved, rebooting ...", 3000};
        } else {
            return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, "No updates.", -1};
        }
    } else if (command == "sys-config-merge") {
        settingsManager->mergeJson(jsonVariant["data"].as<std::string>());
        if (settingsManager->isDirty()) {
            settingsManager->save();
            // Apply log level immediately before reboot
            Log::setLogLevel(settingsManager->getSettings().logLevel);
            return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK_REBOOT, "Saved, rebooting ...", 3000};
        } else {
            return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, "No updates.", -1};
        }
    } else if (command == "firmware-update" || command == "spiffs-update") {
        FirmwareUpdateParams* params;

        if (command == "firmware-update") {
            params = new FirmwareUpdateParams{jsonVariant["data"]["url"].as<std::string>(), false};
        } else {
            params = new FirmwareUpdateParams{jsonVariant["data"]["url"].as<std::string>(), true};
        }

        xTaskCreate(
            firmwareUpdateTask,   // Task function
            "FirmwareUpdateTask", // Name of the task
            10000,                // Stack size (in words)
            params,               // Task input parameters
            1,                    // Priority of the task
            NULL                  // Task handle
        );
        // Wait for the result from the firmware update task
        FirmwareUpdateResult* updateResult;
        if (xQueueReceive(firmwareUpdateResultQueue, &updateResult, pdMS_TO_TICKS(90000)) != pdTRUE) {
            Log::errorln("Failed to receive update result within 90 seconds.");
            return WebAdmin::CommandResult{WebAdmin::CommandStatus::ERROR, "Update timed-out after 90 seconds.", 3000};
        } else {
            if (updateResult->status == FirmwareUpdateStatus::NO_UPDATES) {
                return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, updateResult->message, -1};
            } else if (updateResult->status == FirmwareUpdateStatus::STARTED) {
                return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, updateResult->message, 30000};  // update takes ~20s
            } else if (updateResult->status == FirmwareUpdateStatus::SUCCESS) {
                return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK_REBOOT, updateResult->message, 3000};
            } else {
                return WebAdmin::CommandResult{WebAdmin::CommandStatus::ERROR, updateResult->message, -1};
            }
        }
    } else if (command == "reboot") {
        return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK_REBOOT, "Rebooting ...", 3000};
    } else if (command == "save-dmx") {
        auto updated = dmxManager->getDmxListener()->storeDmxData(dmxManager->getDmxData());
        return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, updated ? "Saved." : "No updates.", -1};
    } else if (command == "reset-dmx") {
        dmxManager->getDmxListener()->clearDmxData();
        return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, "DMX data cleared.", -1};
    }
    return WebAdmin::CommandResult{WebAdmin::CommandStatus::ERROR, "Unknown command.", -1};
}

HeartbeatBroadcast* heartbeatBroadcast;

DigitalReadSensor* getDigitalReadSensor(int pin) {
    return hardwareManager->getDigitalReadSensor(pin);
}

std::vector<Switchabe*> createThings(Settings& settings) {
    return hardwareManager->createThings(settings, dmxManager->getDmxListener(), scheduler);
}

AnalogReadSensor* getAnalogReadSensor(int pin) {
    return hardwareManager->getAnalogReadSensor(pin);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    if (WAIT_FOR_SERIAL) {
        // reset the counter if in debug mode
        FactoryReset::getInstance().resetCounter(true);
        // wait for serial monitor
        while (!Serial) {
            delay(100);
        }
    }

    Log::info("Booting ...");

    settingsManager = new SettingsManager<Settings>("settings");
    hardwareManager = new HardwareManager();
    systemManager = new SystemManager(settingsManager, nullptr);
    dmxManager = new DmxManager(systemManager->getLastCommandReceivedAtPtr());
    systemManager->dmxManager = dmxManager;
    
    systemManager->loadUptimeOffset();
    systemManager->initialize(FORCE_RESET, FACTORY_REST_PIN, WIFI_SSID, WIFI_PASS);
    
    Settings settings = settingsManager->getSettings();

    try {
        switchables = createThings(settings);
        Log::info("Things created.");
    } catch(const std::exception& e) {
        Log::error((std::string("ERR: creating things. ") + e.what()).c_str());
    }

    Log::info("Mounting LittleFS ...");
    if (!LittleFS.begin()) {
        Log::error("An Error has occurred while mounting LittleFS.");
    }

    if (settings.lightsTest) {
        hardwareManager->runLightsTest(switchables);
    }

    hardwareManager->initNeoStipTask();
    firmwareUpdateResultQueue = xQueueCreate(1, sizeof(int));

    networkManager = new NetworkManager(&settings, scheduler);
    networkManager->initializeWiFi({ STATIC_IP, GATEWAY, SUBNET, DNS }, []() {
        systemManager->saveUptimeBeforeReboot();
    });
    
    if (!settings.disableArtnet) {
        networkManager->initializeArtnet([](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {
            dmxManager->onDmxFrame(data, size, metadata, remote);
        });
        networkManager->configureArtnetReply(WiFi.getHostname(), FIRMWARE_VERSION, dmxManager->getListeningUniverses());
    }

    if (_ENABLE_WEBSERVER) {
        Log::infoln("Starting web server ...");
        webAdmin = new WebAdmin(
            settingsManager,
            onSystemCommand,
            FIRMWARE_VERSION,
            FACTORY_REST_PIN
        );
        webAdmin->setOnReceivedCallback([](){
            systemManager->markCommandReceived();
        });
        Log::infoln("Web server listening on %s", WiFi.localIP().toString().c_str());
    }

    webAdmin->setPropertiesSupplier([](){
        std::map<std::string, std::string> props;
        for (auto& humTempSensor : hardwareManager->getHumTempSensors()) {
            props["temp-" + std::to_string(humTempSensor->getPin())] = std::to_string(humTempSensor->getValue().humidity);
            props["hum-" + std::to_string(humTempSensor->getPin())] = std::to_string(humTempSensor->getValue().temperature);
        }

        std::map<uint16_t /*universe*/, std::array<uint8_t, 512>> storedDmx;
        dmxManager->getDmxListener()->initializeDmxData(storedDmx);
        dmxManager->getDmxListener()->restoreDmxData(storedDmx);
        // convert dmxData to string
        std::string dmxDataStr = "";
        for (auto& universeData : storedDmx) {
            dmxDataStr += "U" + std::to_string(universeData.first) + ":";
            for (int i = 0; i < 512; i++) {
                dmxDataStr += std::to_string(universeData.second[i]);
                if (i < 511) {
                    dmxDataStr += ",";
                }
            }
            dmxDataStr += ";";
        }
        props["stored-dmx"] = dmxDataStr;
        return props;
    });

    std::string hostName = WifiUtils::getHostname(settings.hostname.c_str());
    
    networkManager->initializeMqtt(String(hostName.c_str()), [](char* topic, byte* payload, unsigned int length) {
        Log::infoln("MQTT message received: %s, %s", topic, payload);
    });
    
    sensorEvents = new SensorEvents(
        networkManager->getMqtt(),
        std::string("np/") + hostName + "/s/",
        settings.sensorMappings,
        dmxManager->getDmxData()
    );

    // Initialize sensors after sensorEvents is created
    hardwareManager->initializeSensors(settings, sensorEvents);

    Log::infoln("Running ...");
}

int loopCounter = 0;
int executionTimeSum = 0;
int maxExecutionTime = 0;

uint32_t minFreeHeap = UINT32_MAX;
uint32_t minFreePsram = UINT32_MAX;

void loop() {
    unsigned long loopStartTime = micros();

    FactoryReset::getInstance().resetCounter();

    scheduler->loop();

    dmxManager->processAndCommit(20, []() {
        hardwareManager->commitNeoStip();
    });

    networkManager->tryReconnect([](std::string ip) {
        if (_ENABLE_UDP_BROADCAST) {
            networkManager->initializeHeartbeat(FIRMWARE_VERSION);
        }
    });
    
    networkManager->loop();

    hardwareManager->readAllSensors();

    // idle power off
    if (systemManager->shouldSleep()) {
        auto maxIdleMillis = systemManager->getMaxIdleMillis();
        auto virtualUptime = systemManager->getVirtualUptime();
        auto lastCommandReceivedAt = systemManager->getLastCommandReceivedAt();
        
        Log::infoln("No command received for %d min, going to sleep ...", maxIdleMillis / 60000);
        Log::infoln("maxIdleMillis: %d, lastCommandReceivedAt: %d, virtualUptime %d, millis: %d", maxIdleMillis, lastCommandReceivedAt, virtualUptime, millis());
        
        if (webAdmin != nullptr) {
            webAdmin->end();
        }
        networkManager->shutdown();

        // turn off all switchables
        for (auto& switchable : switchables) {
            switchable->off();
        }
        hardwareManager->commitNeoStip();

        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        esp_deep_sleep_start();
    }

    if (PRINT_EXECUTION_STAT) {
        loopCounter++;
        auto executionTime = micros() - loopStartTime;
        executionTimeSum += executionTime;
        if (executionTime > maxExecutionTime) {
            maxExecutionTime = executionTime;
        }

        if (ESP.getFreeHeap() < minFreeHeap) {
            minFreeHeap = ESP.getFreeHeap();
        }
        
        if (ESP.getFreePsram() < minFreePsram) {
            minFreePsram = ESP.getFreePsram();
        }

        if (loopCounter % 5000 == 0) {
            Log::infoln("Max loop execution time: %d us, avg loop execution time: %d us", maxExecutionTime, executionTimeSum / loopCounter);
            executionTimeSum = 0;
            maxExecutionTime = 0;
            loopCounter = 0;
            Log::infoln("Min free heap: %d, low water mark: %d of %d. Min free psram: %d, low water mark: %d of %d", 
                minFreeHeap, ESP.getMinFreeHeap(), ESP.getHeapSize(), minFreePsram, ESP.getMinFreePsram(), ESP.getPsramSize());
            minFreeHeap = UINT32_MAX;
            minFreePsram = UINT32_MAX;
        }
    }
}
