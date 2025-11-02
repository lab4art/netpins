#pragma once

#define FIRMWARE_VERSION "netpins-2.0.0-snapshot"

#define WIFI_SSID ""
#define WIFI_PASS ""

#define STATIC_IP      IPAddress(0, 0, 0, 0) // set non zero to enable static IP
#define GATEWAY        IPAddress(0, 0, 0, 0)
#define SUBNET         IPAddress(0, 0, 0, 0)
#define DNS            IPAddress(0, 0, 0, 0)

#define FACTORY_REST_PIN -1 // -1 to use power cycle factory reset
#define FORCE_RESET false

// debug settings ////////////
#define WAIT_FOR_SERIAL false

// Uncomment line below to fully disable logging, and reduce project size
//#define DISABLE_LOGGING
#define LOG_LEVEL LOG_LEVEL_TRACE
// #define LOG_LEVEL LOG_LEVEL_NOTICE

#define PRINT_EXECUTION_STAT false

#define ANIMATION_FRAME_RATE 50 // Hz

// register factories to make sure they are not stripped by the linker
#include <pluginFactory.h>
#include <waveEffectFactory.cpp>
REGISTER_ANIMATION_FACTORY(WaveEffectFactory);

