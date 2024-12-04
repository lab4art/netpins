# Sample configs

    wifi_ssid: ssid
    wifi_pass: ***
    hostname: tower
    hb_int: 5000
    udp_port: 5824
    lights_test: true
    max_idle: 120 # power of when no activity in minutes
    leds:
      - 13
      - 14
    rgbw_strips: []
    rgb_strips:
    - pin: 13
        size: 60
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
    waves:
    - max_fade_time: 10000
        # slice sequential number in order they are defined, ignoring "pin groups"
        slice_indexes:
        - 0
        - 1
        - 2
        - 3
        - 4
