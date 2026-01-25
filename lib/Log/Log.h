#pragma once

/**
 * Cross-platform logging utility for Arduino and native environments
 * 
 * Features:
 * - Configurable log levels (compile-time via LOG_LEVEL macro, runtime via setLogLevel())
 * - Printf-style formatting support
 * - Timestamp and log level prefixes
 * 
 * Log Levels:
 *   0 - SILENT:  No logging output (can be set at runtime)
 *   1 - ERROR:   Critical errors only
 *   2 - WARNING: Errors and warnings
 *   3 - INFO:    Errors, warnings, and informational messages
 *   4 - TRACE:   Errors, warnings, info, and trace/debug messages (most verbose)
 * 
 * Usage:
 *   Log::errorln("Failed to connect: %s", error);
 *   Log::warningln("Temperature high: %d°C", temp);
 *   Log::infoln("Device started");
 *   Log::traceln("Debug value: %d", value);
 *   
 *   // To disable logging at runtime:
 *   Log::setLogLevel(LOG_LEVEL_SILENT);
 * 
 * Configuration:
 *   - Compile-time: Define LOG_LEVEL in config.h (default: LOG_LEVEL_INFO)
 *                   Define DISABLE_LOGGING to completely disable all logging code
 *   - Runtime: Call Log::setLogLevel(level) to change dynamically
 *              Use Log::setLogLevel(LOG_LEVEL_SILENT) to silence all output
 *   - Can also be configured via Settings struct (persisted in JSON config)
 */

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <iostream>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <chrono>
#endif

// Log level definitions
#define LOG_LEVEL_SILENT 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_TRACE 4

class Log {
private:
    static int& getCurrentLogLevelRef() {
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif
        static int logLevel = LOG_LEVEL;
        return logLevel;
    }
    
    // Low-level output abstraction
    static void output(const char* msg) {
#ifndef DISABLE_LOGGING
#ifdef ARDUINO
        Serial.print(msg);
#else
        std::cout << msg;
#endif
#endif
    }

    static void outputln(const char* msg) {
#ifndef DISABLE_LOGGING
#ifdef ARDUINO
        Serial.println(msg);
#else
        std::cout << msg << std::endl;
#endif
#endif
    }

    static unsigned long getMillis() {
#ifdef ARDUINO
        return millis();
#else
        static auto start = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
#endif
    }

    static void printTimestamp() {
        // Division constants
        const unsigned long MSECS_PER_SEC = 1000;
        const unsigned long SECS_PER_MIN = 60;
        const unsigned long SECS_PER_HOUR = 3600;
        const unsigned long SECS_PER_DAY = 86400;

        // Total time
        const unsigned long msecs = getMillis();
        const unsigned long secs = msecs / MSECS_PER_SEC;

        // Time in components
        const unsigned long MilliSeconds = msecs % MSECS_PER_SEC;
        const unsigned long Seconds = secs % SECS_PER_MIN;
        const unsigned long Minutes = (secs / SECS_PER_MIN) % SECS_PER_MIN;
        const unsigned long Hours = (secs % SECS_PER_DAY) / SECS_PER_HOUR;

        // Time as string
        char timestamp[20];
        sprintf(timestamp, "%02lu:%02lu:%02lu.%03lu ", Hours, Minutes, Seconds, MilliSeconds);
        output(timestamp);
    }

    static void printLogLevel(int logLevel) {
        const char* levelStr;
        switch (logLevel) {
            default:
            case LOG_LEVEL_ERROR: levelStr = "ERROR "; break;
            case LOG_LEVEL_WARNING: levelStr = "WARNING "; break;
            case LOG_LEVEL_INFO: levelStr = "INFO "; break;
            case LOG_LEVEL_TRACE: levelStr = "TRACE "; break;
        }
        output(levelStr);
    }

    static void printPrefix(int logLevel) {
        printTimestamp();
        printLogLevel(logLevel);
    }

    // Format string helper
    static void formatAndLog(int level, const char* format, va_list args) {
#ifndef DISABLE_LOGGING
        if (getCurrentLogLevelRef() < 0 || level > getCurrentLogLevelRef()) {
            return; // Skip logging if silent or level exceeds current log level
        }
#else
        return; // Logging completely disabled at compile time
#endif
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        printPrefix(level);
        println(buffer);
    }

public:
    static void println(const char* msg) {
        outputln(msg);
    }

    static void println(const std::string& msg) {
        println(msg.c_str());
    }

    static void print(const char* msg) {
        output(msg);
    }

    static void print(const std::string& msg) {
        print(msg.c_str());
    }

    // Log with level
    static void log(int level, const char* msg) {
#ifndef DISABLE_LOGGING
        if (getCurrentLogLevelRef() < 0 || level > getCurrentLogLevelRef()) {
            return; // Skip logging if silent or level exceeds current log level
        }
#else
        return; // Logging completely disabled at compile time
#endif
        printPrefix(level);
        println(msg);
    }

    static void log(int level, const std::string& msg) {
        log(level, msg.c_str());
    }

    // Set log level at runtime
    static void setLogLevel(int level) {
        getCurrentLogLevelRef() = level;
    }

    // Get current log level
    static int getLogLevel() {
        return getCurrentLogLevelRef();
    }

    // Convenience methods
    static void error(const char* msg) { log(LOG_LEVEL_ERROR, msg); }
    static void error(const std::string& msg) { log(LOG_LEVEL_ERROR, msg); }
    
    static void warning(const char* msg) { log(LOG_LEVEL_WARNING, msg); }
    static void warning(const std::string& msg) { log(LOG_LEVEL_WARNING, msg); }
    
    static void info(const char* msg) { log(LOG_LEVEL_INFO, msg); }
    static void info(const std::string& msg) { log(LOG_LEVEL_INFO, msg); }
    
    static void trace(const char* msg) { log(LOG_LEVEL_TRACE, msg); }
    static void trace(const std::string& msg) { log(LOG_LEVEL_TRACE, msg); }

    // Format string variants (printf-style)
    static void errorln(const char* format, ...) {
        va_list args;
        va_start(args, format);
        formatAndLog(LOG_LEVEL_ERROR, format, args);
        va_end(args);
    }

    static void warningln(const char* format, ...) {
        va_list args;
        va_start(args, format);
        formatAndLog(LOG_LEVEL_WARNING, format, args);
        va_end(args);
    }

    static void infoln(const char* format, ...) {
        va_list args;
        va_start(args, format);
        formatAndLog(LOG_LEVEL_INFO, format, args);
        va_end(args);
    }

    static void traceln(const char* format, ...) {
        va_list args;
        va_start(args, format);
        formatAndLog(LOG_LEVEL_TRACE, format, args);
        va_end(args);
    }

};
