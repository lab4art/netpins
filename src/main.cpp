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
#include <nvs.h>
#include <nvs_flash.h>
#include <Preferences.h>
#include "wifiUtils.cpp"
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

#define ON_WIFI_EXECUTION_CALLBACK_SIGNATURE std::function<void(String)> wifiExecutionCallback

// https://github.com/Makuna/NeoPixelBus/wiki/ESP32-NeoMethods
std::map<int /* pin */, NeoPixelBus<NeoGrbwFeature, NeoEsp32RmtNSk6812Method>*> rgbwStrips;
std::map<int /* pin */, NeoPixelBus<NeoGrbFeature, NeoEsp32RmtNWs2812xMethod>*> rgbStrips;
std::vector<PwmThing*> pwms;
std::vector<ServoThing*> servos;
std::vector<HumTempSensor*> humTempSensors;
std::vector<TouchSensor*> touchSensors;
std::map<uint8_t /* pin */, DigitalReadSensor*> digitalReadSensors;
std::map<uint8_t /* pin */, AnalogReadSensor*> analogReadSensors;

unsigned long lastCommandReceivedAt = 0;
unsigned long maxIdleMillis = 0;

WiFiUDP* udp;
WifiUtils* wifi;

SettingsManager<Settings>* settingsManager;

DmxListener* dmxListener;

ArtnetWiFiReceiver* artnet;
MqttUtils* mqtt;
SensorEvents* sensorEvents;
WebAdmin* webAdmin;

std::vector<Switchabe*> switchables;

// uptime set by system reboot like WiFi connection failure
ulong uptimeOffset = 0;

std::map<uint16_t /*universe*/, uint8_t /*lastSequence*/> lastDmxSequences;
std::map<uint16_t /*universe*/, std::array<uint8_t, 512> /*data*/> dmxData; // 1st byte in data array is a sequence number

Scheduler* scheduler = new Scheduler();

int numOfCreatedStrips = 0;
template<typename Feature, typename Method>
void createStrip(int pin, int maxNeopx, std::map<int, NeoPixelBus<Feature, Method>*>& strips) {
    Log::infoln("Initializing strip on pin %d ...", pin);
    pinMode(pin, OUTPUT);

    if (numOfCreatedStrips == 0) {
        strips[pin] = new NeoPixelBus<Feature, Method>(maxNeopx, pin, NeoBusChannel_0);
    } else if (numOfCreatedStrips == 1) {
        strips[pin] = new NeoPixelBus<Feature, Method>(maxNeopx, pin, NeoBusChannel_1);
    } else if (numOfCreatedStrips == 2) {
        strips[pin] = new NeoPixelBus<Feature, Method>(maxNeopx, pin, NeoBusChannel_2);
    } else {
        Log::errorln("Reached maximum number of strips (3).");
        return;
    }
    numOfCreatedStrips++;
    // this resets all the neopixels to an off state
    strips[pin]->Begin();
    strips[pin]->Show();
}

WebAdmin::CommandResult onSystemCommand(JsonVariant &jsonVariant) {
    lastCommandReceivedAt = millis();
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
            params,                  // Task input parameters
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
        auto updated = dmxListener->storeDmxData(dmxData);
        return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, updated ? "Saved." : "No updates.", -1};
    } else if (command == "reset-dmx") {
        dmxListener->clearDmxData();
        return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, "DMX data cleared.", -1};
    }
    return WebAdmin::CommandResult{WebAdmin::CommandStatus::ERROR, "Unknown command.", -1};
}

HeartbeatBroadcast* heartbeatBroadcast;

xSemaphoreHandle semaphore = NULL;
TaskHandle_t commitNeoStipTask;

void doCommitThings() {

    for (auto led : pwms) {
        led->commit();
    }

    for (auto pair : rgbwStrips) {
        auto strip = pair.second;
        if (strip != nullptr) {
            strip->Show();
        }
    }
    for (auto pair : rgbStrips) {
        auto strip = pair.second;
        if (strip != nullptr) {
            // Log.traceln("Before Show R:%d G:%d B:%d", strip->GetPixelColor(0).R, strip->GetPixelColor(0).G, strip->GetPixelColor(0).B);
            strip->Show();
        }
    }

    for (auto servo : servos) {
        servo->commit();
    }
}

