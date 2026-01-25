# Configuration Quick Reference

Quick reference for pipeline configuration syntax.

## Basic Structure

```yaml
sensor_pipelines:
  - sensor: <sensor_name>          # Must match hardware config
    pipeline:                      # Processor chain
      - type: <processor_type>
        <param1>: <value1>
        <param2>: <value2>
      - type: <processor_type>
        # ... more parameters ...
    dmx: <channel>@<universe>      # DMX output (optional, 1-based channel)
    mqtt: <topic_name>             # MQTT topic (optional, relative to base)
```

## Processor Types & Parameters

### Range Mapping

#### range_mapping
Map input range to output range
```yaml
- type: range_mapping
  input_min: -40.0
  input_max: 100.0
  output_min: 0.0
  output_max: 255.0
  clamp: true              # Optional, default: true
```

#### scale
Multiply by factor and add offset
```yaml
- type: scale
  factor: 2.5
  offset: -10.0
```

#### clamp
Limit value to range
```yaml
- type: clamp
  min: 0.0
  max: 255.0
```

#### invert
Invert value within range
```yaml
- type: invert
  min: 0.0
  max: 255.0
```

#### round
Round to precision
```yaml
- type: round
  precision: 1.0           # Round to whole numbers
```

### Time-Based

#### debounce
Require stable value for duration
```yaml
- type: debounce
  duration_ms: 500         # Or: time_ms
```

#### persistence
Value must persist for duration
```yaml
- type: persistence
  duration_ms: 10000
  target: 1.0              # Target value
  default: 0.0             # Default output
```

#### timeout
Reset after inactivity
```yaml
- type: timeout
  duration_ms: 60000
  timeout_value: 0.0
  threshold: 0.0           # Optional
```

#### rate_limit
Limit update frequency
```yaml
- type: rate_limit
  interval_ms: 100
```

#### delay
Delay output by duration
```yaml
- type: delay
  delay_ms: 1000
```

#### motion_state
Motion detection with state tracking (replaces old strobe processor)
```yaml
- type: motion_state
  persist_ms: 30000    # Time before entering persisted state (default: 30000)
  no_motion_ms: 30000      # Timeout for no motion (default: 30000)
  threshold: 0.0           # Motion threshold (default: 0.0)
```
**Use case:** Motion detection with different output values for different states. Use with DMX transformer for strobe effects.
**Output:** `low` when no motion, `normal` during motion, `persisted` after motion persists ≥ persistence_ms

### Threshold

#### threshold
Binary threshold
```yaml
- type: threshold
  threshold: 128.0
  high: 255.0              # Output when above threshold
  low: 0.0                 # Output when below threshold
  above: true              # Optional, default: true
```

#### hysteresis
Two-level threshold (prevents oscillation)
```yaml
- type: hysteresis
  upper: 700.0             # Switch to high when above
  lower: 300.0             # Switch to low when below
  high: 255.0
  low: 0.0
```

#### time_threshold
Threshold must persist for duration
```yaml
- type: time_threshold
  threshold: 70.0
  duration_ms: 60000
  high: 255.0
  low: 0.0
```

#### range_threshold
Check if value in range
```yaml
- type: range_threshold
  lower: 20.0
  upper: 80.0
  in_range: 255.0
  out_range: 0.0
```

#### band_pass
Pass values within band
```yaml
- type: band_pass
  lower: 20.0
  upper: 100.0
  in_band: 255.0
  out_band: 0.0
```

#### dead_zone
Ignore small changes around center
```yaml
- type: dead_zone
  center: 127.5
  radius: 5.0
```

### Smoothing

#### moving_average
Simple moving average
```yaml
- type: moving_average
  window_size: 5           # Or: size
```

#### ema
Exponential moving average
```yaml
- type: ema
  alpha: 0.2               # Or: smoothing (0-1, lower = smoother)
```

#### median
Median filter (removes spikes)
```yaml
- type: median
  window_size: 7           # Or: size (odd number recommended)
```

#### weighted_average
Weighted moving average
```yaml
- type: weighted_average
  window_size: 5           # Or: size
```

#### low_pass
Low-pass filter
```yaml
- type: low_pass
  alpha: 0.3               # Or: coefficient (0-1, lower = smoother)
```

#### kalman
Kalman filter (optimal estimation)
```yaml
- type: kalman
  process_noise: 0.01
  measurement_noise: 0.1
```

## Complete Examples

### Temperature Sensor
```yaml
- sensor: temperature_1
  pipeline:
    - { type: range_mapping, input_min: -40.0, input_max: 100.0, output_min: 0.0, output_max: 255.0 }
    - { type: moving_average, window_size: 5 }
    - { type: debounce, duration_ms: 500 }
  dmx: 10@1
  mqtt: temperature
```

