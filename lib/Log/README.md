# Log - Cross-Platform Logging Utility

A configurable logging utility for Arduino and native C++ environments with support for multiple log levels and printf-style formatting.

## Features

- **Cross-platform**: Works on both Arduino (ESP32) and native C++ environments
- **Configurable log levels**: Both compile-time and runtime configuration
- **Printf-style formatting**: Easy to use format strings with variable arguments
- **Timestamped output**: Automatic timestamps in HH:MM:SS.mmm format
- **Log level prefixes**: Clear indication of message severity

## Log Levels

| Level | Value | Name    | Description                                    |
|-------|-------|---------|------------------------------------------------|
| -1    | SILENT | Silent | No logging output (runtime only)              |
| 0     | ERROR | Error   | Critical errors only                           |
| 1     | WARNING | Warning | Errors and warnings                          |
| 2     | INFO  | Info    | Errors, warnings, and informational messages   |
| 3     | TRACE | Trace   | Errors, warnings, info, and trace/debug (most verbose) |

## Usage

### Basic Logging

```cpp
#include <Log.h>

// Log messages with printf-style formatting
Log::errorln("Failed to connect: %s", errorMessage);
Log::warningln("Temperature high: %d°C", temperature);
Log::infoln("Device started on IP: %s", ipAddress.c_str());
Log::traceln("Debug value: %d", debugValue);

// Disable logging at runtime
Log::setLogLevel(LOG_LEVEL_SILENT);
```

### Configuration

#### Compile-Time Configuration

Set the default log level in `config.h`:

```cpp
// Set the initial log level (can be overridden at runtime)
#define LOG_LEVEL LOG_LEVEL_INFO

// Or disable logging completely (removes all logging code)
#define DISABLE_LOGGING
```

When `DISABLE_LOGGING` is defined, all logging code is completely removed at compile time, reducing code size.

#### Runtime Configuration

Change the log level dynamically:

```cpp
// Set to only show errors
Log::setLogLevel(LOG_LEVEL_ERROR);

// Set to show everything (most verbose)
Log::setLogLevel(LOG_LEVEL_TRACE);

// Disable all logging output
Log::setLogLevel(LOG_LEVEL_SILENT);

// Get current log level
int currentLevel = Log::getLogLevel();
```

#### Settings Configuration (Persistent)

The log level can also be configured via the Settings JSON configuration:

```json
{
  "log_level": 2,
  ...
}
```

Valid values: -1 (SILENT), 0 (ERROR), 1 (WARNING), 2 (INFO), 3 (TRACE)

The log level from settings is automatically applied on boot and when settings are updated via the web interface.

## Example Output

```
00:01:23.456 ERROR Failed to connect: timeout
00:01:23.789 WARNING Temperature high: 85°C
00:01:24.012 INFO Device started on IP: 192.168.1.100
00:01:24.345 TRACE Debug value: 42
```

## Implementation Details

- Log messages at levels higher than the current log level are filtered out (not printed)
- When log level is set to `LOG_LEVEL_SILENT` (-1), all logging is disabled at runtime
- When `DISABLE_LOGGING` is defined, all logging is completely disabled at compile time (no code generated)
- On Arduino, logs output to Serial
- On native platforms, logs output to stdout
- Timestamps are calculated from system boot time
