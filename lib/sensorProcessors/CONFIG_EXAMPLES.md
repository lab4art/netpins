# Sensor Processor Pipeline - Configuration Examples

This document provides comprehensive examples for configuring sensor processing pipelines.

## Basic Concepts

The sensor processing pipeline allows you to chain multiple processors together to transform raw sensor data into DMX output values (0-255). Each sensor can have its own pipeline with multiple processors that execute in sequence.

### Pipeline Structure

```yaml
sensor_pipelines:
  - name: "temperature_pipeline"
    sensor: "temperature_sensor"
    dmx: "1@0"  # Universe 1, Channel 0
    enabled: true
    processors:
      - type: "processor_name"
        param1: value1
        param2: value2
```

## Example 1: Temperature Sensor with Averaging and Range Mapping

**Use Case**: Temperature sensor reading from -40 to 100°C, averaging readings, and mapping to DMX 0-255

```yaml
sensor_pipelines:
  - name: "temperature_control"
    sensor: "dht_temperature"
    dmx: "1@0"
    processors:
      # First, smooth the readings with moving average
      - type: "moving_average"
        window_size: 10
      
      # Then map from temperature range to DMX range
      - type: "range_mapping"
        from_min: -40.0
        from_max: 100.0
        to_min: 0.0
        to_max: 255.0
        clamp: true
```

## Example 2: Motion Sensor with Persistence and Timeout

**Use Case**: Motion sensor (0-1) that sets DMX to 255 if motion persists for 10s, returns to 0 after 60s of no motion

```yaml
sensor_pipelines:
  - name: "motion_detection"
    sensor: "pir_sensor"
    dmx: "1@1"
    processors:
      # Convert 0/1 to 0/255
      - type: "scale"
        factor: 255.0
        offset: 0.0
      
      # Motion must persist for 10 seconds
      - type: "persistence"
        time_ms: 10000
        target: 255.0
        default: 0.0
      
      # Reset to 0 after 60 seconds of no motion
      - type: "timeout"
        time_ms: 60000
        timeout_value: 0.0
        threshold: 0.0
```

## Example 3: Temperature with Time-Based Threshold

**Use Case**: If temperature > 30°C for 60 seconds, set DMX to 255, otherwise map normally

```yaml
sensor_pipelines:
  - name: "temperature_alarm"
    sensor: "temperature_sensor"
    dmx: "1@2"
    processors:
      # Smooth readings
      - type: "ema"
        alpha: 0.2
      
      # Check if temp > 30°C for 60 seconds
      - type: "time_threshold"
        threshold: 30.0
        time_ms: 60000
        high: 255.0
        low: 0.0
        above: true
```

## Example 4: Temperature with Averaging and Conditional Mapping

**Use Case**: Average temperature readings, then map to DMX range, but only if stable

```yaml
sensor_pipelines:
  - name: "stable_temperature"
    sensor: "temperature_sensor"
    dmx: "1@3"
    processors:
      # Remove noise with median filter
      - type: "median"
        window_size: 7
      
      # Smooth with exponential moving average
      - type: "ema"
        alpha: 0.3
      
      # Debounce to ensure stability
      - type: "debounce"
        time_ms: 2000
      
      # Map to DMX range
      - type: "range_mapping"
        from_min: -40.0
        from_max: 100.0
        to_min: 0.0
        to_max: 255.0
```

## Example 5: Light Sensor with Hysteresis

**Use Case**: Light sensor that triggers at 500 lux, turns off at 400 lux (prevents flickering)

```yaml
sensor_pipelines:
  - name: "light_control"
    sensor: "light_sensor"
    dmx: "1@4"
    processors:
      # Smooth readings
      - type: "moving_average"
        window_size: 5
      
      # Apply hysteresis
      - type: "hysteresis"
        upper: 500.0
        lower: 400.0
        high: 255.0
        low: 0.0
```

## Example 6: Touch Sensor with Debounce

**Use Case**: Touch sensor that requires stable contact for 100ms

```yaml
sensor_pipelines:
  - name: "touch_button"
    sensor: "touch_sensor"
    dmx: "1@5"
    processors:
      # Scale 0/1 to 0/255
      - type: "scale"
        factor: 255.0
      
      # Debounce to prevent false triggers
      - type: "debounce"
        time_ms: 100
```

## Example 7: Distance Sensor with Inverted Mapping

**Use Case**: Distance sensor (0-400cm) where closer = brighter

```yaml
sensor_pipelines:
  - name: "proximity_light"
    sensor: "distance_sensor"
    dmx: "1@6"
    processors:
      # Smooth readings
      - type: "kalman"
        process_noise: 0.01
        measurement_noise: 0.5
      
      # Map distance to DMX
      - type: "range_mapping"
        from_min: 0.0
        from_max: 400.0
        to_min: 0.0
        to_max: 255.0
      
      # Invert so closer = brighter
      - type: "invert"
        min: 0.0
        max: 255.0
```

