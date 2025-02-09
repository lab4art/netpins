#define _TASK_OO_CALLBACKS

// #define DISABLE_LOGGING
#define _ENABLE_UDP_BROADCAST true
#define _ENABLE_WEBSERVER true

#include "config.h"

#include <NeoPixelBus.h>
#include <ArtnetWifi.h>
#include <WiFiUdp.h>

#include <map>
#include <set>
#include <list>
#include <Arduino.h>
#include <TaskScheduler.h>
#include <ArduinoLog.h>
#include <nvs.h>
#include <nvs_flash.h>
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
ArtnetWifi* artnet;
WebAdmin* webAdmin;

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
  FactoryReset::getInstance().resetCounter(true);

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

void doCommitStrip() {

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

template<typename Feature, typename Method, class ThingType>
std::vector<ThingType*> createStripThings(
    std::map<int, NeoPixelBus<Feature, Method> *>& strips,
    std::vector<StripeCfg> stripeCfgs
  ) {
  std::vector<ThingType*> sliceThings;

  // loop over strips and delete them
  for (auto& pair : strips) {
    delete pair.second;
  }
  strips.clear();

  for (auto& stripeCfg : stripeCfgs) {
    Log.noticeln("Creating led strip on pin: %d", stripeCfg.pin);
    createStrip<Feature, Method>(stripeCfg.pin, stripeCfg.size, strips);
    auto strip = strips[stripeCfg.pin];

    // create led strip things for each slice, slices are defined by first pixel only, last pixel is calculated from the next slice
    // if slices are not defined, the whole strip is used as one thing (slice)
    auto& stripSlices = stripeCfg.slices;
    if (stripSlices.size() == 0 || (stripSlices.size() == 1 && stripSlices[0] == 0)) {
      int firstPx = 0;
      int lastPx = strip->PixelCount() - 1;
      Log.noticeln("Creating led strip single slice: %d-%d", firstPx, lastPx);
      auto thing = new ThingType(strip, firstPx, lastPx);
      sliceThings.push_back(thing);
    } else {
      for (int i = 0; i < stripSlices.size(); i++) {
        int firstPx = stripSlices[i];
        int lastPx = i < stripSlices.size() - 1 ? stripSlices[i + 1] - 1 : strip->PixelCount() - 1;
        Log.noticeln("Creating led strip slice: %d-%d", firstPx, lastPx);
        auto thing = new ThingType(strip, firstPx, lastPx);
        sliceThings.push_back(thing);
      }
    }
  }
  return sliceThings;
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
  std::vector<RgbwThing*> rgbwThings = createStripThings<NeoGrbwFeature, NeoEsp32RmtNSk6812Method, RgbwThing>(rgbwStrips, settings.rgbwStrips);
  for (auto& rgbwThing : rgbwThings) {
    dmxListener->addThing(rgbwThing);
    switchables.push_back(rgbwThing);
  }

  Log.noticeln("Creating RGB strips ...");
  std::vector<RgbThing*> rgbThings = createStripThings<NeoGrbFeature, NeoEsp32RmtNWs2812xMethod, RgbThing>(rgbStrips, settings.rgbStrips);
  for (auto& rgbThing : rgbThings) {
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

  for (auto& waveDef : settings.waves) {
    std::vector<RgbThing*> waveLines;
    for (auto& sliceIndex : waveDef.sliceIndexes) {
      waveLines.push_back(rgbThings[sliceIndex]);
      dmxListener->removeThing(rgbThings[sliceIndex]);
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
void onDmxFrame(uint16_t receivedUniverse, uint16_t length, uint8_t sequence, uint8_t* data) {
  if (receivedUniverse != dmxUniverse) {
      return;
  }
  lastCommandReceivedAt = millis();
  // ignore old sequencees unless counter flipped
  if (sequence < lastDmxSequence && lastDmxSequence - sequence < 10) {
      Log.traceln("Ignoring old sequence %d, last sequence: %d", sequence, lastDmxSequence);
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
        err = nvs_flash_init();
        if (err != ESP_OK) {
            Log.errorln("Failed to initialize NVS: %s", esp_err_to_name(err));
        } else {
            Log.noticeln("NVS initialized successfully.");
        }        
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

  dmxSettingsManager = new SettingsManager<DmxSettings>("dmx");
  settingsManager = new SettingsManager<Settings>("settings");

  FactoryReset::getInstance().evaluate(FACTORY_REST_PIN);
  if (FactoryReset::getInstance().shouldReset()) {
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

  std::vector<Switchabe*> switchables;
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

  wifi = new WifiUtils(settings.wifiSsid.c_str(), settings.wifiPass.c_str(), { STATIC_IP, GATEWAY, SUBNET, DNS }, 5000, settings.hostname.c_str());
  Serial.println("Wifi MAC: " + WifiUtils::macAddress);

  artnet = new ArtnetWifi();
  artnet->begin();
  artnet->setArtDmxCallback(onDmxFrame);

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

int executionTIme = 0;
int loopCounter = 0;
unsigned long lastCommit = 0;
void loop() {
  unsigned long loopStartTime = millis();

  scheduler.execute(); // scheduler should be before commitNeoStip because tasks usually prepare the data

  FactoryReset::getInstance().resetCounter();

  if (millis() - lastCommit > 20) { // 50fps
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
    artnet->read();
  }

  if (maxIdleMillis > 0 && millis() - lastCommandReceivedAt > maxIdleMillis) {
    Log.noticeln("No command received for %d min, going to sleep ...", maxIdleMillis / 60000);
    Log.noticeln("maxIdleMillis: %d, lastCommandReceivedAt: %d, millis: %d", maxIdleMillis, lastCommandReceivedAt, millis());
    
    if (webAdmin != nullptr) {
      webAdmin->end();
    }
    WiFi.mode(WIFI_OFF);

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    esp_deep_sleep_start();
  }

  // loopCounter++;
  // executionTIme += millis() - loopStartTime;
  // if (loopCounter % 100 == 0) {
  // Log.noticeln("Loop execution time: %dms", executionTIme / 10);
  //   executionTIme = 0;
  //   loopCounter = 0;
  //   Serial.println(String("Loop execution time: ") + (millis() - loopStartTime) + "ms");
  //   Serial.println(String("Free memory: ") + ESP.getFreeHeap());
  // }
}
