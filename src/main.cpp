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
#include <webadmin.h>
#include <factoryReset.h>
#include <Things.h>
#include <sensors.h>
#include <mqttUtils.h>
#include <pluginFactory.h>
#include <scheduler.h>
#include <SystemCommandHandler.h>
#include <ConfigurablePipelineManager.h>

#ifndef GIT_VERSION
#define GIT_VERSION "unknown"
#endif

HardwareManager* hardwareManager;
NetworkManager* networkManager;
DmxManager* dmxManager;
SystemManager* systemManager;

SettingsManager<Settings>* settingsManager;
Settings* settings;

WebAdmin* webAdmin;
SystemCommandHandler* systemCommandHandler;
ConfigurablePipelineManager* pipelineManager = nullptr;

Scheduler* scheduler = new Scheduler();

HeartbeatBroadcast* heartbeatBroadcast;

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
    Log::infoln("Firmware: %s", FIRMWARE_VERSION);
    Log::infoln("Git: %s", GIT_VERSION);

    settingsManager = new SettingsManager<Settings>("settings");
    hardwareManager = new HardwareManager();
    systemManager = new SystemManager(settingsManager);
    systemManager->initialize(FORCE_RESET, FACTORY_REST_PIN, WIFI_SSID, WIFI_PASS);
    
    // Allocate settings on heap to avoid stack lifetime issues
    settings = new Settings(settingsManager->getSettings());

    dmxManager = new DmxManager(settings, [](){
        systemManager->markCommandReceived();
    });

    try {
        hardwareManager->createThings(settings, dmxManager, scheduler, [](){
            systemManager->markCommandReceived();
        });
        Log::info("Things created.");
    } catch(const std::exception& e) {
        Log::error((std::string("ERR: creating things. ") + e.what()).c_str());
    }

    firmwareUpdateResultQueue = xQueueCreate(1, sizeof(int));

    networkManager = new NetworkManager(settings, scheduler, FIRMWARE_VERSION);
    networkManager->initializeWiFi({ STATIC_IP, GATEWAY, SUBNET, DNS }, []() {
        systemManager->saveUptimeBeforeReboot();
    });
    
    networkManager->initializeArtnet(
        [](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {
            dmxManager->onDmxFrame(data, size, metadata, remote);
        },
        WiFi.getHostname(),
        dmxManager->getListeningUniverses()
    );

    systemCommandHandler = new SystemCommandHandler(settingsManager, systemManager, dmxManager);

    if (_ENABLE_WEBSERVER) {
        Log::infoln("Starting web server ...");
        webAdmin = new WebAdmin(
            settingsManager,
            [](JsonVariant &jsonVariant) {
                return systemCommandHandler->handleCommand(jsonVariant);
            },
            FIRMWARE_VERSION + std::string(" (") + GIT_VERSION + ")",
            FACTORY_REST_PIN
        );
        webAdmin->setOnReceivedCallback([](){
            systemManager->markCommandReceived();
        });
        IPAddress ip = WiFi.localIP();
        Log::infoln("Web server listening on %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    }

    webAdmin->setPropertiesSupplier([](){
        std::map<std::string, std::string> props;
        for (auto& humTempSensor : hardwareManager->getHumTempSensors()) { // TODO is there a better way to get this data, eg. add some sensor mapping or read it from dmx if it is mapped ?
            props["temp-" + std::to_string(humTempSensor->getPin())] = std::to_string(humTempSensor->getValue().humidity);
            props["hum-" + std::to_string(humTempSensor->getPin())] = std::to_string(humTempSensor->getValue().temperature);
        }

        std::map<uint16_t /*universe*/, std::array<uint8_t, 512>> storedDmx;
        dmxManager->initializeDmxData(storedDmx);
        dmxManager->restoreDmxData(storedDmx);
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

    std::string hostName = WifiUtils::getHostname(settings->hostname.c_str());
    
    networkManager->initializeMqtt(String(hostName.c_str()), [](char* topic, byte* payload, unsigned int length) {
        Log::infoln("MQTT message received: %s, %s", topic, payload);
    });

    // Initialize pipeline manager for sensor processing with MQTT support
    pipelineManager = new ConfigurablePipelineManager(
        dmxManager->getDmxData(),
        *settings,
        networkManager->getMqtt()->getClient(),
        std::string("np/") + hostName + "/s/"
    );
    pipelineManager->initialize();
    Log::infoln("Pipeline manager initialized with %d pipelines", settings->sensorPipelines.size());

    // Initialize sensors with pipelineManager
    hardwareManager->initializeSensors(settings, pipelineManager);

    // Register tasks with scheduler
    scheduler->addTask(dmxManager);

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
        hardwareManager->turnOffAllSwitchables();
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
