# Sensor Processor Pipeline Library

A comprehensive, modular system for processing sensor data through configurable pipelines. Perfect for embedded systems that need to transform raw sensor readings into usable control values (e.g., DMX, PWM).

## Features

✅ **Modular Architecture**: Chain multiple processors in sequence  
✅ **20+ Processor Types**: Range mapping, smoothing, thresholding, time-based operations  
✅ **Easy Configuration**: Load pipelines from JSON/YAML config  
✅ **Fluent Builder API**: Intuitive pipeline construction  
✅ **Memory Efficient**: Optimized for embedded systems  
✅ **Well Documented**: Extensive examples and usage guides  

## Quick Start

### Configuration-Based (Recommended)

The easiest way to use the pipeline system is through configuration files:

```yaml
# In your config.yaml
sensor_pipelines:
  - sensor: temperature_1
    value_range:
      from: -40
      to: 100
    pipeline:
      - type: range_mapping
        input_min: -40.0
        input_max: 100.0
        output_min: 0.0
        output_max: 255.0
      - type: moving_average
        window_size: 5
      - type: debounce
        duration_ms: 500
    dmx: 10@1  # Channel 10, Universe 1
```

```cpp
#include <sensorProcessors.h>

// Initialize once in setup()
ConfigurablePipelineManager* pipelineManager = new ConfigurablePipelineManager(
    dmxData,    // Reference to DMX data structure
    settings    // Reference to Settings object
);
pipelineManager->initialize();

// Use in sensor callbacks
void onTemperature(float temp) {
    pipelineManager->processSensorValue("temperature_1", temp);
    // DMX value is automatically set!
}
```

See **[CONFIG_INTEGRATION_GUIDE.md](CONFIG_INTEGRATION_GUIDE.md)** for complete setup instructions.

### Programmatic (Advanced)

For direct code-based pipeline construction:

```cpp
#include <sensorProcessors.h>

// Create a pipeline for a temperature sensor
auto pipeline = PipelineBuilder("temperature")
    .add<MovingAverageProcessor>(10)                    // Average 10 readings
    .add<RangeMappingProcessor>(-40, 100, 0, 255)       // Map -40°C to 100°C → 0-255
    .build();

// Process sensor reading
float rawTemp = 25.5;  // °C
SensorData result = pipeline.process(rawTemp, millis());
uint8_t dmxValue = (uint8_t)result.value;  // Use for DMX output
```

### Motion Sensor with Persistence

```cpp
// Motion sensor: activate after 10s, deactivate after 60s
auto pipeline = PipelineBuilder("motion")
    .add<ScaleProcessor>(255.0f)                        // 0/1 → 0/255
    .add<PersistenceProcessor>(10000, 255.0f, 0.0f)     // Persist 10s
    .add<TimeoutProcessor>(60000, 0.0f, 0.0f)           // Timeout 60s
    .build();
```

## Processor Categories

### 📊 Range Mapping
- **RangeMappingProcessor**: Map values between ranges (e.g., -40-100°C → 0-255)
- **ScaleProcessor**: Multiply and offset
- **ClampProcessor**: Limit to min/max
- **InvertProcessor**: Invert within range
- **RoundProcessor**: Round to precision

### ⏱️ Time-Based
- **DebounceProcessor**: Require stable value for duration
- **PersistenceProcessor**: Value must persist for duration before triggering
- **TimeoutProcessor**: Reset after inactivity period
- **RateLimitProcessor**: Limit update frequency
- **DelayProcessor**: Delay output by duration

### 🎚️ Threshold
- **ThresholdProcessor**: Simple above/below threshold
- **HysteresisProcessor**: Prevent oscillation with upper/lower bounds
- **TimeBasedThresholdProcessor**: Threshold must persist for duration
- **RangeThresholdProcessor**: Trigger when value in range
- **DeadZoneProcessor**: Ignore values near center

### 📈 Smoothing
- **MovingAverageProcessor**: Simple moving average
- **ExponentialMovingAverageProcessor**: Weighted toward recent values
- **MedianFilterProcessor**: Remove spikes/outliers
- **WeightedMovingAverageProcessor**: Custom weights
- **LowPassFilterProcessor**: RC-style low-pass filter
- **KalmanFilterProcessor**: Optimal estimator

## Common Use Cases

### Noisy Temperature Sensor
```cpp
auto pipeline = PipelineBuilder("temp")
    .add<MedianFilterProcessor>(7)              // Remove spikes
    .add<ExponentialMovingAverageProcessor>(0.3f) // Smooth
    .add<RangeMappingProcessor>(-40, 100, 0, 255)
    .build();
```

### Smart Motion Detection
```cpp
auto pipeline = PipelineBuilder("motion")
    .add<DebounceProcessor>(100)                // Debounce 100ms
    .add<PersistenceProcessor>(5000, 1, 0)      // Must persist 5s
    .add<ScaleProcessor>(255.0f)                // Scale to DMX
    .add<TimeoutProcessor>(120000, 0, 0)        // 2min timeout
    .build();
```

