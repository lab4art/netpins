#define _TASK_OO_CALLBACKS

// #define DISABLE_LOGGING
#define _ENABLE_UDP_BROADCAST true
#define _ENABLE_WEBSERVER true

#include "config.h"

#include <NeoPixelBus.h>
#include <ArtnetWiFi.h>
#include <WiFiUdp.h>

#include <map>
#include <set>
#include <list>
#include <Arduino.h>
#include <TaskScheduler.h>
#include <ArduinoLog.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <Preferences.h>
#include "wifiUtils.cpp"
#include "firmware.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <uri/UriBraces.h>
#include "heartbeatBroadcast.h"
#include <logUtils.h>
#include <GeneralUtils.h>
#include <LittleFS.h>
#include <DmxListener.h>
#include <animations.h>
#include <webadmin.h>
#include <settings.h>
#include <factoryReset.h>
#include <Things.h>


#define ON_WIFI_EXECUTION_CALLBACK_SIGNATURE std::function<void(String)> wifiExecutionCallback

// https://github.com/Makuna/NeoPixelBus/wiki/ESP32-NeoMethods
std::map<int /* pin */, NeoPixelBus<NeoGrbwFeature, NeoEsp32RmtNSk6812Method>*> rgbwStrips;
std::map<int /* pin */, NeoPixelBus<NeoGrbFeature, NeoEsp32RmtNWs2812xMethod>*> rgbStrips;
std::vector<LedThing*> leds;
std::vector<ServoThing*> servos;

unsigned long lastCommandReceivedAt = 0;
unsigned long maxIdleMillis = 0;

WiFiUDP* udp;
WifiUtils* wifi;

SettingsManager<DmxSettings>* dmxSettingsManager;
SettingsManager<Settings>* settingsManager;

DmxListener* dmxListener;


Scheduler scheduler;
ArtnetWiFiReceiver* artnet;
WebAdmin* webAdmin;

FactoryReset* factoryReset;

std::vector<Switchabe*> switchables;

// uptime set by system reboot like WiFi connection failure
ulong uptimeOffset = 0;

int numOfCreatedStrips = 0;
template<typename Feature, typename Method>
void createStrip(int pin, int maxNeopx, std::map<int, NeoPixelBus<Feature, Method>*>& strips) {
    Log.infoln("Initializing strip on pin %d ...", pin);
    pinMode(pin, OUTPUT);

    if (numOfCreatedStrips == 0) {
        strips[pin] = new NeoPixelBus<Feature, Method>(maxNeopx, pin, NeoBusChannel_0);
    } else if (numOfCreatedStrips == 1) {
        strips[pin] = new NeoPixelBus<Feature, Method>(maxNeopx, pin, NeoBusChannel_1);
    } else if (numOfCreatedStrips == 2) {
        strips[pin] = new NeoPixelBus<Feature, Method>(maxNeopx, pin, NeoBusChannel_2);
    } else {
        Log.errorln("Reached maximum number of strips (3).");
        return;
    }
    numOfCreatedStrips++;
    // this resets all the neopixels to an off state
    strips[pin]->Begin();
    strips[pin]->Show();
}

