# Sensor Processor Integration - How DMX Data Gets Set

## Current System Architecture

### Overview

The current system has this data flow:

```
Sensor Hardware → SensorBase → onChange callback → SensorEvents → DMX Data Array
                                                          ↓
                                                  (optional processor)
```

### Where DMX Data Lives

The DMX data is stored in **DmxListener** and accessed throughout the system:

```cpp
// In DmxListener class
std::map<uint16_t /*universe*/, std::array<uint8_t, 512> /*data*/> dmxData;
```

This is a map where:
- **Key**: Universe number (uint16_t)
- **Value**: Array of 512 DMX channels (0-255 each)

### Current Integration Points

#### 1. **Main Setup (main.cpp:161-165)**

The system creates `SensorEvents` and passes a reference to the DMX data:

```cpp
sensorEvents = new SensorEvents(
    networkManager->getMqtt(),
    std::string("np/") + hostName + "/s/",
    settings->sensorMappings,
    dmxListener->getDmxData()    // ← DMX data reference passed here
);
```

#### 2. **Sensor Initialization (HardwareManager.cpp:285-320)**

When sensors are created, they register callbacks with `SensorEvents`:

```cpp
// Touch sensor example
touchSensor->addOnChangeListener([sensorEvents, sensorName](bool touched) {
    sensorEvents->publish(sensorName, touched ? 1 : 0, false);
});

// Digital read sensor example
digitalReadSensor->addOnChangeListener([sensorEvents, dreadCfg](bool value) {
    sensorEvents->publish(dreadCfg.sensorName, value ? 1 : 0, true);
});

// Analog read sensor example
analogReadSensor->addOnChangeListener([sensorEvents, areadCfg](uint16_t value) {
    sensorEvents->publish(areadCfg.sensorName, value, true);
});
```

#### 3. **DMX Data Update (sensorEvents.h:66-76)**

`SensorEvents` sets DMX values based on sensor mappings:

```cpp
void applyProcessedValues(const std::map<std::string, int>& processedValues) {
    for (auto it = processedValues.begin(); it != processedValues.end(); ++it) {
        const std::string& sensorName = it->first;
        int dmxValue = it->second;
        
        for (const auto& mapping : sensorMappings) {
            if (mapping.sensorName == sensorName) {
                uint8_t dmxChannel = mapping.dmxCfg.get0BasedChannel();
                
                // Clamp to DMX range
                if (dmxValue < 0) dmxValue = 0;
                if (dmxValue > 255) dmxValue = 255;
                
                // ← DMX DATA IS SET HERE
                dmxData[mapping.dmxCfg.universe][dmxChannel] = static_cast<uint8_t>(dmxValue);
                break;
            }
        }
    }
}
```

---

## How to Integrate the NEW Pipeline System

You have **3 integration options**:

### Option 1: Direct Integration (Simplest)

Add pipelines directly to the sensor callbacks:

```cpp
// In HardwareManager::initializeSensors()

// Create pipeline for each sensor type
auto tempPipeline = std::make_shared<ProcessorPipeline>("temperature");
tempPipeline->addProcessor(std::make_shared<MovingAverageProcessor>(10));
tempPipeline->addProcessor(std::make_shared<RangeMappingProcessor>(-40, 100, 0, 255));

// Use it in the callback
analogReadSensor->addOnChangeListener([sensorEvents, areadCfg, tempPipeline](uint16_t value) {
    // Process through pipeline
    auto result = tempPipeline->process((float)value, millis());
    
    // Publish processed value
    sensorEvents->publish(areadCfg.sensorName, (int)result.value, true);
});
```

**Where DMX gets set:** Same as before - `SensorEvents::applyProcessedValues()` line 76

---

### Option 2: Use SensorPipelineManager (Recommended)

Replace or supplement `SensorEvents` with the new `SensorPipelineManager`:

```cpp
// In main.cpp setup()
#include <SensorPipelineIntegration.h>

// Create pipeline manager
auto pipelineManager = new SensorPipelineManager(dmxListener->getDmxData());

// Register sensors with pipelines
auto tempPipeline = PipelineBuilder("temp")
    .add<MovingAverageProcessor>(10)
    .add<RangeMappingProcessor>(-40, 100, 0, 255)
    .buildPtr();
    
pipelineManager->registerSensor("temperature", 1, 0, tempPipeline);

// In sensor callback
analogReadSensor->addOnChangeListener([pipelineManager](uint16_t value) {
    pipelineManager->processSensorValue("temperature", (float)value);
});
```

**Where DMX gets set:** `SensorPipelineManager::processSensorValue()` line 75:

```cpp
dmxData[mapping.universe][mapping.channel] = dmxValue;
```

---

### Option 3: Enhance Existing SensorEvents (Most Compatible)

Modify `SensorEvents` to support pipelines:

```cpp
// In sensorEvents.h - add pipeline support
class SensorEvents {
    private:
        std::map<std::string, std::shared_ptr<ProcessorPipeline>> pipelines;
        
    public:
        void registerPipeline(const std::string& sensorName, ProcessorPipeline* pipeline) {
            pipelines[sensorName] = std::shared_ptr<ProcessorPipeline>(pipeline);
        }
        
        void publish(std::string sensorName, int value, bool local) {
            // Process through pipeline if available
            float processedValue = value;
            auto it = pipelines.find(sensorName);
            if (it != pipelines.end()) {
                auto result = it->second->process((float)value, millis());
                processedValue = result.value;
            }
            
            // ... rest of existing code ...
            // Eventually sets: dmxData[universe][channel] = processedValue
        }
};
```