void commitNeoStipTaskProcedure(void *arg) {
    while (true) {

        while (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != 1)
            ;
    
        doCommitThings();
        xSemaphoreGive(semaphore);
  }
}

void commitNeoStip() {
    xTaskNotifyGive(commitNeoStipTask);
    while (xSemaphoreTake(semaphore, portMAX_DELAY) != pdTRUE)
        ;
}

void initNeoStipTask() {
    commitNeoStipTask = NULL;
    semaphore = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(
        commitNeoStipTaskProcedure,  /* Task function. */
        "ThingsCommitTask",          /* name of task. */
        10000,                       /* Stack size of task */
        NULL,                        /* parameter of the task */
        configMAX_PRIORITIES-1,      /* priority of the task higer number higher priority */
        &commitNeoStipTask,          /* Task handle to keep track of created task */
        0);                          /* pin task to core core_id */
}


DigitalReadSensor* getDigitalReadSensor(int pin) {
    if (digitalReadSensors.find(pin) != digitalReadSensors.end()) { // if map is accesssed with missing key, it causes a crash at some later point. No idea why?.
        auto dReadSensor = digitalReadSensors[pin];
        Log::infoln("Found digital sensor at pin %d.", pin);
        return dReadSensor;
    } else {
        Log::traceln("Digital sensor at pin %d not found.", pin);
        return nullptr;
    }
}

template<class ThingGroupType>
class InitializedThingGroup {
    public:
        ThingGroupType* group;
        DmxCfg dmxCfg;
        
        InitializedThingGroup(ThingGroupType* group, DmxCfg dmxCfg) 
            : group(group), dmxCfg(dmxCfg) {}
};

template<typename Feature, typename Method, class ThingType, class ThingGroupType>
std::vector<InitializedThingGroup<ThingGroupType>> createStripThings(    
        std::map<int, NeoPixelBus<Feature, Method> *>& strips,
        std::vector<StripeCfg> stripeCfgs
    ) {
    std::vector<ThingType*> sliceThings;

    // loop over strips and delete them
    for (auto& pair : strips) {
        delete pair.second;
    }
    strips.clear();

    std::vector<InitializedThingGroup<ThingGroupType>> groups;
    for (auto& stripeCfg : stripeCfgs) {
        createStrip<Feature, Method>(stripeCfg.pin, stripeCfg.size, strips);
        auto strip = strips[stripeCfg.pin];

        // create led strip things for each slice, slices are defined by first pixel only, last pixel is calculated from the next slice
        // if slices are not defined, the whole strip is used as one thing (slice)
        auto& stripSlices = stripeCfg.slices;
        if (stripSlices.size() == 0) {
            // if there are no slices, create one slice from the whole strip
            stripSlices.push_back(0);
        }

        for (int i = 0; i < stripSlices.size(); i++) {
            int firstPx = stripSlices[i];
            int lastPx = i < stripSlices.size() - 1 ? stripSlices[i + 1] - 1 : strip->PixelCount() - 1;
            Log::infoln("Creating led strip slice: %d-%d, dimmer mode %s.", firstPx, lastPx, dimmerModeToString(stripeCfg.dimmer).c_str());
            auto thing = new ThingType(strip, firstPx, lastPx, stripeCfg.dimmer == DimmerMode::perSlice ? true : false, stripeCfg.name + "-" + std::to_string(i));
            sliceThings.push_back(thing);
        }
        ThingGroupType* group = new ThingGroupType(sliceThings, stripeCfg.dimmer == DimmerMode::single ? true : false, stripeCfg.name);
        groups.push_back(InitializedThingGroup<ThingGroupType>(group, stripeCfg.dmxCfg));
    }
    return groups;
};

