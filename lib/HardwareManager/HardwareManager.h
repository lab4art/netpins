#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include <map>
#include <vector>
#include <array>
#include <NeoPixelBus.h>
#include <Things.h>
#include <sensors.h>
#include <settings.h>
#include <DmxListener.h>
#include <DmxOutput.h>
#include <DmxInput.h>
#include <scheduler.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Forward declaration
class SensorEvents;
class HardwareManager;
class LedCommitTask;

template<class ThingGroupType>
class InitializedThingGroup {
    public:
        ThingGroupType* group;
        DmxCfg dmxCfg;
        
        InitializedThingGroup(ThingGroupType* group, DmxCfg dmxCfg) 
            : group(group), dmxCfg(dmxCfg) {}
};

class HardwareManager {
private:
    // Hardware components
    std::map<int /* pin */, NeoPixelBus<NeoGrbwFeature, NeoEsp32RmtNSk6812Method>*> rgbwStrips;
    std::map<int /* pin */, NeoPixelBus<NeoGrbFeature, NeoEsp32RmtNWs2812xMethod>*> rgbStrips;
    std::vector<PwmThing*> pwms;
    std::vector<ServoThing*> servos;
    std::vector<Switchabe*> switchables;
    DmxOutput* dmxOutput;
    DmxInput* dmxInput;

    // Sensors
    std::vector<HumTempSensor*> humTempSensors;
    std::vector<TouchSensor*> touchSensors;
    std::map<uint8_t /* pin */, DigitalReadSensor*> digitalReadSensors;
    std::map<uint8_t /* pin */, AnalogReadSensor*> analogReadSensors;
    
    // FreeRTOS task management for LED commits
    xSemaphoreHandle semaphore;
    TaskHandle_t commitNeoStipTask;
    LedCommitTask* ledCommitTask;
    
    // Strip creation counter
    int numOfCreatedStrips;
    
    // Private methods
    template<typename Feature, typename Method>
    void createStrip(int pin, int maxNeopx, std::map<int, NeoPixelBus<Feature, Method>*>& strips);
    
    template<typename Feature, typename Method, class ThingType, class ThingGroupType>
    std::vector<InitializedThingGroup<ThingGroupType>> createStripThings(    
        std::map<int, NeoPixelBus<Feature, Method> *>& strips,
        std::vector<StripeCfg> stripeCfgs
    );
    
    void doCommitThings();
    static void commitNeoStipTaskProcedure(void *arg);
    void runLightsTest();
    void initNeoStipTask();
    
public:
    HardwareManager();
    ~HardwareManager();
    
    // Initialization
    void createThings(Settings& settings, DmxListener* dmxListener, Scheduler* scheduler, std::function<void()> onCommandReceived = nullptr);
    void initializeSensors(Settings& settings, SensorEvents* sensorEvents);
    
    // Commit changes to hardware
    void commitNeoStip();
    
    // Sensor accessors
    const std::vector<HumTempSensor*>& getHumTempSensors() const { return humTempSensors; }
    // const std::vector<TouchSensor*>& getTouchSensors() const { return touchSensors; }
    // const std::map<uint8_t, DigitalReadSensor*>& getDigitalReadSensors() const { return digitalReadSensors; }
    // const std::map<uint8_t, AnalogReadSensor*>& getAnalogReadSensors() const { return analogReadSensors; }
    
    // Switchables control
    void turnOffAllSwitchables();
    
    // Sensor reading loop
    void readAllSensors();
};

#endif // HARDWARE_MANAGER_H
