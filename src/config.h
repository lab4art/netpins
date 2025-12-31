#pragma once

#define FIRMWARE_VERSION "netpins-2.0.0-ebd15dd"

// Useful for initial setup via serial console without booting to AP mode
#define WIFI_SSID ""
#define WIFI_PASS ""

#define STATIC_IP      IPAddress(0, 0, 0, 0) // set non zero to enable static IP
#define GATEWAY        IPAddress(0, 0, 0, 0)
#define SUBNET         IPAddress(0, 0, 0, 0)
#define DNS            IPAddress(0, 0, 0, 0)

#define FACTORY_REST_PIN -1 // -1 to use power cycle factory reset

#define ANIMATION_FRAME_RATE 50 // Hz

#include <pluginFactory.h>
// register factories to make sure they are not stripped by the linker
#include <pwmFadeAnimationFactory.cpp>
REGISTER_ANIMATION_FACTORY(PWMFadeAnimationFactory);
#include <tailAnimationFactory.cpp>
REGISTER_ANIMATION_FACTORY(TailAnimationFactory);
#include <waveEffectFactory.cpp>
REGISTER_ANIMATION_FACTORY(WaveEffectFactory);


// debug settings ////////////
#define FORCE_RESET false
#define WAIT_FOR_SERIAL false

//#define DISABLE_LOGGING // uncomment to disable logging completely

// Log level at compile time (can be overridden at runtime via settings)
// Available levels:
//   LOG_LEVEL_SILENT  (-1) - No output (runtime only, don't use for compile-time default)
//   LOG_LEVEL_ERROR   (0) - Only errors
//   LOG_LEVEL_WARNING (1) - Errors and warnings
//   LOG_LEVEL_INFO    (2) - Errors, warnings, and info (default)
//   LOG_LEVEL_TRACE   (3) - Errors, warnings, info, and trace (most verbose)
// To disable logging completely at compile time, uncomment DISABLE_LOGGING below
#define LOG_LEVEL LOG_LEVEL_INFO

#define PRINT_EXECUTION_STAT false