WebAdmin::CommandResult onSystemCommand(JsonDocument &jsonDoc) {
  lastCommandReceivedAt = millis();
  factoryReset->resetCounter(true);

  if (jsonDoc["command"] == "sys-config") {
    settingsManager->fromJson(jsonDoc["data"].as<String>());
    if (settingsManager->isDirty()) {
      settingsManager->save();
      return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK_REBOOT, "Saved, rebooting ...", 3000};
    } else {
      return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, "No updates.", -1};
    }
  } else if (jsonDoc["command"] == "sys-config-merge") {
    settingsManager->mergeJson(jsonDoc["data"].as<String>());
    if (settingsManager->isDirty()) {
      settingsManager->save();
      return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK_REBOOT, "Saved, rebooting ...", 3000};
    } else {
      return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, "No updates.", -1};
    }
  } else if (jsonDoc["command"] == "dmx-config") {
    Log.noticeln("DMX config command received.");
    dmxSettingsManager->fromJson(jsonDoc["data"].as<String>());
    if (dmxSettingsManager->isDirty()) {
      dmxSettingsManager->save();
      return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK_REBOOT, "Saved, rebooting ...", 3000};
    } else {
      return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, "No updates.", -1};
    }
  } else if (jsonDoc["command"] == "firmware-update" || jsonDoc["command"] == "spiffs-update") {
    FirmwareUpdateParams* params;

    if (jsonDoc["command"] == "firmware-update") {
      params = new FirmwareUpdateParams{jsonDoc["data"]["url"].as<String>(), false};
    } else {
      params = new FirmwareUpdateParams{jsonDoc["data"]["url"].as<String>(), true};
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
      Log.errorln("Failed to receive update result within 90 seconds.");
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
  } else if (jsonDoc["command"] == "reboot") {
    return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK_REBOOT, "Rebooting ...", 3000};
  }
  return WebAdmin::CommandResult{WebAdmin::CommandStatus::ERROR, "Unknown command.", -1};
}

HeartbeatBroadcast* heartbeatBroadcast;

xSemaphoreHandle semaphore = NULL;
TaskHandle_t commitNeoStipTask;

void doCommitStrip() { // TODO rename me

  for (auto led : leds) {
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
    
    doCommitStrip();
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
      commitNeoStipTaskProcedure,         /* Task function. */
      "NeoFxRunnerTask",            /* name of task. */
      10000,                       /* Stack size of task */
      NULL,                        /* parameter of the task */
      configMAX_PRIORITIES-1,      /* priority of the task higer number higher priority */
      &commitNeoStipTask,          /* Task handle to keep track of created task */
      0);                          /* pin task to core core_id */
}

TailAnimation* tailAnimation1; // TODO make this configurable or pluggable
TailAnimation* tailAnimation2;

template<typename Feature, typename Method, class ThingType, class ThingGroupType>
std::vector<ThingGroupType*> createStripThings(
    std::map<int, NeoPixelBus<Feature, Method> *>& strips,
    std::vector<StripeCfg> stripeCfgs
  ) {
  std::vector<ThingType*> sliceThings;

  // loop over strips and delete them
  for (auto& pair : strips) {
    delete pair.second;
  }
  strips.clear();

  std::vector<ThingGroupType*> groups;
  for (auto& stripeCfg : stripeCfgs) {
    Log.noticeln("Creating led strip on pin: %d", stripeCfg.pin);
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
      Log.noticeln("Creating led strip slice: %d-%d", firstPx, lastPx);
      auto thing = new ThingType(strip, firstPx, lastPx, stripeCfg.dimmer == DimmerMode::perStripe ? true : false);
      sliceThings.push_back(thing);
    }
    ThingGroupType* group = new ThingGroupType(sliceThings, stripeCfg.dimmer == DimmerMode::single ? true : false);
    groups.push_back(group);
  }
  return groups;
};

