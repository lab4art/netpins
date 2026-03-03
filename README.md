# NetPins

NetPins is an Arduino project that allows you to manage various peripherals like LEDs, RGB strips, and servos. The project has been tested with esp32dev and esp32s2 (lolin_s2_mini) boards. 

## Features

- **DMX Control**: Control your peripherals using the DMX protocol via:
  - **ArtNet** (DMX over network/WiFi)
  - **DMX Input** (Direct DMX512 via RS-485/MAX485) - Alternative to ArtNet
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

1. checkout the latest release, eg. 2.0.0
    ```
    git checkout 2.0.0
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

## DMX Output
Transmit DMX data (from sensor mappings) via RS-485 to control external DMX devices:

```yaml
dmx_output:
  enabled: true
  uart_port: 2        # UART port number (0, 1, or 2). 0 is used by default for serial output
  tx_pin: 17          # GPIO pin for DMX transmit
  rx_pin: 18          # GPIO pin for DMX receive (can be -1 if not used)
  enable_pin: 16      # GPIO pin for RS485 enable (DE/RE on MAX485)
  universe: 0         # Source (ArtNet) DMX universe to output to DMX
```

**Hardware Requirements:**
- MAX485 or similar RS-485 transceiver
- Connections:
  - TX pin → DI (Driver Input) on MAX485
  - RX pin → RO (Receiver Output) on MAX485 (optional)
  - Enable pin → DE/RE pins on MAX485
  - A and B terminals → DMX+ and DMX- on XLR cable

## DMX Input (Alternative to ArtNet)
Receive DMX data via RS-485 from an external DMX source (like a lighting console). This provides an alternative to ArtNet for receiving DMX control data:

```yaml
dmx_input:
  enabled: true
  uart_port: 2        # UART port number (0, 1, or 2). Use different port than output
  tx_pin: 17          # GPIO pin for DMX transmit
  rx_pin: 18          # GPIO pin for DMX receive
  enable_pin: 4       # GPIO pin for RS485 enable (DE/RE on MAX485)
  universe: 0         # Which universe this input represents
```

**Hardware Requirements:**
- MAX485 or similar RS-485 transceiver
- Connections:
  - TX pin → DI (Driver Input) on MAX485 (optional, can be -1)
  - RX pin → RO (Receiver Output) on MAX485
  - Enable pin → DE/RE pins on MAX485
  - A and B terminals → DMX+ and DMX- from DMX source

**Notes:**
- DMX input can be used **instead of** or **alongside** ArtNet
- To disable ArtNet and use only DMX input, set `disable_artnet: true`
- Both ArtNet and DMX input can feed different universes simultaneously
- Universe mapping follows the same rules as ArtNet input


## Sample Configuration

```yaml
dmx_offset: 0
wifi_ssid: your-ssid
wifi_pass: your-secret-password
hostname: tower
hb_int: 5000 # 0 = disabled
udp_port: 5824
lights_test: true # power on all at boot for 2 seconds
max_idle: 120 # power off microcontroller when no network activity for N minutes
reboot_after_wifi_failed: 15 # reboot after 15 failed wifi connections, 0 means no reboot
disable_wifi_power_save: false # disable WiFi power save to prevent led flicering on "poor" power connection
disable_wifi_reconnect: false # try to connect once only (repeat in case a successful connection is lost)
disable_artnet: false
log_level: -1 # -1=no-affects-from-this-settging, 0=no logs, 1=error, 2=warning, 3=info, 4=debug, 5=trace
pwms:
  - pin: 13
    name: pwm-13
    dmx: 1@0 # channel@universe
  - pin: 14
    name: pwm-14
    dmx: 2@0
rgbw_strips: []
rgb_strips:
  - pin: 13
    name: rgb-strip-1
    dmx: 1@0
    size: 20
    dimmer: none # add dimmer channel. Options: none, single, per-slice
    slices:
      - 0
      - 5
      - 10
      - 15
servos:
  - pin: 4
    max_angle: 180
    min_pulse_width: 500
    max_pulse_width: 2500
  - pin: 13
    max_angle: 90
