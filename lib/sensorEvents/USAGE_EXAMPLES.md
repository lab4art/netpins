# SensorEventProcessor Usage Examples

The `SensorEventProcessor` allows you to collect sensor events and process them conditionally before setting DMX values.

## Configuration-Based Usage (Recommended)

The easiest way to use processors is through YAML/JSON configuration. The factory will create and configure the processor automatically.

### Configuration Format

```yaml
sensor_processor:
  name: averaging          # Processor type: averaging, median, peak, ema, threshold, change, gesture, passthrough
  max_events: 10          # Process after collecting N events
  max_time_ms: 1000       # Or after N milliseconds
  min_events: 1           # Minimum events required before processing
  apply_value_mapping: true  # Map sensor values to DMX range (0-255)
  params:                 # Processor-specific parameters
    threshold: 500        # For peak/threshold processors
    alpha: 0.3           # For EMA processor
    delta: 50            # For change detector
    sequence:            # For gesture processor
      - sensor1
      - sensor2
```

### Code Usage with Config

```cpp
#include <ProcessorFactory.h>
#include <sensorEvents.h>

// Create processor from config
auto [config, processor] = ProcessorFactory::createFromConfig(
    settings.sensorProcessor, 
    settings.sensorMappings
);

// Create SensorEvents with the processor
SensorEvents sensorEvents(mqtt, topicPrefix, settings.sensorMappings, dmxData, processor);

// Events are automatically processed based on config
sensorEvents.publish("temperature", 100, true);
```

## Available Processors

### 1. Averaging Processor
Collects events and returns the average value for each sensor.

**Config:**
```yaml
sensor_processor:
  name: averaging
  max_events: 5
  max_time_ms: 500
  min_events: 3
  apply_value_mapping: true
```

### 2. Median Filter
Returns the median value (good for noise reduction).

**Config:**
```yaml
sensor_processor:
  name: median
  max_events: 7
  max_time_ms: 300
  min_events: 5
  apply_value_mapping: true
```

### 3. Peak Detector
Only triggers when sensor value exceeds threshold.

**Config:**
```yaml
sensor_processor:
  name: peak
  max_events: 10
  max_time_ms: 1000
  min_events: 1
  apply_value_mapping: true
  params:
    threshold: 500
```

### 4. Exponential Moving Average (EMA)
Applies smoothing to sensor values.

**Config:**
```yaml
sensor_processor:
  name: ema
  max_events: 3
  max_time_ms: 200
  min_events: 1
  apply_value_mapping: true
  params:
    alpha: 0.3  # 0.0 = more smoothing, 1.0 = no smoothing
```

### 5. Threshold Gate
Only allows values through if they meet threshold criteria.

**Config:**
```yaml
sensor_processor:
  name: threshold
  max_events: 5
  max_time_ms: 500
  min_events: 1
  apply_value_mapping: true
  params:
    threshold: 500
    above: true  # true = must be above threshold, false = must be below
```

### 6. Change Detector
Only triggers when value changes by more than delta.

**Config:**
```yaml
sensor_processor:
  name: change
  max_events: 10
  max_time_ms: 1000
  min_events: 1
  apply_value_mapping: true
  params:
    delta: 50  # Minimum change required
```

### 7. Gesture Detector
Detects sequential sensor triggers.

**Config:**
```yaml
sensor_processor:
  name: gesture
  max_events: 20
  max_time_ms: 2000
  min_events: 2
  apply_value_mapping: false
  params:
    sequence:
      - sensor1
      - sensor2
      - sensor3
    threshold: 100
    max_time_between: 500  # Max ms between triggers
```

### 8. Pass-Through
Returns the most recent value without processing (for testing).

**Config:**
```yaml
sensor_processor:
  name: passthrough
  max_events: 1
  max_time_ms: 100
  min_events: 1
  apply_value_mapping: true
```

## Programmatic Usage (Advanced)

You can also create processors programmatically:

### Manual Averaging Processor

```cpp
#include <sensorEvents.h>
#include <SensorEventProcessor.h>

// Configure the processor
ProcessorConfig config;
config.maxEventsBeforeProcess = 5;      // Process after 5 events
config.maxTimeBeforeProcess = 500;      // Or after 500ms
config.minEventsForProcess = 3;         // Need at least 3 events

// Enable value range mapping in the processor
config.applyValueRangeMapping = true;
config.valueRangeMappings["temperature"] = ValueRangeMapping(0, 1023, 0, 255);
config.valueRangeMappings["light"] = ValueRangeMapping(0, 4095, 0, 255);

// Define processing logic: average the values
auto averagingProcessor = [](const std::vector<SensorEvent>& events) -> ProcessingResult {
    ProcessingResult result;
    result.shouldSetDmx = true;
    
    // Group events by sensor name and average their values
    std::map<std::string, std::vector<int>> sensorValues;
    for (const auto& event : events) {
        sensorValues[event.sensorName].push_back(event.value);
    }
    
    // Calculate averages (in original sensor range)
    for (const auto& [sensorName, values] : sensorValues) {
        int sum = 0;
        for (int val : values) {
            sum += val;
        }
        int average = sum / values.size();
        // Return raw sensor value - processor will map to DMX range (0-255)
        result.values[sensorName] = average;
    }
    
    return result;
};

// Create the processor
SensorEventProcessor* processor = new SensorEventProcessor(config, averagingProcessor);

// Create SensorEvents with the processor
SensorEvents sensorEvents(mqtt, topicPrefix, sensorMappings, dmxData, processor);

// Now when you publish sensor events, they will be collected and averaged
sensorEvents.publish("temperature", 100, true);  // Collected
sensorEvents.publish("temperature", 105, true);  // Collected
sensorEvents.publish("temperature", 95, true);   // Collected
// ... continues collecting until condition is met
// When processed, average (100) will be mapped from 0-1023 range to 0-255 range
```

## Direct Mode (Without Processor)

If you don't need conditional processing, simply don't configure a processor or pass nullptr:

```cpp
// Original behavior: immediate DMX mapping
SensorEvents sensorEvents(mqtt, topicPrefix, sensorMappings, dmxData);
sensorEvents.publish("temperature", 100, true);  // Sets DMX immediately
```

## Force Processing

You can manually trigger processing before conditions are met:

```cpp
// In your main loop or based on some external trigger
sensorEvents.forceProcess();
```

## Configuration Parameters

- `maxEventsBeforeProcess`: Maximum events to collect before auto-processing
- `maxTimeBeforeProcess`: Maximum time (ms) before auto-processing
- `minEventsForProcess`: Minimum events required before processing is allowed
- `applyValueRangeMapping`: Enable value range mapping in the processor (default: false)
- `valueRangeMappings`: Per-sensor value range mappings (fromMin, fromMax, toMin, toMax)

Processing triggers when:
- Event count >= `maxEventsBeforeProcess`, OR
- Time since first event >= `maxTimeBeforeProcess`
- AND event count >= `minEventsForProcess`

### Value Range Mapping Options

**Option 1: Processor handles mapping** (`applyValueRangeMapping = true`)
- Define mappings in `ProcessorConfig`
- Processor returns DMX-ready values (0-255 or custom range)
- More flexible - can use different ranges per sensor independent of config file
- Good for complex processing where you want control over output ranges

**Option 2: SensorEvents handles mapping** (`applyValueRangeMapping = false`, default)
- Uses `valueRange` from `SensorMappingCfg` in settings
- Processor returns raw sensor values
- SensorEvents applies mapping using config file settings
- Better for simple averaging/filtering where you want to keep config-based ranges