---

## Complete Integration Example

Here's a full example showing how to add pipeline processing to your existing system:

### Step 1: Include Headers (in main.cpp or HardwareManager.cpp)

```cpp
#include <sensorProcessors.h>
#include <SensorPipelineIntegration.h>
```

### Step 2: Create Pipelines in Setup

```cpp
// In main.cpp after sensorEvents creation:

// Create pipeline manager
auto pipelineManager = new SensorPipelineManager(dmxListener->getDmxData());

// Create temperature pipeline
auto tempPipeline = PipelineBuilder("temperature")
    .add<MedianFilterProcessor>(5)
    .add<ExponentialMovingAverageProcessor>(0.3f)
    .add<RangeMappingProcessor>(-40.0f, 100.0f, 0.0f, 255.0f)
    .buildPtr();
pipelineManager->registerSensor("dht_temperature", 1, 0, tempPipeline);

// Create motion pipeline  
auto motionPipeline = PipelineBuilder("motion")
    .add<DebounceProcessor>(100)
    .add<ScaleProcessor>(255.0f)
    .add<PersistenceProcessor>(10000, 255.0f, 0.0f)
    .add<TimeoutProcessor>(60000, 0.0f, 0.0f)
    .buildPtr();
pipelineManager->registerSensor("pir_motion", 1, 1, motionPipeline);
```

### Step 3: Modify Sensor Callbacks

In `HardwareManager::initializeSensors()`:

```cpp
// Pass pipelineManager to the function
void HardwareManager::initializeSensors(Settings* settings, 
                                       SensorEvents* sensorEvents,
                                       SensorPipelineManager* pipelineManager) {
    
    // For sensors with pipelines
    analogReadSensor->addOnChangeListener([pipelineManager, sensorName](uint16_t value) {
        pipelineManager->processSensorValue(sensorName, (float)value);
    });
    
    // For sensors without pipelines (backward compatible)
    digitalReadSensor->addOnChangeListener([sensorEvents, sensorName](bool value) {
        sensorEvents->publish(sensorName, value ? 1 : 0, true);
    });
}
```

### Step 4: DMX Data Flow

```
Sensor Reading (e.g., 25.5°C)
       ↓
Sensor Callback (onChange)
       ↓
pipelineManager->processSensorValue("temperature", 25.5)
       ↓
Pipeline Processing:
  1. MedianFilter → 25.5
  2. EMA → 25.4
  3. RangeMapping → 182 (DMX value)
       ↓
Set DMX: dmxData[1][0] = 182  ← DMX DATA SET HERE
       ↓
DmxListener broadcasts to Art-Net
       ↓
DMX Output Hardware
```

---

## Configuration-Based Setup

You can also load pipelines from your existing YAML config:

### Add to settings.h:

```cpp
struct SensorPipelineConfig {
    std::string sensorName;
    uint16_t universe;
    uint8_t channel;
    std::vector<ProcessorConfig> processors;
};
```

### Load from config:

```cpp
// Parse YAML config
PipelineConfig config = SensorProcessorFactory::parsePipelineConfig(jsonObject);
ProcessorPipeline* pipeline = SensorProcessorFactory::createPipeline(config);

// Register with manager
pipelineManager->registerSensor(config.name, universe, channel, pipeline);
```

---

## Summary: Where DMX Gets Set

### Current System
**File:** `lib/sensorEvents/sensorEvents.h`  
**Line:** 76  
**Code:** `dmxData[mapping.dmxCfg.universe][dmxChannel] = static_cast<uint8_t>(dmxValue);`

### New Pipeline System
**File:** `lib/sensorProcessors/SensorPipelineIntegration.h`  
**Line:** 75  
**Code:** `dmxData[mapping.universe][mapping.channel] = dmxValue;`

### Both systems set the **same DMX data structure**:
```cpp
std::map<uint16_t, std::array<uint8_t, 512>>& dmxData
```

This data structure is:
- **Owned by:** DmxListener
- **Accessed by:** SensorEvents, SensorPipelineManager, animations, web admin
- **Broadcast by:** DmxListener via Art-Net

---

## Quick Start Integration

To integrate with minimal changes:

1. **Add include** in `HardwareManager.h`:
   ```cpp
   #include <sensorProcessors.h>
   ```

2. **Create pipelines** in `HardwareManager::initializeSensors()`:
   ```cpp
   auto pipeline = PipelineBuilder("sensor")
       .add<MovingAverageProcessor>(10)
       .add<RangeMappingProcessor>(0, 1023, 0, 255)
       .buildPtr();
   ```

3. **Use in callbacks**:
   ```cpp
   sensor->addOnChangeListener([pipeline, &dmxData](uint16_t value) {
       auto result = pipeline->process(value, millis());
       dmxData[1][0] = (uint8_t)result.value;  // Direct DMX set
   });
   ```

That's it! The pipeline processes the sensor data and you set DMX directly.