std::vector<Switchabe*> createThings(Settings& settings) {
  std::vector<Switchabe*> switchables;

  // LEDS
  if (settings.leds.size() > 0) {
    analogWriteResolution(14);
    LedThing::set8bitTo14BitMapping();
  for (auto& ledPin : settings.leds) {
    // initialize led Things
    auto ledThing = new LedThing(ledPin);
    dmxListener->addThing(ledThing);
    switchables.push_back(ledThing);
    leds.push_back(ledThing);
  }
  Log.noticeln("LEDs created.");
  }

  Log.noticeln("Creating RGBW strips ...");
  std::vector<RgbwThingGroup*> rgbwThings = createStripThings<NeoGrbwFeature, NeoEsp32RmtNSk6812Method, RgbwThing, RgbwThingGroup>(rgbwStrips, settings.rgbwStrips);
  for (auto& rgbwThing : rgbwThings) {
    dmxListener->addThing(rgbwThing);
    switchables.push_back(rgbwThing);
  }

  Log.noticeln("Creating RGB strips ...");
  std::vector<RgbThingGroup*> rgbThingsGroups = createStripThings<NeoGrbFeature, NeoEsp32RmtNWs2812xMethod, RgbThing, RgbThingGroup>(rgbStrips, settings.rgbStrips);
  for (auto& rgbThing : rgbThingsGroups) {
    dmxListener->addThing(rgbThing);
    switchables.push_back(rgbThing);
  }

  Log.noticeln("Creating servos ...");
  for (auto& servoCfg : settings.servos) {
    auto minPulseWidth = servoCfg.minPulseWidth == 0 ? 500 : servoCfg.minPulseWidth;
    auto maxPulseWidth = servoCfg.maxPulseWidth == 0 ? 2500 : servoCfg.maxPulseWidth;
    auto thing = new ServoThing(servoCfg.pin, servoCfg.maxAngle, minPulseWidth, maxPulseWidth);
    dmxListener->addThing(thing);
    servos.push_back(thing);
  }

  // ANIMATIONS
  // auto rgbThing = rgbThings[0]; //TODO make this configurable
  // tailAnimation1 = new TailAnimation(
  //   &scheduler, 
  //   rgbThing, 
  //   TailAnimation::Direction::RIGHT,
  //   false,
  //   "Ani-1");
  // tailAnimation1->setColor1(RgbColor(100, 0, 0));
  // tailAnimation1->setColor2(RgbColor(0, 0, 70));
  // tailAnimation1->setTailLength(10);
  // tailAnimation1->setDuration(4000);

  // tailAnimation2 = new TailAnimation(
  //   &scheduler, 
  //   rgbThing, 
  //   TailAnimation::Direction::RIGHT,
  //   false, 
  //   "Ani-2");
  // tailAnimation2->setColor1(RgbColor(0, 0, 70));
  // tailAnimation2->setColor2(RgbColor(100, 0, 0));
  // tailAnimation2->setTailLength(10);
  // tailAnimation2->setDuration(4000);
  
  // Log.noticeln("Led strips created.");
  // tailAnimation1->setOnHeadReachedEnd([](){
  //     tailAnimation2->restart();
  // });
  // tailAnimation2->setOnHeadReachedEnd([](){
  //   tailAnimation1->restart();
  // });
  // tailAnimation1->restart();

  //wave1 = new Wave(&scheduler, rgbThings, 4000);
  // loop over settings waves and create animations

  std::vector<RgbThing*> allRgbThings;
  std::map<uint8_t, RgbThing*> rgbThingsGroupsIndex;
  for (auto& rgbThingsGroup : rgbThingsGroups) {
    for (auto& rgbThing : rgbThingsGroup->things()) {
      allRgbThings.push_back(rgbThing);
      rgbThingsGroupsIndex[allRgbThings.size() - 1] = rgbThing;
    }
  }

  for (auto& waveDef : settings.waves) {
    std::vector<RgbThing*> waveLines;
    for (auto& sliceIndex : waveDef.sliceIndexes) {
      waveLines.push_back(allRgbThings[sliceIndex]);
      // remove the group if at least one of the lines is in the wave
      dmxListener->removeThing(rgbThingsGroupsIndex[sliceIndex]);
    }
    auto wave = new Wave(&scheduler, waveLines, waveDef.maxFadeTime);
    Serial.println(String("Wave created with ") + waveLines.size() + " lines.");
    dmxListener->addThing(wave);
  }

  return switchables;
};

uint16_t dmxUniverse;
uint8_t lastDmxSequence = 0;
uint8_t dmxData[512] = {0}; // 1st byte is sequence number

