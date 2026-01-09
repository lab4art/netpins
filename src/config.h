#pragma once

#define FIRMWARE_VERSION "netpins-2.0.0-snapshot"

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
#define LOG_LEVEL LOG_LEVEL_INFO

#define PRINT_EXECUTION_STAT false
