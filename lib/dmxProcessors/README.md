# DMX Processors

DMX Processors are a system for applying effects and transformations directly to DMX channel data. Unlike sensor processors which transform sensor readings, DMX processors operate on DMX universe data to create visual effects.

## Configuration

DMX Processors can be configured via YAML similar to sensor processors, with optional control channels for sensor-driven activation:

```yaml
dmx_processors:
  - type: cue_list
    name: scene_player
    control_channel: 100@0      # Optional: controlled by DMX channel
    enable_threshold: 2.0       # Enable when channel >= 2
    disable_threshold: 1.5      # Disable when channel < 1.5 (hysteresis)
    auto_advance: true
    loop: true
    scenes:
      - channels:
          1@0: 255  # channel@universe: value
          2@0: 200
        fade_in_ms: 2000
        hold_ms: 5000
```

**Control Parameters:**
- `control_channel`: DMX channel to monitor (format: `channel@universe`). Omit for always-on processors.
- `enable_threshold`: Enable processor when channel value >= this threshold
- `disable_threshold`: Disable when channel value < this threshold (defaults to enable_threshold)

See [dmx-processors-config.yaml](../../gitignored/dmx-processors-config.yaml) for complete examples.

## Concept

```
Sensor Data → Sensor Processors → DMX Values → DMX Processors → Final Output
              (data transform)     ↓ control    (visual effects)
                                   └─────────────────┘
```

DMX Processors work at the DMX layer and can be controlled by DMX channel values set by sensor processors. This allows sensor data to trigger and control visual effects.

## Available Processors

### DmxCueListProcessor

Manages scenes (cues) with smooth fading transitions. Each scene is a configurable map of DMX channels to values with individual fade-in times.

**Use Cases:**
- Lighting scene playback
- Smooth transitions between states
- Time-based scene sequences
- Manual scene control
- Strobe effects (using fast scene transitions)

**Configuration:**
```cpp
auto cueList = new DmxCueListProcessor();

// Scene 1: Warm white - 2 second fade
cueList->addScene(0,  // universe
    {
        {1, 255},   // channel 1 = 255
        {2, 200},   // channel 2 = 200
        {3, 100}    // channel 3 = 100
    },
    2000,  // fadeInMs: 2 second fade
    5000   // holdMs: hold for 5 seconds
);

// Scene 2: Cool blue - 1 second fade
cueList->addScene(0,  // universe
    {
        {1, 0},
        {2, 100},
        {3, 255}
    },
    1000,  // fadeInMs: 1 second fade
    3000   // holdMs: hold for 3 seconds
);

// Scene 3: Off - 3 second fade
cueList->addScene(0,  // universe
    {
        {1, 0},
        {2, 0},
        {3, 0}
    },
    3000,  // fadeInMs: 3 second slow fade to black
    2000   // holdMs
);

cueList->setAutoAdvance(true);  // Automatically advance through scenes
cueList->setLoop(true);         // Loop back to scene 1
```

**Multi-universe scenes:**
```cpp
// Create scene across multiple universes using DmxCfg
std::map<DmxCfg, uint8_t> channels;
channels[{0, 1}] = 255;  // Universe 0, Channel 1
channels[{0, 2}] = 200;  // Universe 0, Channel 2
channels[{1, 1}] = 100;  // Universe 1, Channel 1

cueList->addScene(channels, 1000, 5000);
```

**Strobe Effect with Cue List:**
```cpp
// Create a rapid strobe effect using two-scene cue list
auto strobeEffect = new DmxCueListProcessor();

// Scene 1: ON (255)
strobeEffect->addScene(0, {{1, 255}, {2, 255}, {3, 255}}, 0, 100);  // 0ms fade, 100ms hold

## Integration

### Method 1: YAML Configuration (Recommended)

Define processors in your settings YAML:

```yaml
sensor_pipelines:
  - sensor: motion_sensor
    pipeline:
      - type: motion_state
        persistence_ms: 30000
    dmx: 100@0  # Output motion state to channel 100