void onDmxFrame(const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {
  if (metadata.universe != dmxUniverse) {
      return;
  }
  lastCommandReceivedAt = millis();
  // ignore old sequencees unless counter flipped
  if (metadata.sequence < lastDmxSequence && lastDmxSequence - metadata.sequence < 10) {
      Log.traceln("Ignoring old sequence %d, last sequence: %d", metadata.sequence, lastDmxSequence);
      return;
  }

  for (int i = 0; i < 512; i++) {
    dmxData[i] = data[i];
  }
  // do not process the data here, leave IO callback as soon as possible
};

void eraseAllPreferences() {
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        Log.errorln("Failed to erase NVS: %s\n", esp_err_to_name(err));
    } else {
        Log.noticeln("NVS erased successfully.");
    }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  // wait for serial monitor
  while (WAIT_FOR_SERIAL && !Serial) {
    delay(100);
  }

  Serial.println("Booting ...");

  Log.begin(LOG_LEVEL_NOTICE, &Serial, true);
  // Log.begin(LOG_LEVEL_TRACE, &Serial, true);
  Log.setShowLevel(false);    // Do not show loglevel, we will do this in the prefix
  Log.setPrefix(printPrefix);
  Log.infoln("Logging initialized.");

  Preferences preferences;
  preferences.begin("sys", false);
  uptimeOffset = preferences.getULong("reboot-wifi", 0L);
  if (uptimeOffset != 0) {
    // erase to not count the same offset on regular reboots
    preferences.putULong("reboot-wifi", 0L);
  }
  preferences.end();

  dmxSettingsManager = new SettingsManager<DmxSettings>("dmx");
  settingsManager = new SettingsManager<Settings>("settings");

  factoryReset = new FactoryReset();
  if (factoryReset->shouldReset()) {
    Log.noticeln("Factory reset requested, setting defaults ...");
    eraseAllPreferences();
    settingsManager->setDefaults();
    settingsManager->save();
    dmxSettingsManager->setDefaults();
    dmxSettingsManager->save();
  } else {
    // load settings
    settingsManager->load();
    dmxSettingsManager->load();
  }

  // TODO validate input configs
  if (settingsManager->getSettings().wifiSsid.empty() || settingsManager->getSettings().wifiSsid == "null") { // TODO make sure string literal "null" is not stored
    Log.noticeln("Empty settings, setting defaults ...");
    settingsManager->setDefaults();
    settingsManager->save();
    dmxSettingsManager->setDefaults();
    dmxSettingsManager->save();
  }
  Settings settings = settingsManager->getSettings();
  Serial.println(String("Loaded settings: ") + settings.asJson().c_str());
  
  auto dmxSettings = dmxSettingsManager->getSettings();
  dmxUniverse = dmxSettings.universe;
  dmxListener = new DmxListener(dmxSettings.channel, settings.enableDmxStore);

  try {
    switchables = createThings(settings);
    Serial.println("Things created.");
  } catch(const std::exception& e) {
    Serial.println(String("ERR: creating things. ") + e.what());
  }

  Serial.println("Mounting LittleFS ...");
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
    // return; //TODO add error message to sys-info
  }

  if (settings.lightsTest) {
    Serial.println("Starting lights test ...");
    for (auto& switchable : switchables) {
      switchable->on();
    }
    doCommitStrip();
    Serial.println("Waiting 2s ...");
    delay(2000);
    for (auto& switchable : switchables) {
      switchable->off();
    }
    doCommitStrip();
    Serial.println("Lights test done.");
  }

  initNeoStipTask();
  firmwareUpdateResultQueue = xQueueCreate(1, sizeof(int));

  dmxListener->restoreDmxData(dmxData);

  if (settings.maxIdle > 0) {
    maxIdleMillis = settings.maxIdle * 60000;
  }

  Serial.println(String("Using ssid: ") + settings.wifiSsid.c_str());

  wifi = new WifiUtils(
    settings.wifiSsid.c_str(), 
    settings.wifiPass.c_str(), 
    { STATIC_IP, GATEWAY, SUBNET, DNS }, 
    5000,
    settings.rebootAfterWifiFailed,
    uptimeOffset,
    settings.hostname.c_str());
  Serial.println("Wifi MAC: " + WifiUtils::macAddress);

  artnet = new ArtnetWiFiReceiver();
  artnet->begin();
  artnet->subscribeArtDmx(onDmxFrame);
  artnet->setArtPollReplyConfigShortName("NetPins");
  artnet->setArtPollReplyConfigLongName(String(WiFi.getHostname()) + " - " + dmxSettings.channel + "@" + dmxSettings.universe + " - " + FIRMWARE_VERSION);

  if (_ENABLE_WEBSERVER) {
    Log.noticeln("Starting web server ...");
    webAdmin = new WebAdmin(
      settingsManager,
      dmxSettingsManager,
      onSystemCommand
    );
    webAdmin->setOnReceivedCallback([](){
      lastCommandReceivedAt = millis();
    });
    Log.noticeln("Web server listening on %s", WiFi.localIP().toString().c_str());
  }

  Log.noticeln("Running ...");
}

