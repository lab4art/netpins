# NetPins

NetPins is an Arduino project that allows you to manage various peripherals like LEDs, RGB strips, and servos. The project has been tested with esp32dev and esp32s2 (lolin_s2_mini) boards. 

## Features

- **DMX (Artnet) Control**: Control your peripherals using the DMX protocol over Artnet.
- **Mapping DMX Channels to Pin Functionality**: Easily map DMX channels to specific pins on your microcontroller to control LEDs, RGB strips, servos, and more.
- **Web UI Configuration**: Configure your device settings through a web interface.
- **Over-the-Air (OTA) Updates**: Update your firmware wirelessly without the need for physical connections.

## Uploading firmware

### Compiling and Uploading from sources

Firmware and filesystem images can be compiled and uploaded using [PlatformIO for VSCode](https://docs.platformio.org/en/latest/integration/ide/vscode.html#ide-vscode).
You also need a [git scm](https://git-scm.com/) installed.

1. **Install PlatformIO for VSCode** following the instructions on the [PlatformIO website](https://docs.platformio.org/en/latest/integration/ide/vscode.html#installation).

1. **Clone this repository** using the following command (needs to be done only the 1st time)
    ```
    git clone https://github.com/lab4art/netpins.git
    ```

1. Change to the project directory and pull the latest changes
    ```
    cd netpins
    git fetch --tags
    ```

1. checkout the latest release, eg. 1.1.2
    ```
    git checkout 1.1.2
    ```

1. **Upload the firmaware** by following the [quick start instructions](https://docs.platformio.org/en/latest/integration/ide/vscode.html#quick-start) for PlatformIO for VSCode. The only difference is that you need to open the existing `netpins` project instead of creating a "New Project" and you don't need to edit any source code.
     
     - Note, some boards needs to be put into upload mode manually (1st time only), eg. by pressing the BOOT button when powered on, in case of `lolin_s2_mini` board you need to hold `0` while pressing the `RST` button.

1. **Upload the filesystem** image for the web based admin UI, by opening the PatformIO left sidebar in VSCode by clicking on the PlatformIO icon (alien) and then click on the `Platform / Upload File System Image"` button. Make sure the right board is selected by looking at the bottom of the window eg. `env:lolin_s2_mini (netpins)`.


## Configuration

1. **Initial Setup**:
   - NetPins will start in AP (WiFi access point) mode.
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

## Factory Reset

To clear all settings and reset the device to factory defaults, a power cycle is required.
After power-on wait between 5 and 10 seconds and press the `RST` button or disconnect the power.
The power cycle needs to be repeated 5 times in a row.
The device will start in AP mode with the default SSID `netpins-<mac-address>`.

Power cycle sequence:
- Power-off, power-on (or press `RST` button)
- Wait 5-10 seconds
- power-off, power-on (or press `RST` button)
- Wait 5-10 seconds
- *Repeat 3 more times*


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
    dimmer: none # add dimmer channel. Options: none, single, per-slice
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
```

### Experimental
```yaml
digital_reads:
  - pin: 4
    read_ms: 100
hum_temps:
  - pin: 4
    read_ms: 1000
touch_sensors:
  - pin: 4
    threshold: 250 # works ok with a wire on a s2_mini pin

waves: # Works only with rgb strip
  - max_fade_time: 10000
    # RGB slice sequential number in order they are defined, ignoring "pin groups" (supports cross pin slices)
    slice_indexes:
      - 0
      - 1
      - 2
      - 3
      - 4

pwm_fades:
  - name: fade-13
    led: 13 # led identified by pin number

# animation_control:
thing_controls:
  - name: fade-13
    sensor:
      pin: 4

```

## Testing

### Direct API calls

    curl -v http://192.168.4.1/sys-info
    curl -v http://192.168.4.1/conf/sys

    curl -v 
      --header "Content-Type: application/json" \
      --request POST \
      --data '{"command": "sys-config-merge", "data": {"wifi_ssid": "SSID","wifi_pass": "***", "hostname": "esp-devel"}}' \
      http://192.168.4.1/system

### Test scenarios

1. WiFi and Mqtt reconnect
1. Hearthbeat broadcast
1. Admin UI loading and saving preferences (yaml, dmx)
1. Firmware and Filesystem OTA update
1. Factory reset
1. Iddle poweroff
1. Simple led control over DMX
1. RGB(W) stripe with slices control over DMX
   1. w/ slices and without slices
   1. w/ dimmer and without
1. servo over dmx


## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

