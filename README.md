# NetPins

NetPins is an Arduino project that allows you to manage various peripherals like LEDs, RGB strips, and servos.

## Features

- **DMX (Artnet) Control**: Control your peripherals using the DMX protocol over Artnet.
- **Mapping DMX Channels to Pin Functionality**: Easily map DMX channels to specific pins on your microcontroller to control LEDs, RGB strips, servos, and more.
- **Web UI Configuration**: Configure your device settings through a web interface.
- **Over-the-Air (OTA) Updates**: Update your firmware wirelessly without the need for physical connections.



## Getting Started

### Requirements

- ESP32 or compatible microcontroller
- [PlatformIO](https://platformio.org/)

### Installation

Recommended option is to use [PlatformIO for VSCode](https://docs.platformio.org/en/latest/integration/ide/vscode.html#ide-vscode).

1. **Compile and Upload**:
   - Compile the project and upload it to your microcontroller.

2. **Create and Upload Filesystem Image**:
   - Create and upload the filesystem image for the Web UI.

### Configuration

1. **Initial Setup**:
   - If you did not put WiFi credentials in the `src/config.h`, NetPins will start in AP (WiFi access point) mode.
   - Connect with your PC (or phone) to the WiFi network named `netpins-<mac-address>` and visit `http://192.168.4.1/` with your web browser to access the Admin console.
   - Set up your WiFi credentials.
   - Connect your PC back to your WiFi network.

2. **Find Device IP**:
   - Once WiFi is configured, go to your router console and find the IP of the newly connected device.
   - You can also find the IP by listening to the broadcast messages NetPins is sending (default = enabled). On Linux, you can use `sudo tcpdump -A udp port 5824` to listen to broadcast messages.

3. **Access Admin Console**:
   - Open the Admin console by visiting `http://<ip-address>` with your browser.
   - Once connected to the Admin console, you can configure your microcontroller (see the example configuration below).
   - It's recommended to set a meaningful hostname for each of your microcontrollers.

## DMX Channels

- Configure the first DMX channel and the universe in the Admin console.
- Next DMX channels are mapped without gaps depending on how many channels the function takes:
  - LEDs: 1 channel per pin
  - RGBW strips: 4 channels per slice (5 if dimmer is enabled)
  - RGB strips: 3 channels per slice (4 if dimmer is enabled)
  - Servos: 1 channel per pin
  - Waves: 7 channels per wave (2 x RGB + fade) (8 if dimmer is enabled)

## Sample Configuration

```yaml
wifi_ssid: your-ssid
wifi_pass: your-secret-password
hostname: tower
hb_int: 5000 # 0 = disabled
udp_port: 5824
lights_test: true # power on all at boot for 2 seconds
max_idle: 120 # power off microcontroller when no network activity for N minutes
reboot_after_wifi_failed: 15 # reboot after 15 failed wifi connections, 0 means no reboot
disable_wifi_power_save: false # disable WiFi power save to prevent led flicering on "poor" power connection
leds:
  - 13
  - 14
rgbw_strips: []
rgb_strips:
  - pin: 13
    size: 60
    dimmer: none # add dimmer channel. Options: none, single, per-stripe
    slices:
      - 0
      - 15
      - 33
      - 49
      - 57
servos:
  - pin: 4
    max_angle: 180
    min_pulse_width: 500
    max_pulse_width: 2500
  - pin: 13
    max_angle: 90
waves: # *deprecated* Works only with rgb strip
  - max_fade_time: 10000
    # RGB slice sequential number in order they are defined, ignoring "pin groups"
    slice_indexes:
      - 0
      - 1
      - 2
      - 3
      - 4
```

### Experimental
```yaml
hum_temps:
  - pin: 4
    read_ms: 1000
touch_sensors:
  - pin: 4
    threshold: 250 # work ok with a wire on a s2_mini pin
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

