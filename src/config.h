#pragma once

#define FIRMWARE_VERSION "netpins-1.1.3-snapshot"

#define WIFI_SSID ""
#define WIFI_PASS ""

#define STATIC_IP      IPAddress(0, 0, 0, 0) // set non zero to enable static IP
#define GATEWAY        IPAddress(0, 0, 0, 0)
#define SUBNET         IPAddress(0, 0, 0, 0)
#define DNS            IPAddress(0, 0, 0, 0)

#define FACTORY_REST_PIN -1 // -1 to use power cycle factory reset

// debug settings ////////////
#define WAIT_FOR_SERIAL false

// Uncomment line below to fully disable logging, and reduce project size
//#define DISABLE_LOGGING
#define LOG_LEVEL LOG_LEVEL_NOTICE
// #define LOG_LEVEL LOG_LEVEL_TRACE

#define PRINT_EXECUTION_TIME false