std::vector<Switchabe*> createThings(Settings& settings) {
    std::vector<Switchabe*> switchables;

    // PWMS
    if (settings.pwms.size() > 0) {
        analogWriteResolution(14);
        PwmThing::set8bitTo14BitMapping();
        for (auto& pwmCfg : settings.pwms) {
            // initialize pwm Things
            auto pwmThing = new PwmThing(pwmCfg.pin, pwmCfg.name);
            dmxListener->addMapping(pwmThing, pwmCfg.dmxCfg);
            switchables.push_back(pwmThing);
            pwms.push_back(pwmThing);
        }
        Log::infoln("PWMs created.");
    }

    Log::infoln("Creating RGBW strips ...");
    std::vector<InitializedThingGroup<RgbwThingGroup>> rgbwThingGroups = createStripThings<NeoGrbwFeature, NeoEsp32RmtNSk6812Method, RgbwThing, RgbwThingGroup>(rgbwStrips, settings.rgbwStrips);
    for (auto& rgbwThingGroup : rgbwThingGroups) {
        dmxListener->addMapping(rgbwThingGroup.group, rgbwThingGroup.dmxCfg);
        switchables.push_back(rgbwThingGroup.group);
    }

    Log::infoln("Creating RGB strips ...");
    std::vector<InitializedThingGroup<RgbThingGroup>> rgbThingsGroups = createStripThings<NeoGrbFeature, NeoEsp32RmtNWs2812xMethod, RgbThing, RgbThingGroup>(rgbStrips, settings.rgbStrips);
    for (auto& rgbThingGroup : rgbThingsGroups) {
        dmxListener->addMapping(rgbThingGroup.group, rgbThingGroup.dmxCfg);
        switchables.push_back(rgbThingGroup.group);
    }

    Log::infoln("Creating servos ...");
    for (auto& servoCfg : settings.servos) {
        auto minPulseWidth = servoCfg.minPulseWidth == 0 ? 500 : servoCfg.minPulseWidth;
        auto maxPulseWidth = servoCfg.maxPulseWidth == 0 ? 2500 : servoCfg.maxPulseWidth;
        auto thing = new ServoThing(servoCfg.pin, servoCfg.maxAngle, minPulseWidth, maxPulseWidth);
        dmxListener->addMapping(thing, servoCfg.dmxCfg);
        servos.push_back(thing);
    }

    // Plugins
    try {
        int createdAnimations = PluginFactory::getInstance().createAnimationsFromPlugins(
            scheduler, settings.plugins, dmxListener);
        Log::infoln("Created %d animations from plugins", createdAnimations);
    } catch (const std::exception& e) {
        Log::error((std::string("ERR: configuring plugins. ") + e.what()).c_str());
    }


    // ANIMATIONS
    // Log.noticeln("Creating tail animations ...");
    // // TODO create tail animations from settings
    // if (settings.tailAnimations.size() > 0 && allRgbThings.size() > 0) {
    //     TailAnimationCfg& taCfg1 = settings.tailAnimations[0];
    //     auto rgbThing1 = allRgbThings[0];
    //     // dmxListener->removeMapping(rgbThing1);
    //     dmxListener->removeMapping(rgbThingsGroupsIndex[0]);
    //     tailAnimation1 = new TailAnimation(
    //         &scheduler, 
    //         rgbThing1, 
    //         taCfg1.direction,
    //         true);
    //     tailAnimation1->setColor1(taCfg1.color1);
    //     tailAnimation1->setColor2(taCfg1.color2);
    //     tailAnimation1->setDimm(taCfg1.dimm);
    //     tailAnimation1->setTailLength(taCfg1.tailLength);
    //     tailAnimation1->setHeadLength(taCfg1.headLength);
    //     tailAnimation1->setDuration(taCfg1.duration);
    //     tailAnimation1Duration = taCfg1.duration;
    //     Log.noticeln("Tail animation 1 created with color1: %s, color2: %s, dimm: %d, tail length: %d, head length: %d, duration: %d ms.",
    //         toHexColor(taCfg1.color1).c_str(),
    //         toHexColor(taCfg1.color2).c_str(),
    //         taCfg1.dimm,
    //         taCfg1.tailLength,
    //         taCfg1.headLength,
    //         taCfg1.duration);
    //     // tailAnimation1->setRepeat(false);

    //     if (allRgbThings.size() > 1) {
    //         rgbSlice2 = allRgbThings[1];
    //         dmxListener->removeMapping(rgbThingsGroupsIndex[1]);
    //         animationColors = taCfg1.colors;
    //         if (animationColors.size() < 1) {
    //             animationColors.push_back(taCfg1.color1);
    //             animationColors.push_back(taCfg1.color2);
    //         }
    //     }
    // } else {
    //     Log.warningln("No tail animations or RGB things available, skipping tail animation setup.");
    // }


    return switchables;
};