dmx_processors:
  - type: cue_list
    name: motion_strobe
    control_channel: 100@0      # Controlled by motion state
    enable_threshold: 2.0       # Enable when state >= 2 (persisted)
    disable_threshold: 1.5
    auto_advance: true
    loop: true
    scenes:
      - channels: {1@0: 255}
        fade_in_ms: 0
        hold_ms: 100
      - channels: {1@0: 0}
        fade_in_ms: 0
        hold_ms: 200
```

Then create and register the manager from settings:

```cpp
#include <dmxProcessors.h>

// Create the processor manager
auto procManager = new DmxProcessorManager(dmxManager, 20);

// Load processors from settings
for (const auto& cfg : settings.dmxProcessors) {
    DmxProcessor* processor = DmxProcessorFactory::createProcessor(cfg);
    if (processor) {
        if (cfg.controlChannel.empty()) {
            // Always-on processor
            procManager->addAlwaysOnProcessor(processor);
        } else {
            // Controlled by DMX channel
            DmxCfg ctrlCh = DmxCfg::deserialize(cfg.controlChannel);
            if (cfg.disableThreshold != cfg.enableThreshold) {
                procManager->addProcessorWithHysteresis(
                    processor, ctrlCh, 
                    cfg.enableThreshold, cfg.disableThreshold);
            } else {
                procManager->addProcessor(processor, ctrlCh, cfg.enableThreshold);
            }
        }
    }
}

// Add manager to scheduler
scheduler->addTask(procManager);
```

See [dmx-processor-manager-example.cpp](../../gitignored/dmx-processor-manager-example.cpp) for complete integration example.

### Method 2: C++ API

Create processors programmatically:

```cpp
// In HardwareManager.cpp or similar:

// Create scheduled task wrapper
class DmxProcessorTask : public ScheduledTask {
    DmxProcessor* processor;
    DmxManager* dmxManager;
public:
    DmxProcessorTask(DmxProcessor* p, DmxManager* dmx)
        : ScheduledTask(20, "DmxProcessor"),
          processor(p),
          dmxManager(dmx) {}
    
    void callback() override {
        processor->process(dmxManager->getDmxData(), millis());
    }
};

// Add to scheduler
auto processorTask = new DmxProcessorTask(cueList, dmxManager);
scheduler->addTask(processorTask);
```

## Complete Example: Motion-Activated Strobe

### Step 1: Sensor Pipeline (YAML)
```yaml
sensor_pipelines:
  - sensor: motion_sensor
    pipeline:
      - type: motion_state
        persistence_ms: 30000
        no_motion_ms: 30000
        threshold: 0.0
    dmx: 1@0  # Outputs state 0, 1, or 2
```

### Step 2: DMX Processor (C++)
```cpp
// Create conditional strobe that activates on motion state 2
auto strobeEffect = new DmxCueListProcessor();

// Strobe pattern: 50ms on, 200ms off (quick flash)
strobeEffect->addScene(0, {{1, 255}, {2, 255}, {3, 255}}, 0, 50);
strobeEffect->addScene(0, {{1, 0}, {2, 0}, {3, 0}}, 0, 200);
strobeEffect->setAutoAdvance(true);
strobeEffect->setLoop(true);

// Create custom processor to enable/disable based on motion state
class ConditionalStrobeTask : public ScheduledTask {
    DmxCueListProcessor* strobe;
    DmxManager* dmxManager;
public:
    ConditionalStrobeTask(DmxCueListProcessor* s, DmxManager* dmx)
        : ScheduledTask(50, "ConditionalStrobe"), strobe(s), dmxManager(dmx) {}
    
    void callback() override {
        auto& data = dmxManager->getDmxData();
        uint8_t motionState = data[0][0];  // Channel 1 has motion state
        
        // Enable strobe when motion persists (state 2)
        if (motionState >= 2) {
            strobe->setEnabled(true);
            strobe->process(data, millis());
        } else {
            strobe->setEnabled(false);
            // Set to steady value based on state
            data[0][0] = motionState >= 1 ? 255 : 0;  // On if motion, off if no motion
            data[0][1] = motionState >= 1 ? 255 : 0;
            data[0][2] = motionState >= 1 ? 255 : 0;
        }
    }
};