void onWifiExecutionCallback(String ip) {
  Log.noticeln("WiFi connected, IP address: %s.", ip.c_str());
  if (settingsManager->getSettings().disableWifiPowerSave) {
    esp_wifi_set_ps(WIFI_PS_NONE); // Disable power-saving mode
  }

  auto settigns = settingsManager->getSettings();
  if (_ENABLE_UDP_BROADCAST) {
    if (settigns.udpPort > 0) {
      if (heartbeatBroadcast != nullptr) {
        Log.noticeln("Stopping UDP heartbeat ...");
        delete heartbeatBroadcast;
        heartbeatBroadcast = nullptr;
      }

      if (settigns.hbInt > 0) {
        auto ip = WiFi.localIP();
        IPAddress broadcastIp = IPAddress(ip[0], ip[1], ip[2], 255);
        Log.noticeln("Starting UDP heartbeat on %d.%d.%d.255 ...", ip[0], ip[1], ip[2]);
        udp = new WiFiUDP();
        heartbeatBroadcast = new HeartbeatBroadcast(
          udp, 
          broadcastIp, 
          settigns.udpPort, 
          &scheduler, 
          FIRMWARE_VERSION, 
          settigns.hostname,
          settigns.hbInt);
      }
    }
  }
}

int loopCounter = 0;
int executionTimeSum = 0;
int maxExecutionTime = 0;
unsigned long lastCommit = 0;
void loop() {
  unsigned long loopStartTime = micros();

  scheduler.execute(); // scheduler should be before commitNeoStip because tasks usually prepare the data

  if (factoryReset != nullptr) {
    if (factoryReset->resetCounter()) {
      // delete factoryReset; // if deleting, ESP can crash
    }
  }

  /*
  30ms = 30fps
  20ms = 50fps
  13ms = 75fps
  */
  if (millis() - lastCommit > 20) {
    // unsigned long startProcessing = micros();
    dmxListener->processDmxData(512, dmxData);
    // doCommitStrip();
    commitNeoStip();
    // Serial.println(String(millis()) + " Processed in [us]: " + (micros() - startProcessing));
    lastCommit = millis();
  }

  if (wifi != nullptr) {
    wifi->tryReconnect(onWifiExecutionCallback);
  }
  
  if (artnet != nullptr) {
    artnet->parse();
  }

  auto virtualUptime = uptimeOffset + millis();
  if (maxIdleMillis > 0 && virtualUptime - lastCommandReceivedAt > maxIdleMillis) {
    Log.noticeln("No command received for %d min, going to sleep ...", maxIdleMillis / 60000);
    Log.noticeln("maxIdleMillis: %d, lastCommandReceivedAt: %d, virtualUptime %d, millis: %d", maxIdleMillis, lastCommandReceivedAt, virtualUptime, millis());
    
    if (webAdmin != nullptr) {
      webAdmin->end();
    }
    WiFi.mode(WIFI_OFF);

    // turn off all switchables
    for (auto& switchable : switchables) {
      switchable->off();
    }
    doCommitStrip();

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_deep_sleep_start();
  }

  loopCounter++;
  auto executionTime = micros() - loopStartTime;
  executionTimeSum += executionTime;
  if (executionTime > maxExecutionTime) {
    maxExecutionTime = executionTime;
  }
  if (loopCounter % 1000 == 0) {
    Log.noticeln("Max loop execution time: %d us, avg loop execution time: %d us", maxExecutionTime, executionTimeSum / loopCounter);
    executionTimeSum = 0;
    loopCounter = 0;
    Serial.println(String("Free memory: ") + ESP.getFreeHeap());
  }
}
