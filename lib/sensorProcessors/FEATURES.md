# Feature Matrix - Sensor Processor Pipeline

## Supported Operations

| Feature | Supported | Processor Type | Example |
|---------|-----------|----------------|---------|
| **Range Mapping** | ✅ | RangeMapping | -40-100°C → 0-255 |
| **Scaling** | ✅ | Scale | value × 2 + 10 |
| **Clamping** | ✅ | Clamp | Keep in 0-255 |
| **Inversion** | ✅ | Invert | 0→255, 255→0 |
| **Rounding** | ✅ | Round | 25.7 → 26 |
| **Debouncing** | ✅ | Debounce | Stable for 100ms |
| **Persistence** | ✅ | Persistence | Active for 10s |
| **Timeout** | ✅ | Timeout | Reset after 60s |
| **Rate Limiting** | ✅ | RateLimit | Update every 500ms |
| **Delay** | ✅ | Delay | Delay by 1s |
| **Threshold** | ✅ | Threshold | >100 → 255, else 0 |
| **Hysteresis** | ✅ | Hysteresis | On at 120, off at 80 |
| **Time Threshold** | ✅ | TimeThreshold | >30°C for 60s |
| **Range Check** | ✅ | RangeThreshold | 20-30 → active |
| **Band Pass** | ✅ | BandPass | Only 50-200 |
| **Dead Zone** | ✅ | DeadZone | Ignore ±10 |
| **Moving Average** | ✅ | MovingAverage | Average last 10 |
| **Exponential MA** | ✅ | EMA | Weighted average |
| **Median Filter** | ✅ | MedianFilter | Remove spikes |
| **Weighted Average** | ✅ | WeightedAverage | Custom weights |
| **Low Pass Filter** | ✅ | LowPass | RC filter |
| **Kalman Filter** | ✅ | Kalman | Optimal estimation |

## Real-World Scenarios

| Scenario | Solution | Processors Used |
|----------|----------|-----------------|
| **Noisy temperature sensor** | Filter spikes, smooth, map | Median + EMA + RangeMapping |
| **Bouncy switch** | Debounce | Debounce |
| **Motion sensor (PIR)** | Persist before trigger, timeout | Persistence + Timeout |
| **Flickering light** | Hysteresis | Hysteresis |
| **Analog joystick** | Dead zone, map | DeadZone + RangeMapping |
| **Distance sensor** | Kalman filter, invert | Kalman + RangeMapping + Invert |
| **Multi-sensor averaging** | Average readings | MovingAverage (per sensor) |
| **Alarm condition** | Time-based threshold | TimeThreshold |
| **Variable speed control** | Smooth + clamp | EMA + Clamp |
| **Touch sensor** | Debounce + scale | Debounce + Scale |

### CPU Usage (approximate)

| Processor Type | Operations | CPU Time | Notes |
|----------------|------------|----------|-------|
| RangeMapping | ~10 ops | Fast | Simple math |
| Scale | ~3 ops | Very fast | Multiply + add |
| Threshold | ~2 ops | Very fast | Compare |
| Debounce | ~5 ops | Fast | Time check |
| MovingAverage | N ops | Fast | N = window size |
| MedianFilter | N log N | Medium | Requires sorting |
| EMA | ~4 ops | Fast | Weighted average |
| Kalman | ~20 ops | Medium | Matrix operations |
