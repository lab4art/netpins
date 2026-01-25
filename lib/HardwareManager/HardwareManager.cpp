#include "HardwareManager.h"
#include "LedCommitTask.h"
#include <Log.h>
#include <pluginFactory.h>
#include <Arduino.h>
#include <ConfigurablePipelineManager.h>

HardwareManager::HardwareManager() 
    : semaphore(NULL), commitNeoStipTask(NULL), numOfCreatedStrips(0), dmxOutput(nullptr), dmxInput(nullptr) {
    initNeoStipTask();
    ledCommitTask = new LedCommitTask(this, 20);  // 20ms = 50Hz
}

HardwareManager::~HardwareManager() {
    // Clean up strips
    for (auto& pair : rgbwStrips) {
        delete pair.second;
    }
    for (auto& pair : rgbStrips) {
        delete pair.second;
    }
    
    // Clean up PWMs
    for (auto pwm : pwms) {
        delete pwm;
    }
    
    // Clean up servos
    for (auto servo : servos) {
        delete servo;
    }
    
    // Clean up sensors
    for (auto sensor : humTempSensors) {
        delete sensor;
    }
    for (auto sensor : touchSensors) {
        delete sensor;
    }
    for (auto& pair : digitalReadSensors) {
        delete pair.second;
    }
    
    // Clean up LED commit task
    delete ledCommitTask;
    for (auto& pair : analogReadSensors) {
        delete pair.second;
    }
    
    // Clean up DMX output
    if (dmxOutput != nullptr) {
        delete dmxOutput;
    }
    
    // Clean up DMX input
    if (dmxInput != nullptr) {
        delete dmxInput;
    }
    
    // Clean up semaphore
    if (semaphore != NULL) {
        vSemaphoreDelete(semaphore);
    }
    
    // Note: FreeRTOS tasks are not deleted here to avoid issues during shutdown
}

template<typename Feature, typename Method>
void HardwareManager::createStrip(int pin, int maxNeopx, std::map<int, NeoPixelBus<Feature, Method>*>& strips) {
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

void HardwareManager::doCommitThings() {
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
            strip->Show();
        }
    }

    for (auto servo : servos) {
        servo->commit();
    }
}

void HardwareManager::commitNeoStipTaskProcedure(void *arg) {
    HardwareManager* manager = static_cast<HardwareManager*>(arg);
    while (true) {
        while (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != 1)
            ;
    
        manager->doCommitThings();
        xSemaphoreGive(manager->semaphore);
    }
}

void HardwareManager::commitNeoStip() {
    xTaskNotifyGive(commitNeoStipTask);
    while (xSemaphoreTake(semaphore, portMAX_DELAY) != pdTRUE)
        ;
}

void HardwareManager::initNeoStipTask() {
    commitNeoStipTask = NULL;
    semaphore = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(
        commitNeoStipTaskProcedure,  /* Task function. */
        "ThingsCommitTask",          /* name of task. */
        10000,                       /* Stack size of task */
        this,                        /* parameter of the task */
        configMAX_PRIORITIES-1,      /* priority of the task higer number higher priority */
        &commitNeoStipTask,          /* Task handle to keep track of created task */
        0);                          /* pin task to core core_id */
}

template<typename Feature, typename Method, class ThingType, class ThingGroupType>
std::vector<InitializedThingGroup<ThingGroupType>> HardwareManager::createStripThings(    
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
}