### Temperature Alarm
```cpp
auto pipeline = PipelineBuilder("alarm")
    .add<MovingAverageProcessor>(10)
    .add<TimeBasedThresholdProcessor>(30, 60000, 255, 0)  // >30°C for 60s
    .build();
```

## Configuration

### YAML Configuration

```yaml
sensor_pipelines:
  - name: "temperature_control"
    sensor: "dht_temperature"
    dmx: "1@0"
    processors:
      - type: "moving_average"
        window_size: 10
      
      - type: "range_mapping"
        from_min: -40.0
        from_max: 100.0
        to_min: 0.0
        to_max: 255.0
```

### Load from Config

```cpp
#include <SensorProcessorFactory.h>

PipelineConfig config = SensorProcessorFactory::parsePipelineConfig(jsonObject);
ProcessorPipeline* pipeline = SensorProcessorFactory::createPipeline(config);
```

## Documentation

### Getting Started
- **[CONFIG_INTEGRATION_GUIDE.md](CONFIG_INTEGRATION_GUIDE.md)**: Complete configuration-based setup (recommended)
- **[GETTING_STARTED.md](GETTING_STARTED.md)**: 5-minute quick start tutorial
- **[QUICK_REFERENCE.md](QUICK_REFERENCE.md)**: Quick lookup for common requirements

### Examples & Configuration
- **[examples/pipeline_config_examples.yaml](examples/pipeline_config_examples.yaml)**: 10 complete YAML examples
- **[examples/quick_start.yaml](examples/quick_start.yaml)**: Minimal starter configuration
- **[CONFIG_EXAMPLES.md](CONFIG_EXAMPLES.md)**: Configuration patterns and use cases

### Comprehensive Guides
- **[USAGE.md](USAGE.md)**: Complete usage guide with patterns and best practices
- **[FEATURES.md](FEATURES.md)**: Feature comparison and performance characteristics
- **[ARCHITECTURE.md](ARCHITECTURE.md)**: System architecture with visual diagrams

### Integration
- **[CONFIG_INTEGRATION_GUIDE.md](CONFIG_INTEGRATION_GUIDE.md)**: Configuration-based integration
- **[INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md)**: Manual integration guide
- **[DMX_FLOW.md](DMX_FLOW.md)**: DMX data flow visualization

## Architecture

```
Raw Sensor Data → [Processor 1] → [Processor 2] → ... → [Processor N] → DMX Output
                      ↓                ↓                      ↓
                   Median          Smoothing            Range Mapping
                   Filter            EMA                 (0-255)
```

Each processor:
- Receives `SensorData` (value + metadata)
- Transforms the data
- Passes to next processor
- Can be enabled/disabled individually
- Can be reset to clear state

## Advanced Features

### Dynamic Pipeline Modification

```cpp
pipeline.addProcessor(std::make_shared<MedianFilterProcessor>(5));
pipeline.insertProcessor(1, std::make_shared<ThresholdProcessor>(128));
pipeline.removeProcessor(0);
pipeline.getProcessor(0)->setEnabled(false);
```

### Debugging

```cpp
pipeline.printStructure();
// Output:
// Pipeline 'temperature' (enabled, 3 processors):
//   [0] MedianFilter (enabled)
//   [1] ExponentialMovingAverage (enabled)
//   [2] RangeMapping (enabled)
```

### Reset State

```cpp
pipeline.reset();  // Clear all processor history/state
```

## Performance

- **Minimal overhead**: ~100 bytes per pipeline + processor storage
- **Fast execution**: Direct function calls, no virtual dispatch overhead in tight loops
- **Memory efficient**: Only processors that need history use memory

Memory usage examples:
- MovingAverage(10): ~40 bytes
- MedianFilter(7): ~28 bytes
- Threshold: ~4 bytes
- RangeMapping: ~0 bytes (no state)

## Integration with Existing System

```cpp
// In your sensor reading code
float rawValue = readTemperatureSensor();
SensorData processed = temperaturePipeline.process(rawValue, millis());
setDmxChannel(universe, channel, (uint8_t)processed.value);
```

## Requirements

- C++11 or later
- Arduino framework (for `millis()`)
- ArduinoJson library (for configuration parsing)

## Examples

See the `CONFIG_EXAMPLES.md` file for 10 complete examples:
1. Temperature with averaging and mapping
2. Motion sensor with persistence and timeout
3. Temperature with time-based threshold
4. Light sensor with hysteresis
5. Touch sensor with debounce
6. Distance sensor with inverted mapping
7. Analog sensor with dead zone
8. Multi-sensor averaging
9. Complex multi-stage processing
10. Complete weather station

## License

See LICENSE file in repository root.

## Contributing

Contributions welcome! Areas for improvement:
- Additional processor types
- Performance optimizations
- More configuration examples
- Integration examples

## Version History

- **1.0.0** (2026-01-10): Initial release
  - 20+ processor types
  - Pipeline architecture
  - Configuration support
  - Comprehensive documentation