void onDmxFrame(const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {
    // process only if we listen to this universe
    if (dmxListener->isListeningToUniverse(metadata.universe) == false) {
        return;
    }
    lastCommandReceivedAt = millis();
    
    // ignore old sequences unless counter flipped (per-universe tracking)
    uint8_t lastSequence = lastDmxSequences[metadata.universe];
    if (metadata.sequence < lastSequence && lastSequence - metadata.sequence < 10) {
        Log::traceln("Ignoring old sequence %d for universe %d, last sequence: %d", metadata.sequence, metadata.universe, lastSequence);
        return;
    }
    lastDmxSequences[metadata.universe] = metadata.sequence;

    memcpy(dmxData[metadata.universe].data(), data, std::min(size, (uint16_t)512));
    
    // do not process the data here, leave IO callback as soon as possible
};

void eraseAllPreferences() {
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
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    Log::infoln("MQTT message received: %s, %s", topic, payload);
};

void beforeWiFiReboot() {
    // store the uptime to preferences, used for idle power off
    Preferences preferences;
    preferences.begin("sys", false);
    preferences.putULong("reboot-wifi", millis() + uptimeOffset);
    preferences.end();
};

AnalogReadSensor* getAnalogReadSensor(int pin) {
    if (analogReadSensors.find(pin) != analogReadSensors.end()) { // if map is accesssed with missing key, it causes a crash at some later point. No idea why?.
        auto aReadSensor = analogReadSensors[pin];
        Log::infoln("Found analog sensor at pin %d.", pin);
        return aReadSensor;
    } else {
        Log::traceln("Analog sensor at pin %d not found.", pin);
        return nullptr;
    }
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

    Preferences preferences;
    preferences.begin("sys", false);
    uptimeOffset = preferences.getULong("reboot-wifi", 0L);
    if (uptimeOffset != 0) {
        // erase to not count the same offset on reboots
        preferences.putULong("reboot-wifi", 0L);
    }
    preferences.end();

    settingsManager = new SettingsManager<Settings>("settings");

    FactoryReset::getInstance().evaluate(FACTORY_REST_PIN);
    if (FORCE_RESET || FactoryReset::getInstance().shouldReset()) {
        Log::infoln("Factory reset requested, setting defaults ...");
        eraseAllPreferences();
        settingsManager->setDefaults();
        // Set WiFi credentials from config
        settingsManager->getSettings().wifiSsid = WIFI_SSID;
        settingsManager->getSettings().wifiPass = WIFI_PASS;
        settingsManager->save();
    } else {
        // load settings
        settingsManager->load();
    }

    if (settingsManager->getSettings().wifiSsid.empty() || settingsManager->getSettings().wifiSsid == "null") {
        Log::infoln("Empty settings, setting defaults ...");
        settingsManager->setDefaults();
        // Set WiFi credentials from config
        settingsManager->getSettings().wifiSsid = WIFI_SSID;
        settingsManager->getSettings().wifiPass = WIFI_PASS;
        settingsManager->save();
    }
    Settings settings = settingsManager->getSettings();
    Log::info((std::string("Loaded settings: ") + settings.asJson()).c_str());
    
    // Apply log level from settings
    Log::setLogLevel(settings.logLevel);
    Log::infoln("Log level set to %d", settings.logLevel);
    
    dmxListener = new DmxListener(settings.dmxChOffset);

    try {
        switchables = createThings(settings);
        Log::info("Things created.");
    } catch(const std::exception& e) {
        Log::error((std::string("ERR: creating things. ") + e.what()).c_str());
    }

    // SENSORS ////
    Log::infoln("Creating Hum/Temp sensor ...");
    for (auto& humTempCfg : settings.humTemps) {
        auto humTempSensor = new HumTempSensor(humTempCfg.pin, humTempCfg.readMs);
        humTempSensors.push_back(humTempSensor);
    }

    Log::infoln("Creating touch sensors ...");
    for (auto& touch : settings.touchSensors) {
        auto touchSensor = new TouchSensor(touch.pin, 200, touch.threshold);
        std::string sensorName = touch.sensorName;
        touchSensor->addOnChangeListener([sensorName](boolean touched) {
            sensorEvents->publish(sensorName, touched ? 1 : 0, false); // TODO reference by name not pin
        });
        touchSensors.push_back(touchSensor);
    }

    Log::infoln("Creating digital read sensors ...");
    for (auto& dreadCfg : settings.digitalReadSensors) {
        auto digitalReadSensor = new DigitalReadSensor(dreadCfg.pin, dreadCfg.readMs, INPUT_PULLUP);
        digitalReadSensor->addOnChangeListener([dreadCfg](bool value) {
            sensorEvents->publish(dreadCfg.sensorName, value ? 1 : 0, true); // TODO read from cfg local/remote
        });

        digitalReadSensor->addOnChangeListener([](bool value) {
            Log::infoln("Digital read sensor value changed to: %d", value);
            if (value) {
                // isMovementDetected = true; TODO
            } else {
                // isMovementDetected = false;
            }
        });
        Log::infoln("Digital read sensor %d created.", dreadCfg.pin);
        digitalReadSensors[dreadCfg.pin] = digitalReadSensor;
    }

    Log::infoln("Creating analog read sensors ...");
    for (auto& areadCfg : settings.analogReadSensors) {
        auto analogReadSensor = new AnalogReadSensor(areadCfg.pin, areadCfg.readMs);
        // TODO add optional filters to the listener: trashold, move average, etc.
        analogReadSensor->addOnChangeListener([areadCfg](uint16_t value) {
            // Log.traceln("Analog read %d value changed to: %d", areadCfg.pin, value);
            sensorEvents->publish(areadCfg.sensorName, value, true);
        });
        Log::infoln("Analog read sensor created. Pin: %d, readMs: %d", areadCfg.pin, areadCfg.readMs);
        analogReadSensors[areadCfg.pin] = analogReadSensor;
    }

    Log::info("Mounting LittleFS ...");
    if (!LittleFS.begin()) {
        Log::error("An Error has occurred while mounting LittleFS.");
    }

    if (settings.lightsTest) {
        Log::info("Starting lights test ...");
        for (auto& switchable : switchables) {
            switchable->on();
        }
        doCommitThings();
        Log::info("Waiting 2s ...");
        delay(2000);
        for (auto& switchable : switchables) {
            switchable->off();
        }
        doCommitThings();
        Log::info("Lights test done.");
    }

    initNeoStipTask();
    firmwareUpdateResultQueue = xQueueCreate(1, sizeof(int));

    dmxListener->initializeDmxData(dmxData);
    dmxListener->restoreDmxData(dmxData);

    if (settings.maxIdle > 0) {
        maxIdleMillis = settings.maxIdle * 60000;
    }

    Log::info((std::string("Using ssid: ") + settings.wifiSsid).c_str());

    wifi = new WifiUtils(
        settings.wifiSsid.c_str(), 
        settings.wifiPass.c_str(), 
        { STATIC_IP, GATEWAY, SUBNET, DNS }, 
        5000,
        settings.rebootAfterWifiFailed,
        beforeWiFiReboot,
        settings.hostname.c_str());
    Log::info((std::string("Wifi MAC: ") + WifiUtils::macAddress.c_str()).c_str());

    if (!settings.disableArtnet) {
        artnet = new ArtnetWiFiReceiver();
        artnet->begin();
        artnet->subscribeArtDmx(onDmxFrame);
        artnet->setArtPollReplyConfigShortName("NetPins");
        auto universes = dmxListener->getListeningUniverses();
        // convert to array of uint8_t to string
        String universeStr = "";
        bool first = true;
        for (auto universe : universes) {
            if (!first) {
                universeStr += ",";
            }
            universeStr += String(universe);
            first = false;
        }

        artnet->setArtPollReplyConfigLongName(String(WiFi.getHostname()) + " - " + settings.dmxChOffset + "@" + universeStr + " - " + FIRMWARE_VERSION);
    } else {
        Log::infoln("Artnet is disabled.");
    }

    if (_ENABLE_WEBSERVER) {
        Log::infoln("Starting web server ...");
        webAdmin = new WebAdmin(
            settingsManager,
            onSystemCommand
        );
        webAdmin->setOnReceivedCallback([](){
            lastCommandReceivedAt = millis();
        });
        Log::infoln("Web server listening on %s", WiFi.localIP().toString().c_str());
    }

    webAdmin->setPropertiesSupplier([](){
        std::map<std::string, std::string> props;
        for (auto& humTempSensor : humTempSensors) {
            props["temp-" + std::to_string(humTempSensor->getPin())] = std::to_string(humTempSensor->getValue().humidity);
            props["hum-" + std::to_string(humTempSensor->getPin())] = std::to_string(humTempSensor->getValue().temperature);
        }

        std::map<uint16_t /*universe*/, std::array<uint8_t, 512>> storedDmx;
        dmxListener->initializeDmxData(storedDmx);
        dmxListener->restoreDmxData(storedDmx);
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

    String hostName = WifiUtils::getHostname(settings.hostname.c_str());
    mqtt = new MqttUtils(
        settings.mqtt.server.c_str(),
        settings.mqtt.port,
        settings.mqtt.user.c_str(),
        settings.mqtt.password.c_str(),
        (String("np/") + hostName + "/c/#").c_str(),
        hostName,
        onMqttMessage
    );
    sensorEvents = new SensorEvents(
        mqtt,
        std::string("np/") + std::string(hostName.c_str()) + "/s/",
        settings.sensorMappings,
        dmxData
    );

    Log::infoln("Running ...");
}

void onWifiExecutionCallback(String ip) {
    Log::infoln("WiFi connected, IP address: %s.", ip.c_str());
    if (settingsManager->getSettings().disableWifiPowerSave) {
        esp_wifi_set_ps(WIFI_PS_NONE); // Disable power-saving mode
    }
    
    auto settigns = settingsManager->getSettings();
    if (_ENABLE_UDP_BROADCAST) {
        if (settigns.udpPort > 0 && settigns.hbInt > 0) {
            if (heartbeatBroadcast == nullptr && settigns.hbInt > 0) {
                auto ip = WiFi.localIP();
                IPAddress broadcastIp = IPAddress(ip[0], ip[1], ip[2], 255);
                Log::infoln("Starting UDP heartbeat on %d.%d.%d.255 ...", ip[0], ip[1], ip[2]);
                udp = new WiFiUDP();
                heartbeatBroadcast = new HeartbeatBroadcast(
                udp, 
                broadcastIp, 
                settigns.udpPort, 
                FIRMWARE_VERSION, 
                settigns.hostname,
                settigns.hbInt);
                scheduler->addTask(heartbeatBroadcast);
            }
        }
    }
}

int loopCounter = 0;
int executionTimeSum = 0;
int maxExecutionTime = 0;

uint32_t minFreeHeap = UINT32_MAX;
uint32_t minFreePsram = UINT32_MAX;

unsigned long lastDmxCommit = 0;
void loop() {
    unsigned long loopStartTime = micros();

    FactoryReset::getInstance().resetCounter();

    scheduler->loop();

    /*
    30ms = 30fps
    20ms = 50fps
    13ms = 75fps
    */
    if (millis() - lastDmxCommit > 20) {
        for (auto& universeData : dmxData) {
            dmxListener->processDmxData(universeData.first, universeData.second);
        }
        commitNeoStip();
        lastDmxCommit = millis();
    }

    if (wifi != nullptr) {
        wifi->tryReconnect(onWifiExecutionCallback);
    }
    
    if (artnet != nullptr) {
        artnet->parse();
    }

    for (auto& humTempSensor : humTempSensors) {
        humTempSensor->read();
    }

    for (TouchSensor* touchSensor : touchSensors) {
        touchSensor->read();
    }

    for (auto& pair : digitalReadSensors) {
        auto& sensor = pair.second;
        sensor->read();
    }

    for (auto& pair : analogReadSensors) {
        auto& sensor = pair.second;
        sensor->read();
    }

    mqtt->tryReconnect();
    mqtt->loop();

    // idle power off
    auto virtualUptime = uptimeOffset + millis();
    if (maxIdleMillis > 0 && virtualUptime - lastCommandReceivedAt > maxIdleMillis) {
        Log::infoln("No command received for %d min, going to sleep ...", maxIdleMillis / 60000);
        Log::infoln("maxIdleMillis: %d, lastCommandReceivedAt: %d, virtualUptime %d, millis: %d", maxIdleMillis, lastCommandReceivedAt, virtualUptime, millis());
        
        if (webAdmin != nullptr) {
            webAdmin->end();
        }
        WiFi.mode(WIFI_OFF);

        // turn off all switchables
        for (auto& switchable : switchables) {
            switchable->off();
        }
        doCommitThings();

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