void HardwareManager::createThings(Settings* settings, DmxManager* dmxManager, Scheduler* scheduler, std::function<void()> onCommandReceived) {
    switchables.clear();

    // PWMS
    if (settings->pwms.size() > 0) {
        analogWriteResolution(14);
        PwmThing::set8bitTo14BitMapping();
        for (auto& pwmCfg : settings->pwms) {
            // initialize pwm Things
            auto pwmThing = new PwmThing(pwmCfg.pin, pwmCfg.name);
            dmxManager->addMapping(pwmThing, pwmCfg.dmxCfg);
            switchables.push_back(pwmThing);
            pwms.push_back(pwmThing);
        }
        Log::infoln("PWMs created.");
    }

    Log::infoln("Creating RGBW strips ...");
    std::vector<InitializedThingGroup<RgbwThingGroup>> rgbwThingGroups = createStripThings<NeoGrbwFeature, NeoEsp32RmtNSk6812Method, RgbwThing, RgbwThingGroup>(rgbwStrips, settings->rgbwStrips);
    for (auto& rgbwThingGroup : rgbwThingGroups) {
        dmxManager->addMapping(rgbwThingGroup.group, rgbwThingGroup.dmxCfg);
        switchables.push_back(rgbwThingGroup.group);
    }

    Log::infoln("Creating RGB strips ...");
    std::vector<InitializedThingGroup<RgbThingGroup>> rgbThingsGroups = createStripThings<NeoGrbFeature, NeoEsp32RmtNWs2812xMethod, RgbThing, RgbThingGroup>(rgbStrips, settings->rgbStrips);
    for (auto& rgbThingGroup : rgbThingsGroups) {
        dmxManager->addMapping(rgbThingGroup.group, rgbThingGroup.dmxCfg);
        switchables.push_back(rgbThingGroup.group);
    }

    Log::infoln("Creating servos ...");
    for (auto& servoCfg : settings->servos) {
        auto minPulseWidth = servoCfg.minPulseWidth == 0 ? 500 : servoCfg.minPulseWidth;
        auto maxPulseWidth = servoCfg.maxPulseWidth == 0 ? 2500 : servoCfg.maxPulseWidth;
        auto thing = new ServoThing(servoCfg.pin, servoCfg.maxAngle, minPulseWidth, maxPulseWidth);
        dmxManager->addMapping(thing, servoCfg.dmxCfg);
        servos.push_back(thing);
    }

    // Plugins
    try {
        int createdAnimations = PluginFactory::getInstance().createAnimationsFromPlugins(
            scheduler, settings->plugins, dmxManager);
        Log::infoln("Created %d animations from plugins", createdAnimations);
    } catch (const std::exception& e) {
        Log::error((std::string("ERR: configuring plugins. ") + e.what()).c_str());
    }

    // DMX Output
    if (settings->dmxOutput.enabled) {
        Log::infoln("Creating DMX output on UART%d ...", settings->dmxOutput.uartPort);
        dmxOutput = new DmxOutput(
            settings->dmxOutput.uartPort,
            settings->dmxOutput.txPin,
            settings->dmxOutput.rxPin,
            settings->dmxOutput.enablePin,
            settings->dmxOutput.universe,
            dmxManager->getDmxData()
        );
        dmxManager->addUniverse(settings->dmxOutput.universe);
        if (dmxOutput->begin()) {
            scheduler->addTask(dmxOutput);
            Log::infoln("DMX output initialized successfully for universe %d", settings->dmxOutput.universe);
        } else {
            Log::errorln("Failed to initialize DMX output");
            delete dmxOutput;
            dmxOutput = nullptr;
        }
    } else {
        Log::info("DMX output disabled in settings.");
    }

    // DMX Input
    if (settings->dmxInput.enabled) {
        Log::infoln("Creating DMX input on UART%d ...", settings->dmxInput.uartPort);
        dmxInput = new DmxInput(
            settings->dmxInput.uartPort,
            settings->dmxInput.txPin,
            settings->dmxInput.rxPin,
            settings->dmxInput.enablePin,
            settings->dmxInput.universe,
            dmxManager->getDmxData(),
            onCommandReceived
        );
        dmxManager->addUniverse(settings->dmxInput.universe);
        if (dmxInput->begin()) {
            Log::infoln("Adding DmxInput to shcheduler task.");
            scheduler->addTask(dmxInput);
        } else {
            Log::errorln("Failed to initialize DMX input");
            delete dmxInput;
            dmxInput = nullptr;
        }
        dmxManager->initializeDmxData(dmxManager->getDmxData());
    } else {
        Log::info("DMX input disabled in settings.");
    }

    if (settings->lightsTest) {
        runLightsTest();
    }

    scheduler->addTask(ledCommitTask);
}

void HardwareManager::initializeSensors(Settings* settings, ConfigurablePipelineManager* pipelineManager) {
    if (!pipelineManager) {
        Log::warning("Pipeline manager is null, sensors will not be processed");
        return;
    }
    
    Log::infoln("Creating Hum/Temp sensor ...");
    for (size_t i = 0; i < settings->humTemps.size(); i++) {
        auto& humTempCfg = settings->humTemps[i];
        std::string tempSensorName = "temperature_" + std::to_string(i + 1);
        std::string humSensorName = "humidity_" + std::to_string(i + 1);
        
    auto humTempSensor = new HumTempSensor(humTempCfg.pin, humTempCfg.readMs);
    humTempSensor->addOnChangeListener([pipelineManager, tempSensorName, humSensorName](HumTemp data) {
        pipelineManager->processSensorValue(tempSensorName, data.temperature);
        pipelineManager->processSensorValue(humSensorName, data.humidity);
    });
    humTempSensors.push_back(humTempSensor);
}

Log::infoln("Creating touch sensors ...");
for (auto& touch : settings->touchSensors) {
    auto touchSensor = new TouchSensor(touch.pin, 200, touch.threshold);
    std::string sensorName = touch.sensorName;
    touchSensor->addOnChangeListener([pipelineManager, sensorName](bool touched) {
        float value = touched ? 1.0f : 0.0f;
        pipelineManager->processSensorValue(sensorName, value);
    });
    touchSensors.push_back(touchSensor);
}

    Log::infoln("Creating digital read sensors ...");
    for (auto& dreadCfg : settings->digitalReadSensors) {
        auto digitalReadSensor = new DigitalReadSensor(dreadCfg.pin, dreadCfg.readMs, INPUT_PULLUP);
        digitalReadSensor->addOnChangeListener([pipelineManager, dreadCfg](bool value) {
            float floatValue = value ? 1.0f : 0.0f;
            pipelineManager->processSensorValue(dreadCfg.sensorName, floatValue);
        });

        digitalReadSensor->addOnChangeListener([](bool value) {
            // TODO fix, on change called also when there is no change
            // Log::traceln("Digital read sensor value changed to: %d", value); // TODO uncommment
        });
        Log::infoln("Digital read sensor %d created.", dreadCfg.pin);
        digitalReadSensors[dreadCfg.pin] = digitalReadSensor;
    }

    Log::infoln("Creating analog read sensors ...");
    for (auto& areadCfg : settings->analogReadSensors) {
        auto analogReadSensor = new AnalogReadSensor(areadCfg.pin, areadCfg.readMs);
        analogReadSensor->addOnChangeListener([pipelineManager, areadCfg](uint16_t value) {
            float floatValue = static_cast<float>(value);
            pipelineManager->processSensorValue(areadCfg.sensorName, floatValue);
        });
        Log::infoln("Analog read sensor created. Pin: %d, readMs: %d", areadCfg.pin, areadCfg.readMs);
        analogReadSensors[areadCfg.pin] = analogReadSensor;
    }
}

void HardwareManager::readAllSensors() {
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
}

void HardwareManager::turnOffAllSwitchables() {
    for (auto& switchable : switchables) {
        switchable->off();
    }
}

void HardwareManager::runLightsTest() {
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