dmx_input:
  enabled: true
  uart_port: 1
  tx_pin: 13
  rx_pin: 14
  enable_pin: 15
  universe: 0 # universe to set from DMX input
dmx_output:
  enabled: true
  uart_port: 2
  tx_pin: 17
  rx_pin: 18
  enable_pin: 16
  universe: 0 # universe to output to DMX
```

### Experimental
DmxOut sample configuration:
```yaml
dmxOutput:
  enabled: true
  uart_port: 1        # UART port number (0, 1, or 2). 0 is used by default for serial output
  tx_pin: 17          # GPIO pin for DMX transmit
  rx_pin: 18          # GPIO pin for DMX receive (can be -1 if not used)
  enable_pin: 16      # GPIO pin for RS485 enable (can be -1 if not needed)
  universe: 0         # Source (Artnet) DMX universe to output to DMX
```

motion_state processor with dmx sequences example configuration:
```yaml
dmx_offset: 0
wifi_ssid: 
wifi_pass: 
hostname: 
hb_int: 5000
udp_port: 5824
lights_test: true
max_idle: 0
reboot_after_wifi_failed: 15
disable_wifi_power_save: false
disable_wifi_reconnect: false
disable_artnet: false
log_level: -1
rgbw_strips:
  - pin: 13
    name: rgb-strip-1
    size: 2
    dimmer: none
    slices:
      - 0
    dmx: 1@0
digital_reads:
  - pin: 5
    name: motion_sensor
    read_ms: 100
sensor_pipelines:
  - sensor: motion_sensor
    pipeline:
      - type: motion_state
        no_motion_ms: 5000
        persist_ms: 5000
        threshold: 0.5
    dmx: 1@100

dmx_processors:
  - type: sequence
    name: strobe_effect
    sequence:
      - channels:
          1@0: 0
          2@0: 50
          3@0: 50
          4@0: 0
        fade_in_ms: 0
        hold_ms: 20
      - channels:
          1@0: 0
          2@0: 0
          3@0: 0
          4@0: 0
        fade_in_ms: 0
        hold_ms: 50
  - type: sequence
    name: steady_cyan
    loop: false
    sequence:
      - channels:
          1@0: 0
          2@0: 50
          3@0: 50
          4@0: 0
        fade_in_ms: 0
        hold_ms: 0
  - type: sequence
    name: steady_cyan_fade
    control_channel: 1@100 # ignored when used as included sequence
    min_value: 1
    max_value: 1
    loop: false
    sequence:
      - channels:
          1@0: 0
          2@0: 50
          3@0: 50
          4@0: 0
        fade_in_ms: 3000
        hold_ms: 0
  - type: sequence
    name: strobe_runner
    control_channel: 1@100
    min_value: 2
    max_value: 2
    loop: true
    sequence:
      - include: steady_cyan
        hold_ms: 10000 # if > 0 cancel the included sequence after hold_ms
      - include: strobe_effect
        hold_ms: 5000
  - type: sequence
    name: fade_to_black
    control_channel: 1@100
    min_value: 0
    max_value: 0
    loop: false
    sequence:
      - channels:
          1@0: 0
          2@0: 0
          3@0: 0
          4@0: 0
        fade_in_ms: 5000
        hold_ms: 0
```

Autostart always run
```yaml
dmx_processors:
  - type: sequence
    name: sparks
    min_value: -1 # always enabled, does not require a control channel
    max_value: 255
    loop: true
    sequence:
      - channels:
          1@0: 0
          2@0: 50
          3@0: 50
          4@0: 0
        fade_in_ms: 0
        hold_ms: 50
      - channels:
          1@0: 0
          2@0: 0
          3@0: 0
          4@0: 0
        fade_in_ms: 0
        hold_ms: 200
```


```yaml
# control tail aniimation plugin with dmx sequences
rgb_strips:
  - pin: 13
    name: rgb-1
    size: 20
    dimmer: none
    slices:
      - 0
    dmx: 1@10