## Example 8: Analog Sensor with Dead Zone

**Use Case**: Joystick or potentiometer with noise around center

```yaml
sensor_pipelines:
  - name: "joystick_control"
    sensor: "analog_joystick"
    dmx: "1@7"
    processors:
      # Map analog reading to -128 to +127
      - type: "range_mapping"
        from_min: 0.0
        from_max: 1023.0
        to_min: -128.0
        to_max: 127.0
      
      # Ignore small movements around center
      - type: "dead_zone"
        center: 0.0
        radius: 10.0
      
      # Map back to 0-255
      - type: "range_mapping"
        from_min: -128.0
        from_max: 127.0
        to_min: 0.0
        to_max: 255.0
```

## Example 9: Multiple Sensors for Same DMX Channel

**Use Case**: Average readings from multiple temperature sensors

```yaml
sensor_pipelines:
  - name: "temp_sensor_1"
    sensor: "temp1"
    dmx: "1@8"
    processors:
      - type: "ema"
        alpha: 0.3
      - type: "range_mapping"
        from_min: -40.0
        from_max: 100.0
        to_min: 0.0
        to_max: 255.0
  
  - name: "temp_sensor_2"
    sensor: "temp2"
    dmx: "1@8"  # Same channel - values will be averaged
    processors:
      - type: "ema"
        alpha: 0.3
      - type: "range_mapping"
        from_min: -40.0
        from_max: 100.0
        to_min: 0.0
        to_max: 255.0
```

## Example 10: Complex Multi-Stage Processing

**Use Case**: Environment monitoring with multiple conditions

```yaml
sensor_pipelines:
  - name: "environment_control"
    sensor: "environment_sensor"
    dmx: "1@9"
    processors:
      # Stage 1: Noise reduction
      - type: "median"
        window_size: 5
      
      # Stage 2: Smoothing
      - type: "ema"
        alpha: 0.2
      
      # Stage 3: Remove dead zone
      - type: "dead_zone"
        center: 0.0
        radius: 2.0
      
      # Stage 4: Check if value in acceptable range
      - type: "band_pass"
        min: 10.0
        max: 90.0
        default: 0.0
      
      # Stage 5: Map to output range
      - type: "range_mapping"
        from_min: 10.0
        from_max: 90.0
        to_min: 0.0
        to_max: 255.0
      
      # Stage 6: Rate limit updates
      - type: "rate_limit"
        interval_ms: 500
```

## Processor Order Matters

Arrange processors in logical order:

```yaml
# Recommended order:
pipeline:
  # 1. Noise filtering
  - type: moving_average
    window_size: 5
  
  # 2. Range mapping
  - type: range_mapping
    input_min: 0.0
    input_max: 100.0
    output_min: 0.0
    output_max: 255.0
  
  # 3. Threshold/logic
  - type: hysteresis
    lower: 100.0
    upper: 150.0
    high: 255.0
    low: 0.0
  
  # 4. Stabilization
  - type: debounce
    duration_ms: 1000
```

## Processor Types Reference

### Range Mapping Processors
- **range_mapping** / **map**: Map from one range to another
- **scale**: Multiply by factor and add offset
- **clamp**: Limit values to min/max range
- **invert**: Invert values within range
- **round**: Round to specified precision

### Time-Based Processors
- **debounce**: Require stable value for duration
- **persistence**: Value must persist for duration
- **timeout**: Reset after inactivity period
- **rate_limit**: Limit update frequency
- **delay**: Delay output by duration

### Threshold Processors
- **threshold**: Simple above/below threshold
- **hysteresis**: Threshold with upper/lower bounds
- **time_threshold**: Threshold must persist for duration
- **range_threshold**: Trigger when in range
- **band_pass**: Only pass values in range
- **dead_zone**: Ignore values near center

### Smoothing Processors
- **moving_average** / **avg**: Simple moving average
- **ema** / **exponential_average**: Exponential moving average
- **median**: Median filter
- **weighted_average**: Weighted moving average
- **low_pass**: Low-pass RC filter
- **kalman**: Kalman filter

## Tips and Best Practices

1. **Order Matters**: Processors execute in sequence, so order is important
   - Usually: noise reduction → smoothing → mapping → thresholding

2. **Start Simple**: Begin with 1-2 processors and add more as needed

3. **Test Incrementally**: Enable processors one at a time to understand their effect

4. **Watch for Over-Smoothing**: Too much averaging/smoothing can make the system unresponsive

5. **Use Appropriate Window Sizes**: Larger windows = smoother but slower response

6. **Combine Filters**: Median (remove spikes) + EMA (smooth) works well for noisy sensors

7. **Monitor Performance**: Many processors = more CPU usage

8. **Use Time-Based Wisely**: Time-based processors maintain state and use memory