// Add to scheduler
scheduler->addTask(new ConditionalStrobeTask(strobeEffect, dmxManager));
```

### Result
- Motion detected (state 1): Channels = 255 (steady on)
- Motion persists 30s (state 2): Channels = strobe (0 ↔ 255)
- No motion 30s (state 0)

## Complete Example: Motion + Strobe

### Step 1: Sensor Pipeline (YAML)
```yaml
sensor_pipelines:
  - sensor: motion_sensor
    pipeline:
      - type: scale
        factor: 255.0
      - type: motion_state
        persistence_ms: 30000
        normal: 255.0      # Motion detected
        persisted: 510.0   # Trigger strobe
        low: 0.0           # No motion
    dmx: 1@0
```

### Step 2: DMX Processor (C++)
```cpp
// Create strobe that triggers at 510
// Quick flash: 50ms on, 200ms off
auto strobe = new DmxStrobeProcessor(50, 200, 510.0f);
strobe->addChannels(0, {1, 2, 3});

// Add to scheduler
scheduler->addTask(new DmxProcessorTask(strobe, dmxManager));
```

### Result
- Motion detected: Channels = 255 (steady on)
- Motion persists 30s: Channels = strobe (0 ↔ 255 @ 5Hz)
- No motion 30s: Channels = 0 (off)

## Creating Custom DMX Processors

### Base Class
```cppyCustomProcessor : public DmxProcessor {
public:
    MyCustomProcessor() : DmxProcessor("MyEffect") {}
    
    void process(
        std::map<uint16_t, std::array<uint8_t, 512>>& dmxData,
        unsigned long currentTime
    ) override {
        if (!enabled) return;
        
        // Your effect logic here
        // Modify dmxData directly
    }
    
    void reset() override {
        // Reset internal state
    }
};
```

### Ideas for Custom Processors
- **DmxFadeProcessor** - Smooth fade in/out
- **DmxRainbowProcessor** - RGB color cycling
- **DmxWaveProcessor** - Wave patterns across channels
- **DmxFlickerProcessor** - Candle/fire flicker effect
- **DmxChaseProcessor** - Sequential channel chase
- **DmxDimmerProcessor** - Master dimmer/brightness
- **DmxColorMixProcessor** - Color temperature/mixing
- **DmxPulseProcessor** - Smooth pulse/breathe effect

## Performance Notes

- DMX Processors run at scheduled intervals (typically 20-50ms)
- Keep processing lightweight - runs in main loop
- Avoid blocking operations
- Use timestamps for timing, not delays
- Multiple processors can run simultaneously
- Processors modify DMX data in-place

## Trigger Methods

DMX Processors can be triggered by various sources:

### 1. Sensor Values
Sensor processors output trigger values (e.g., 510):
```yamlstate values to control effects:
```yaml
- type: motion_state
  persistence_ms: 30000  # Outputs state 2 to trigger effects
```

### 2. MQTT Commands
```cpp
// In MQTT callback:
if (topic == "cue/go") {
    cueList->nextCue();
}
```

### 3. Time-Based
```cpp
// In scheduled task:
if (hour >= 20 && hour < 22) {
    cueList->setEnabled(true);
} else {
    cueList->setEnabled(false);
}
```

### 4. Manual/API
```cpp
// Direct API call:
cueList->goToCue(3);
cueList

### 5. ArtNet Input
External DMX console can set values that trigger effects.

## Architecture Benefits

✅ **Separation of Concerns**
- Sensor processing separate from visual effects
- Each layer has clear responsibility

✅ **Reusability**
- Effects work with any trigger source
- Same effect can be applied to different situations

✅ **Flexibility**
- Mix and match sensors and effects
- Multiple effects on same channels
- Easy to add new effects

✅ **Maintainability**
- Effects code isolated from sensor code
- Easy to test independently
- Clear interfaces

## See Also

- [Sensor Processors](../sensorProcessors/) - Data transformation layer
- [ARCHITECTURE_REFACTOR.md](../../gitignored/ARCHITECTURE_REFACTOR.md) - Complete architecture
- [motion-with-dmx-processors.yaml](../../gitignored/motion-with-dmx-processors.yaml) - Example config