### Motion Sensor
```yaml
- sensor: motion_sensor_1
  pipeline:
    - { type: debounce, duration_ms: 500 }
    - { type: persistence, duration_ms: 10000, target: 1.0, default: 0.0 }
    - { type: range_mapping, input_min: 0.0, input_max: 1.0, output_min: 0.0, output_max: 255.0 }
  dmx: 15@1
  mqtt: motion
```

### Light Sensor (Auto-lights)
```yaml
- sensor: light_sensor_1
  pipeline:
    - { type: ema, alpha: 0.1 }
    - { type: hysteresis, lower: 200.0, upper: 400.0, high: 255.0, low: 0.0 }
    - { type: debounce, duration_ms: 5000 }
  dmx: 20@1
```

## Parameter Aliases

Some parameters have aliases for convenience:

| Standard | Aliases |
|----------|---------|

| `duration_ms` | `time_ms` |
| `window_size` | `size` |
| `alpha` | `smoothing`, `coefficient` |

## Common Sensor Ranges

Typical raw sensor output ranges (use in range_mapping processor):

| Sensor Type | Raw Range |
|-------------|-------|
| Temperature (DHT22) | -40 to 100 |
| Humidity | 0 to 100 |
| Analog (10-bit ADC) | 0 to 1023 |
| Digital (binary) | 0 to 1 |
| Distance (ultrasonic) | 2 to 400 |
| Light (photoresistor) | 0 to 1023 |
| Touch | 0 to 1 |

## Output Configuration

### DMX Format (optional)
Format: `channel@universe`
- Channel: 1-512 (1-based)
- Universe: 0-65535
- Example: `10@1` = channel 10, universe 1

### MQTT Topic (optional)
Format: `mqtt: <topic_name>`
- Topic is relative to configured base path
- Example: `mqtt: temperature` → publishes to `<base_path>temperature`
- Publishes processed DMX value (0-255) as string
- Omit field to disable MQTT publishing

## Processing Order

Recommended processor order:
1. **Noise filtering** (median, moving_average)
2. **Range mapping** (range_mapping, scale)
3. **Threshold/logic** (threshold, hysteresis)
4. **Time-based** (debounce, persistence, timeout)
5. **Final smoothing** (ema, low_pass)

Example:
```yaml
pipeline:
  - { type: median, window_size: 5 }              # 1. Remove spikes
  - { type: range_mapping, ... }                  # 2. Map to 0-255
  - { type: hysteresis, ... }                     # 3. Apply logic
  - { type: debounce, duration_ms: 1000 }        # 4. Stabilize
```

## Tips

### Keep Pipelines Simple
- Use 2-5 processors per pipeline
- More processors = more processing time
- Test incrementally

### Sensor Naming
- Use descriptive names: `temp_living_room` not `sensor1`
- Match names exactly between hardware and pipeline config
- Use underscores, not spaces

### Range Mapping
- Use range_mapping processor to map raw sensor values to 0-255
- Measure actual min/max from your sensors for input_min/input_max
- Set output_min: 0.0, output_max: 255.0 for DMX

### Testing
- Start with simple pipeline (just range_mapping)
- Add processors one at a time
- Test each addition
- Use preview mode: `processSensorValuePreview()`

### Performance
- Fast sensors (>10Hz): Keep pipelines simple
- Slow sensors (<1Hz): Can use complex pipelines
- Monitor processing time if needed

## Common Patterns

### Pattern: Noisy Sensor
```yaml
pipeline:
  - { type: median, window_size: 5 }
  - { type: moving_average, window_size: 10 }
  - { type: range_mapping, ... }
```

### Pattern: Event Detection
```yaml
pipeline:
  - { type: debounce, duration_ms: 200 }
  - { type: persistence, duration_ms: 5000, target: 1.0, default: 0.0 }
  - { type: range_mapping, ... }
```

### Pattern: Threshold with Delay
```yaml
pipeline:
  - { type: moving_average, window_size: 10 }
  - { type: time_threshold, threshold: 70.0, duration_ms: 30000, high: 255.0, low: 0.0 }
```

### Pattern: Auto-Control
```yaml
pipeline:
  - { type: ema, alpha: 0.1 }
  - { type: hysteresis, lower: 200.0, upper: 400.0, high: 255.0, low: 0.0 }
  - { type: debounce, duration_ms: 5000 }
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Pipeline not working | Check sensor name matches exactly |
| Wrong DMX values | Verify range_mapping input_min/input_max matches actual sensor |
| Flickering output | Add debounce or increase smoothing |
| Slow response | Reduce window_size or smoothing alpha |
| Stuck at min/max | Check clamp settings in range_mapping |
| Not triggering | Check threshold values, add logging |
| MQTT not publishing | Ensure mqtt field is set and MQTT is configured |

## See Also

- [CONFIG_INTEGRATION_GUIDE.md](CONFIG_INTEGRATION_GUIDE.md) - Complete setup guide
- [pipeline_config_examples.yaml](examples/pipeline_config_examples.yaml) - 10 complete examples
- [quick_start.yaml](examples/quick_start.yaml) - Minimal starter config