dmx_processors:
  - type: sequence
    name: on_off_tail_animation
    min_value: -1
    max_value: 255
    loop: true
    sequence:
      - channels:
          1@0: 50
          2@0: 0
          3@0: 0
          4@0: 0
          5@0: 0
          6@0: 0
          7@0: 255
          8@0: 10
          9@0: 0
          10@0: 2
        fade_in_ms: 0
        hold_ms: 2000
      - channels:
          1@0: 10
          2@0: 0
          3@0: 0
          4@0: 0
          5@0: 0
          6@0: 0
          7@0: 0
          8@0: 10
          9@0: 0
          10@0: 2
        fade_in_ms: 0
        hold_ms: 2000
plugins:
  - name: tail-animation-1
    type: tail-animation
    config:
      rgb_strip_name: rgb-1
      dmx: 1@0
      max_duration: 10000
      direction: right


# control tail animation plugin with dmx sequences using includes
dmx_processors:
  - type: sequence
    name: ta-on
    sequence:
      - channels:
          1@0: 50
          2@0: 0
          3@0: 0
          4@0: 0
          5@0: 0
          6@0: 0
          7@0: 255
          8@0: 10
          9@0: 0
          10@0: 2
        fade_in_ms: 0
  - type: sequence
    name: ta-off
    sequence:
      - channels:
          1@0: 50
          2@0: 0
          3@0: 0
          4@0: 0
          5@0: 0
          6@0: 0
          7@0: 0
          8@0: 10
          9@0: 0
          10@0: 2
        fade_in_ms: 0
  - type: sequence
    name: ta-runner
    loop: true
    initial_state_on: true
    sequence:
      - include: ta-on
        hold_ms: 1000
      - include: ta-off
        hold_ms: 3000
      - include: ta-on
        hold_ms: 3000
      - include: ta-off
        hold_ms: 8000


```

```yaml
sensor_publish: # enable/disable sensor publishing over: mqtt, artnet, local
  - mqtt
  - artnet
  - local

sensor_mappings:
  - sensor: dr-4
    dmx: 4@1 # controll blue (assuming rgb strip is mapped to 2@1) collor of the rgb strip
    value_range: # map read value range to dmx value 0-255
      from: 0
      to: 1023

digital_reads:
  - pin: 4
    name: dr-4
    read_ms: 100
hum_temps:
  - pin: 4
    read_ms: 1000
touch_sensors:
  - pin: 4
    threshold: 250 # works ok with a wire on a s2_mini pin

pwm_fades:
  - name: fade-13
    pwm: pwm-13 # identified by name

```
Tail animation example configuration:
```yaml
rgb_strips:
  - pin: 13
    name: rgb-strip-1
    size: 30
    dimmer: single
    slices:
      - 0
    dmx: 1@0
plugins:
  - name: tail-animation-1
    type: tail-animation
    config:
      rgb_strip_name: rgb-strip-1
      dmx: 1@0
      max_duration: 10000
      direction: left
```
Wave effect example configuration:
```yaml
rgb_strips:
  - pin: 13
    name: rgb-strip-1
    size: 12
    dimmer: single
    slices:
      - 0
      - 3
      - 6
      - 9
    dmx: 1@0
plugins:
  - name: wave-effect-1
    type: wave-effect
    config:
      rgb_strip_name: rgb-strip-1
      dmx: 1@0
      max_fade_time: 5000
      dimmable: true
```
PWM fade
```yaml
pwms:
  - pin: 13
    name: pwm-13
    dmx: 1@0 # channel@universe
plugins:
  - name: pwm-fade-1
    type: pwm-fade
    config:
      pwm_name: pwm-13
      dmx: 1@0
      max_fade_duration: 5000
```

Analog read sensor example configuration:
```yaml
pwms:
  - pin: 5
    name: pwm-5
    dmx: 1@0
analog_reads:
  - pin: 3
    name: analog_read
    read_ms: 10
sensor_mappings:
  - sensor: analog_read
    dmx: 1@0
    value_range:
      from: 0
      to: 65536
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

