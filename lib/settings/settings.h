#pragma once

#include <string>
#include <vector>
#include <map>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Log.h>

enum DimmerMode {
    none,
    single,
    perSlice
};

static DimmerMode dimmerModeFromString(std::string dimmer) {
    if (dimmer == "single") {
        return DimmerMode::single;
    } else if (dimmer == "per-slice") {
        return DimmerMode::perSlice;
    }
    return DimmerMode::none;
};

static std::string dimmerModeToString(DimmerMode dimmer) {
    switch (dimmer) {
        case DimmerMode::single:
            return "single";
        case DimmerMode::perSlice:
            return "per-slice";
        default:
            return "none";
    }
};

/**
 * dmx: 0@1 # channel@universe
 */
struct DmxCfg {
    uint16_t universe;
    uint16_t channel; // 1 based

    bool operator==(const DmxCfg& other) const {
        return universe == other.universe &&
            channel == other.channel;
    }

    bool operator!=(const DmxCfg& other) const {
        return !(*this == other);
    }

    bool operator<(const DmxCfg& other) const {
        if (universe != other.universe) return universe < other.universe;
        return channel < other.channel;
    }

    static DmxCfg deserialize(const std::string& dmx) {
        DmxCfg d;
        auto atPos = dmx.find('@');
        if (atPos != std::string::npos) {
            d.channel = std::stoi(dmx.substr(0, atPos));
            d.universe = std::stoi(dmx.substr(atPos + 1));
        } else {
            d.channel = 0;
            d.universe = 0;
        }
        return d;
    }

    static std::string serialize(const DmxCfg& d) {
        return std::to_string(d.channel) + "@" + std::to_string(d.universe);
    }

    uint16_t get0BasedChannel() const {
        if (channel > 512) {
            throw std::invalid_argument("DMX channel must be between 1 and 512");
        }
        return channel - 1;
    }
};

/**
 * - pin: 13
 * name: pwm-13
 * dmx: 0@1 # channel@universe
 */

struct PwmCfg {
    std::uint8_t pin;
    std::string name;
    DmxCfg dmxCfg = {0, 0}; // dmx universe and 1 based channel, 0 means not set

    bool operator==(const PwmCfg& other) const {
        return pin == other.pin &&
            name == other.name &&
            dmxCfg == other.dmxCfg;
    }

    bool operator!=(const PwmCfg& other) const {
        return !(*this == other);
    }

    static PwmCfg deserialize(JsonObject& json) {
        PwmCfg p;
        p.pin = json["pin"].as<std::uint8_t>();
        p.name = json["name"].as<std::string>();
        if (json.containsKey("dmx")) {
            p.dmxCfg = DmxCfg::deserialize(json["dmx"].as<std::string>());
        }
        return p;
    }

    static void serialize(JsonObject& jsonPwm, const PwmCfg& p) {
        jsonPwm["pin"] = p.pin;
        jsonPwm["name"] = p.name;
        jsonPwm["dmx"] = DmxCfg::serialize(p.dmxCfg);
    }
};

struct StripeCfg {
    std::uint8_t pin;
    std::string name;
    std::uint16_t size;
    DimmerMode dimmer;
    // first pixel of each slice
    std::vector<std::uint16_t> slices;
    DmxCfg dmxCfg = {0, 0};

    bool operator==(const StripeCfg& other) const {
        return pin == other.pin &&
            name == other.name &&
            size == other.size &&
            dimmer == other.dimmer &&
            slices == other.slices &&
            dmxCfg == other.dmxCfg;
    }

    bool operator!=(const StripeCfg& other) const {
        return !(*this == other);
    }

    static StripeCfg deserialize(JsonObject& json) {
        StripeCfg s;
        s.pin = json["pin"].as<std::uint8_t>();
        s.name = json["name"].as<std::string>();
        s.size = json["size"].as<std::uint16_t>();
        if (json.containsKey("dimmer")) { // backward compatibility
            s.dimmer = dimmerModeFromString(json["dimmer"].as<std::string>());
        } else {
            s.dimmer = DimmerMode::none;
        }
        JsonArray slicesArray = json["slices"].as<JsonArray>();
        for (JsonVariant v : slicesArray) {
            auto slice = v.as<std::uint16_t>();
            s.slices.push_back(slice);
        }
        if (json.containsKey("dmx")) {
            s.dmxCfg = DmxCfg::deserialize(json["dmx"].as<std::string>());
        }
        return s;
    }

    static void serialize(JsonObject& jsonStripe, const StripeCfg& s) {
        jsonStripe["pin"] = s.pin;
        jsonStripe["name"] = s.name;
        jsonStripe["size"] = s.size;
        jsonStripe["dimmer"] = dimmerModeToString(s.dimmer);
        JsonArray slices = jsonStripe["slices"].to<JsonArray>();
        for (auto slice : s.slices) {
            slices.add(slice);
        }
        jsonStripe["dmx"] = DmxCfg::serialize(s.dmxCfg);
    }
};

enum Direction {
    RIGHT,
    LEFT
};

struct ServoCfg {
    std::uint8_t pin;
    std::string name;
    std::uint8_t maxAngle;
    std::uint16_t minPulseWidth = 0;
    std::uint16_t maxPulseWidth = 0;
    DmxCfg dmxCfg = {0, 0};

    bool operator==(const ServoCfg& other) const {
        return pin == other.pin &&
            name == other.name &&
            maxAngle == other.maxAngle &&
            minPulseWidth == other.minPulseWidth &&
            maxPulseWidth == other.maxPulseWidth &&
            dmxCfg == other.dmxCfg;
    }

    bool operator!=(const ServoCfg& other) const {
        return !(*this == other);
    }

    static ServoCfg deserialize(JsonObject& json) {
        ServoCfg s;
        s.pin = json["pin"].as<std::uint8_t>();
        s.name = json["name"].as<std::string>();
        s.maxAngle = json["max_angle"].as<std::uint8_t>();
        if (json.containsKey("min_pulse_width")) {
            s.minPulseWidth = json["min_pulse_width"].as<std::uint16_t>();
        }
        if (json.containsKey("max_pulse_width")) {
            s.maxPulseWidth = json["max_pulse_width"].as<std::uint16_t>();
        }
        if (json.containsKey("dmx")) {
            s.dmxCfg = DmxCfg::deserialize(json["dmx"].as<std::string>());
        }
        return s;
    }

    static void serialize(JsonObject& jsonServo, const ServoCfg& s) {
        jsonServo["pin"] = s.pin;
        jsonServo["name"] = s.name;
        jsonServo["max_angle"] = s.maxAngle;
        if (s.minPulseWidth != 0) {
            jsonServo["min_pulse_width"] = s.minPulseWidth;
        }
        if (s.maxPulseWidth != 0) {
            jsonServo["max_pulse_width"] = s.maxPulseWidth;
        }
        jsonServo["dmx"] = DmxCfg::serialize(s.dmxCfg);
    }
};

struct DigitalReadSensorCfg {
    std::uint8_t pin;
    std::string sensorName;
    int readMs;

    bool operator==(const DigitalReadSensorCfg& other) const {
        return pin == other.pin &&
            sensorName == other.sensorName &&
            readMs == other.readMs;
    };

    bool operator!=(const DigitalReadSensorCfg& other) const {
        return !(*this == other);
    };

    static DigitalReadSensorCfg deserialize(JsonObject& json) {
        DigitalReadSensorCfg s;
        s.pin = json["pin"].as<std::uint8_t>();
        s.sensorName = json["name"].as<std::string>();
        s.readMs = json["read_ms"].as<int>();
        return s;
    };

    static void serialize(JsonObject& json, const DigitalReadSensorCfg& h) {
        json["pin"] = h.pin;
        json["name"] = h.sensorName;
        json["read_ms"] = h.readMs;
    };

};

struct AnalogReadSensorCfg {
    std::uint8_t pin;
    std::string sensorName;
    int readMs;

    bool operator==(const AnalogReadSensorCfg& other) const {
        return pin == other.pin &&
            sensorName == other.sensorName &&
            readMs == other.readMs;
    };

    bool operator!=(const AnalogReadSensorCfg& other) const {
        return !(*this == other);
    };

    static AnalogReadSensorCfg deserialize(JsonObject& json) {
        AnalogReadSensorCfg s;
        s.pin = json["pin"].as<std::uint8_t>();
        s.sensorName = json["name"].as<std::string>();
        s.readMs = json["read_ms"].as<int>();
        return s;
    };

    static void serialize(JsonObject& json, const AnalogReadSensorCfg& h) {
        json["pin"] = h.pin;
        json["name"] = h.sensorName;
        json["read_ms"] = h.readMs;
    };
};

struct HumTempSensorCfg {
    std::uint8_t pin;
    int readMs;

    bool operator==(const HumTempSensorCfg& other) const {
        return pin == other.pin &&
            readMs == other.readMs;
    };

    bool operator!=(const HumTempSensorCfg& other) const {
        return !(*this == other);
    };

    static HumTempSensorCfg deserialize(JsonObject& json) {
        HumTempSensorCfg h;
        h.pin = json["pin"].as<std::uint8_t>();
        h.readMs = json["read_ms"].as<int>();
        return h;
    };

    static void serialize(JsonObject& jsonHumTemp, const HumTempSensorCfg& h) {
        jsonHumTemp["pin"] = h.pin;
        jsonHumTemp["read_ms"] = h.readMs;
    };
};

struct TouchSensorCfg {
    std::uint8_t pin;
    std::string sensorName;
    int threshold;

    bool operator==(const TouchSensorCfg& other) const {
        return pin == other.pin &&
            sensorName == other.sensorName &&
            threshold == other.threshold;
    };

    bool operator!=(const TouchSensorCfg& other) const {
        return !(*this == other);
    };

    static TouchSensorCfg deserialize(JsonObject& json) {
        TouchSensorCfg t;
        t.pin = json["pin"].as<std::uint8_t>();
        t.sensorName = json["name"].as<std::string>();
        t.threshold = json["threshold"].as<int>();
        return t;
    };

    static void serialize(JsonObject& jsonTouchSensor, const TouchSensorCfg& t) {
        jsonTouchSensor["pin"] = t.pin;
        jsonTouchSensor["name"] = t.sensorName;
        jsonTouchSensor["threshold"] = t.threshold;
    };
};

/**
 * Pipeline processor configuration (used within SensorPipelineCfg)
 * Internal structure for individual processors in a pipeline
 */
struct SensorProcessorCfg {
    std::string name;  // Processor name: averaging, median, peak, ema, threshold, change, gesture, passthrough
    
    // Processor-specific parameters (stored as JSON)
    std::map<std::string, std::string> params;

    bool operator==(const SensorProcessorCfg& other) const {
        return name == other.name &&
            params == other.params;
    };

    bool operator!=(const SensorProcessorCfg& other) const {
        return !(*this == other);
    };

    static SensorProcessorCfg deserialize(JsonObject& json) {
        SensorProcessorCfg p;
        p.name = json["name"].as<std::string>();
        if (json.containsKey("params")) {
            JsonObject paramsObj = json["params"].as<JsonObject>();
            for (JsonPair kv : paramsObj) {
                // Store as string, will be parsed by factory
                if (kv.value().is<int>()) {
                    p.params[kv.key().c_str()] = std::to_string(kv.value().as<int>());
                } else if (kv.value().is<float>()) {
                    p.params[kv.key().c_str()] = std::to_string(kv.value().as<float>());
                } else if (kv.value().is<JsonArray>()) {
                    // For arrays like gesture sequence, serialize as JSON
                    String arrayStr;
                    serializeJson(kv.value(), arrayStr);
                    p.params[kv.key().c_str()] = arrayStr.c_str();
                } else {
                    p.params[kv.key().c_str()] = kv.value().as<std::string>();
                }
            }
        }
        return p;
    };

    static void serialize(JsonObject& json, const SensorProcessorCfg& p) {
        json["name"] = p.name;
        if (!p.params.empty()) {
            JsonObject paramsObj = json["params"].to<JsonObject>();
            for (auto it = p.params.begin(); it != p.params.end(); ++it) {
                const std::string& key = it->first;
                const std::string& value = it->second;
                // Try to parse as number, otherwise store as string
                char* endPtr;
                int intVal = strtol(value.c_str(), &endPtr, 10);
                if (*endPtr == '\0') {
                    paramsObj[key] = intVal;
                } else {
                    float floatVal = strtof(value.c_str(), &endPtr);
                    if (*endPtr == '\0') {
                        paramsObj[key] = floatVal;
                    } else {
                        paramsObj[key] = value;
                    }
                }
            }
        }
    };
};

/**
 * Pipeline-based sensor processor configuration
 * 
 * Example YAML:
 * sensor_pipelines:
 *   - sensor: temperature_1
 *     pipeline:
 *       - type: range_mapping
 *         input_min: -40.0
 *         input_max: 100.0
 *         output_min: 0.0
 *         output_max: 255.0
 *       - type: debounce
 *         duration_ms: 500
 *       - type: moving_average
 *         window_size: 5
 *     dmx: 10@1  # channel 10, universe 1 (optional)
 *     mqtt: temperature  # MQTT topic suffix (optional, full topic: prefix + suffix)
 */
struct SensorPipelineCfg {
    std::string sensorName;
    std::vector<SensorProcessorCfg> processors;
    DmxCfg dmxCfg = {0, 0};
    std::string mqttTopic = "";  // MQTT topic suffix (empty = don't publish)

    bool operator==(const SensorPipelineCfg& other) const {
        return sensorName == other.sensorName &&
            processors == other.processors &&
            dmxCfg == other.dmxCfg &&
            mqttTopic == other.mqttTopic;
    };

    bool operator!=(const SensorPipelineCfg& other) const {
        return !(*this == other);
    };

    static SensorPipelineCfg deserialize(JsonObject& json) {
        SensorPipelineCfg cfg;
        cfg.sensorName = json["sensor"].as<std::string>();
        
        // Parse pipeline processors
        if (json.containsKey("pipeline")) {
            JsonArray pipelineArray = json["pipeline"].as<JsonArray>();
            for (JsonVariant v : pipelineArray) {
                JsonObject procJson = v.as<JsonObject>();
                SensorProcessorCfg proc;
                
                // The 'type' field maps to the processor name
                if (procJson.containsKey("type")) {
                    proc.name = procJson["type"].as<std::string>();
                }
                
                // All other fields go into params
                for (JsonPair kv : procJson) {
                    std::string key(kv.key().c_str());
                    if (key != "type") {
                        if (kv.value().is<int>()) {
                            proc.params[key] = std::to_string(kv.value().as<int>());
                        } else if (kv.value().is<float>()) {
                            proc.params[key] = std::to_string(kv.value().as<float>());
                        } else if (kv.value().is<bool>()) {
                            proc.params[key] = kv.value().as<bool>() ? "true" : "false";
                        } else if (kv.value().is<JsonArray>() || kv.value().is<JsonObject>()) {
                            String jsonStr;
                            serializeJson(kv.value(), jsonStr);
                            proc.params[key] = jsonStr.c_str();
                        } else {
                            proc.params[key] = kv.value().as<std::string>();
                        }
                    }
                }
                
                cfg.processors.push_back(proc);
            }
        }
        
        // Parse DMX configuration
        if (json.containsKey("dmx")) {
            cfg.dmxCfg = DmxCfg::deserialize(json["dmx"].as<std::string>());
        }
        
        // Parse MQTT topic suffix
        if (json.containsKey("mqtt")) {
            cfg.mqttTopic = json["mqtt"].as<std::string>();
        }
        
        return cfg;
    };

    static void serialize(JsonObject& json, const SensorPipelineCfg& cfg) {
        json["sensor"] = cfg.sensorName;
        
        // Serialize pipeline
        if (!cfg.processors.empty()) {
            JsonArray pipelineArray = json["pipeline"].to<JsonArray>();
            for (const auto& proc : cfg.processors) {
            JsonObject procJson = pipelineArray.add<JsonObject>();
            procJson["type"] = proc.name;
            
            // Serialize all params
            for (const auto& param : proc.params) {
                const std::string& key = param.first;
                const std::string& value = param.second;
                
                // Try to parse as number
                char* endPtr;
                int intVal = strtol(value.c_str(), &endPtr, 10);
                if (*endPtr == '\0') {
                    procJson[key] = intVal;
                } else {
                    float floatVal = strtof(value.c_str(), &endPtr);
                    if (*endPtr == '\0') {
                        procJson[key] = floatVal;
                    } else if (value == "true" || value == "false") {
                        procJson[key] = (value == "true");
                    } else {
                        procJson[key] = value;
                    }
                }
            }
            }
        }
        
        // Serialize DMX
        json["dmx"] = DmxCfg::serialize(cfg.dmxCfg);
        
        // Serialize MQTT topic
        if (!cfg.mqttTopic.empty()) {
            json["mqtt"] = cfg.mqttTopic;
        }
    };
};

/**
 * DMX Processor Configuration
 * 
 * Example YAML:
 * dmx_processors:
 *   - type: sequence
 *     name: cue_player
 *     loop: true
 *     control_channel: 100@0  # Optional: control via DMX channel
 *     enable_threshold: 2.0   # Enable when channel >= 2
 *     disable_threshold: 1.5  # Disable when channel < 1.5 (hysteresis)
 *     sequence:
 *       - channels:
 *           1@0: 255  # channel@universe: value
 *           2@0: 200
 *           3@0: 100
 *         fade_in_ms: 2000
 *         hold_ms: 5000
 *       - channels:
 *           1@0: 0
 *           2@0: 100
 *           3@0: 255
 *         fade_in_ms: 1000
 *         hold_ms: 3000
 */
struct DmxProcessorCfg {
    std::string type;     // Processor type: sequence, etc.
    std::string name;     // Optional name for identification
    DmxCfg controlChannel;  // DMX channel to control enable/disable (0,0 = always on)
    int minValue;         // Minimum DMX value for range (inclusive, default: -1 = always on)
    int maxValue;         // Maximum DMX value for range (inclusive, default: 255)
    std::map<std::string, std::string> params;  // Type-specific parameters

    bool operator==(const DmxProcessorCfg& other) const {
        return type == other.type &&
            name == other.name &&
            controlChannel == other.controlChannel &&
            minValue == other.minValue &&
            maxValue == other.maxValue &&
            params == other.params;
    }

    bool operator!=(const DmxProcessorCfg& other) const {
        return !(*this == other);
    }

    static DmxProcessorCfg deserialize(JsonObject& json) {
        DmxProcessorCfg cfg;
        cfg.type = json["type"].as<std::string>();
        
        if (json.containsKey("name")) {
            cfg.name = json["name"].as<std::string>();
        }
        
        // Parse control channel
        if (json.containsKey("control_channel")) {
            cfg.controlChannel = DmxCfg::deserialize(json["control_channel"].as<std::string>());
        } else {
            cfg.controlChannel = {0, 0};  // 0,0 = always on
        }
        
        // Parse range values
        cfg.minValue = json["min_value"] | -1;  // -1 = always on
        cfg.maxValue = json["max_value"] | 255;  // Default to full DMX range
        
        // Store all other fields as params
        for (JsonPair kv : json) {
            std::string key(kv.key().c_str());
            if (key != "type" && key != "name" && 
                key != "control_channel" && key != "min_value" && key != "max_value") {
                if (kv.value().is<int>()) {
                    cfg.params[key] = std::to_string(kv.value().as<int>());
                } else if (kv.value().is<float>()) {
                    cfg.params[key] = std::to_string(kv.value().as<float>());
                } else if (kv.value().is<bool>()) {
                    cfg.params[key] = kv.value().as<bool>() ? "true" : "false";
                } else if (kv.value().is<JsonArray>() || kv.value().is<JsonObject>()) {
                    String jsonStr;
                    serializeJson(kv.value(), jsonStr);
                    cfg.params[key] = jsonStr.c_str();
                } else {
                    cfg.params[key] = kv.value().as<std::string>();
                }
            }
        }
        
        return cfg;
    }

    static void serialize(JsonObject& json, const DmxProcessorCfg& cfg) {
        json["type"] = cfg.type;
        
        if (!cfg.name.empty()) {
            json["name"] = cfg.name;
        }
        
        // Serialize control channel
        if (cfg.controlChannel.channel != 0 || cfg.controlChannel.universe != 0) {
            json["control_channel"] = DmxCfg::serialize(cfg.controlChannel);
        }
        
        json["min_value"] = cfg.minValue;
        json["max_value"] = cfg.maxValue;
        
        // Serialize all params
        for (const auto& param : cfg.params) {
            const std::string& key = param.first;
            const std::string& value = param.second;
            
            // Try to parse as number or bool
            char* endPtr;
            int intVal = strtol(value.c_str(), &endPtr, 10);
            if (*endPtr == '\0') {
                json[key] = intVal;
            } else {
                float floatVal = strtof(value.c_str(), &endPtr);
                if (*endPtr == '\0') {
                    json[key] = floatVal;
                } else if (value == "true" || value == "false") {
                    json[key] = (value == "true");
                } else if (value[0] == '{' || value[0] == '[') {
                    // Try to parse as JSON
                    JsonDocument subDoc;
                    DeserializationError error = deserializeJson(subDoc, value);
                    if (!error) {
                        json[key] = subDoc.as<JsonVariant>();
                    } else {
                        json[key] = value;
                    }
                } else {
                    json[key] = value;
                }
            }
        }
    }
};

struct MqttCfg {
    std::string server;
    std::uint16_t port;
    std::string user;
    std::string password;

    bool operator==(const MqttCfg& other) const {
        return server == other.server &&
            port == other.port &&
            user == other.user &&
            password == other.password;
    };
    bool operator!=(const MqttCfg& other) const {
        return !(*this == other);
    };

    static MqttCfg deserialize(JsonObject& json) {
        MqttCfg m;
        m.server = json["server"].as<std::string>();
        m.port = json["port"].as<std::uint16_t>();
        m.user = json["user"].as<std::string>();
        m.password = json["password"].as<std::string>();
        return m;
    };

    static void serialize(JsonObject& json, const MqttCfg& m) {
        json["server"] = m.server;
        json["port"] = m.port;
        json["user"] = m.user;
        json["password"] = m.password;
    };
};


struct PluginCfg {
    std::string name;
    std::string type;
    std::string config; // Store as JSON string to avoid reference issues

    bool operator==(const PluginCfg& other) const {
        return name == other.name &&
        type == other.type &&
        config == other.config;
    };

    bool operator!=(const PluginCfg& other) const {
        return !(*this == other);
    };

    static PluginCfg deserialize(JsonObject& json) {
        PluginCfg p;
        
        try {
            p.name = json["name"].as<std::string>();
            p.type = json["type"].as<std::string>();
            
            // Serialize the config object to a string for safe storage
            if (json.containsKey("config")) {
                std::string configStr;
                size_t result = serializeJson(json["config"], configStr);
                if (result > 0) {
                    p.config = configStr.c_str();
                } else {
                    p.config = "{}";
                }
            } else {
                p.config = "{}";
            }
            
        } catch (...) {
            // If anything fails, set safe defaults
            p.name = "error";
            p.type = "error";
            p.config = "{}";
        }
        
        return p;
    };

    static void serialize(JsonObject& jsonPlugin, const PluginCfg& p) {
        try {
            // Set name and type
            jsonPlugin["name"] = p.name;
            jsonPlugin["type"] = p.type;
            
            // Handle config: parse JSON string back to object
            if (!p.config.empty() && p.config != "{}") {
                JsonDocument configDoc;
                DeserializationError error = deserializeJson(configDoc, p.config);
                if (!error) {
                    jsonPlugin["config"] = configDoc.as<JsonObject>();
                } else {
                    // Parse failed, use empty object
                    jsonPlugin["config"].to<JsonObject>();
                }
            } else {
                // Empty or default config
                jsonPlugin["config"].to<JsonObject>();
            }
            
        } catch (...) {
            // If anything fails, set safe defaults
            jsonPlugin["name"] = "error";
            jsonPlugin["type"] = "error";
            jsonPlugin["config"].to<JsonObject>();
        }
    };
};

/**
 * DMX Output Configuration for MAX485
 * 
 * Transmits DMX data via RS-485 to control external DMX devices.
 * Sensors are mapped to DMX channels using sensor_pipelines, and this output
 * physically transmits the specified universe's data.
 * 
 * dmx_output:
 *   enabled: true
 *   uart_port: 1
 *   tx_pin: 17
 *   rx_pin: 16  # can be -1 if not used
 *   enable_pin: 4  # DE/RE pins on MAX485
 *   universe: 0  # which universe to transmit (matches sensor_pipelines universe)
 */
struct DmxOutputCfg {
    bool enabled;
    int uartPort;
    int txPin;
    int rxPin;
    int enablePin;
    uint16_t universe;

    bool operator==(const DmxOutputCfg& other) const {
        return enabled == other.enabled &&
            uartPort == other.uartPort &&
            txPin == other.txPin &&
            rxPin == other.rxPin &&
            enablePin == other.enablePin &&
            universe == other.universe;
    }

    bool operator!=(const DmxOutputCfg& other) const {
        return !(*this == other);
    }

    static DmxOutputCfg deserialize(JsonObject& json) {
        DmxOutputCfg d;
        d.enabled = json["enabled"].as<bool>();
        d.uartPort = json["uart_port"].as<int>();
        d.txPin = json["tx_pin"].as<int>();
        d.rxPin = json["rx_pin"].as<int>();
        d.enablePin = json["enable_pin"].as<int>();
        d.universe = json["universe"].as<uint16_t>();
        return d;
    }

    static void serialize(JsonObject& jsonDmxOut, const DmxOutputCfg& d) {
        jsonDmxOut["enabled"] = d.enabled;
        jsonDmxOut["uart_port"] = d.uartPort;
        jsonDmxOut["tx_pin"] = d.txPin;
        jsonDmxOut["rx_pin"] = d.rxPin;
        jsonDmxOut["enable_pin"] = d.enablePin;
        jsonDmxOut["universe"] = d.universe;
    }
};

/**
 * DMX Input Configuration for MAX485
 * 
 * Receives DMX data via RS-485 from an external DMX source (like a lighting console).
 * This provides an alternative to ArtNet for receiving DMX control data.
 * The received data is mapped to the specified universe and processed by DmxManager.
 * 
 * dmx_input:
 *   enabled: true
 *   uart_port: 2
 *   tx_pin: -1  # can be -1 if not used for transmit
 *   rx_pin: 16  # RX pin for receiving DMX data
 *   enable_pin: 4  # DE/RE pins on MAX485
 *   enable_pin: 4  # DE/RE pins on MAX485
 *   universe: 0  # which universe this input represents
 */
struct DmxInputCfg {
    bool enabled;
    int uartPort;
    int txPin;
    int rxPin;
    int enablePin;
    uint16_t universe;

    bool operator==(const DmxInputCfg& other) const {
        return enabled == other.enabled &&
            uartPort == other.uartPort &&
            txPin == other.txPin &&
            rxPin == other.rxPin &&
            enablePin == other.enablePin &&
            universe == other.universe;
    }

    bool operator!=(const DmxInputCfg& other) const {
        return !(*this == other);
    }

    static DmxInputCfg deserialize(JsonObject& json) {
        DmxInputCfg d;
        d.enabled = json["enabled"].as<bool>();
        d.uartPort = json["uart_port"].as<int>();
        d.txPin = json["tx_pin"].as<int>();
        d.rxPin = json["rx_pin"].as<int>();
        d.enablePin = json["enable_pin"].as<int>();
        d.universe = json["universe"].as<uint16_t>();
        return d;
    }

    static void serialize(JsonObject& jsonDmxIn, const DmxInputCfg& d) {
        jsonDmxIn["enabled"] = d.enabled;
        jsonDmxIn["uart_port"] = d.uartPort;
        jsonDmxIn["tx_pin"] = d.txPin;
        jsonDmxIn["rx_pin"] = d.rxPin;
        jsonDmxIn["enable_pin"] = d.enablePin;
        jsonDmxIn["universe"] = d.universe;
    }
};

struct Settings {
    u_int8_t dmxChOffset;
    std::string wifiSsid;
    std::string wifiPass;
    std::string hostname;
    std::uint32_t hbInt;
    std::uint16_t udpPort;

    std::vector<PwmCfg> pwms;
    std::vector<StripeCfg> rgbwStrips;
    std::vector<StripeCfg> rgbStrips;
    std::vector<ServoCfg> servos;
    
    std::vector<HumTempSensorCfg> humTemps;
    std::vector<TouchSensorCfg> touchSensors;
    std::vector<DigitalReadSensorCfg> digitalReadSensors;
    std::vector<AnalogReadSensorCfg> analogReadSensors;
    
    std::vector<SensorPipelineCfg> sensorPipelines; // Pipeline-based sensor processing
    std::vector<DmxProcessorCfg> dmxProcessors;     // DMX effect processors

    std::vector<PluginCfg> plugins;

    bool lightsTest;
    std::uint16_t maxIdle; // max idle time in min, 0 means no sleep
    unsigned int rebootAfterWifiFailed = 15; // reboot after 15 failed wifi connections, 0 means no reboot
    bool disableWifiPowerSave = false;
    bool disableWifiReconnect = false; // if true, only try to connect once at startup
    bool disableArtnet = false;
    int logLevel = -1; // default to not apply (-1=don't apply, 0=SILENT, 1=ERROR, 2=WARNING, 3=INFO, 4=TRACE)

    MqttCfg mqtt;
    DmxOutputCfg dmxOutput;
    DmxInputCfg dmxInput;

    bool operator==(const Settings& other) const {
        return
            dmxChOffset == other.dmxChOffset &&
            wifiSsid == other.wifiSsid &&
            wifiPass == other.wifiPass &&
            hostname == other.hostname &&
            hbInt == other.hbInt &&
            udpPort == other.udpPort &&
            lightsTest == other.lightsTest &&
            maxIdle == other.maxIdle &&
            rebootAfterWifiFailed == other.rebootAfterWifiFailed &&
            disableWifiPowerSave == other.disableWifiPowerSave &&
            disableWifiReconnect == other.disableWifiReconnect &&
            disableArtnet == other.disableArtnet &&
            logLevel == other.logLevel &&
            mqtt == other.mqtt &&
            dmxOutput == other.dmxOutput &&
            dmxInput == other.dmxInput &&

            pwms == other.pwms &&
            rgbwStrips == other.rgbwStrips &&
            rgbStrips == other.rgbStrips &&
            servos == other.servos &&

            humTemps == other.humTemps &&
            touchSensors == other.touchSensors &&
            digitalReadSensors == other.digitalReadSensors &&
            analogReadSensors == other.analogReadSensors &&

            sensorPipelines == other.sensorPipelines &&
            dmxProcessors == other.dmxProcessors &&

            plugins == other.plugins;
            
    }

    bool operator!=(const Settings& other) const {
        return !(*this == other);
    }

    static void deserialize(Settings& s, JsonDocument& json) {
        s.dmxChOffset = json["dmx_offset"].as<u_int8_t>();
        s.wifiSsid = json["wifi_ssid"].as<std::string>();
        s.wifiPass = json["wifi_pass"].as<std::string>();
        s.hostname = json["hostname"].as<std::string>();
        s.hbInt = json["hb_int"].as<std::uint32_t>();
        s.udpPort = json["udp_port"].as<std::uint16_t>();
        s.lightsTest = json["lights_test"].as<bool>();
        s.maxIdle = json["max_idle"].as<std::uint16_t>();
        s.rebootAfterWifiFailed = json["reboot_after_wifi_failed"].as<unsigned int>();
        if (json.containsKey("disable_wifi_power_save")) { // backward compatibility
            s.disableWifiPowerSave = json["disable_wifi_power_save"].as<bool>();
        } else {
            s.disableWifiPowerSave = false;
        }
        if (json.containsKey("disable_wifi_reconnect")) {
            s.disableWifiReconnect = json["disable_wifi_reconnect"].as<bool>();
        } else {
            s.disableWifiReconnect = false;
        }
        if (json.containsKey("disable_artnet")) {
            s.disableArtnet = json["disable_artnet"].as<bool>();
        } else {
            s.disableArtnet = false;
        }
        if (json.containsKey("log_level")) {
            s.logLevel = json["log_level"].as<int>();
        } else {
            s.logLevel = -1; // default to not apply
        }
        if (json.containsKey("mqtt")) {
            JsonObject jsonMqtt = json["mqtt"].as<JsonObject>();
            s.mqtt = MqttCfg::deserialize(jsonMqtt);
        } else {
            s.mqtt = MqttCfg();
        }
        if (json.containsKey("dmx_output")) {
            JsonObject jsonDmxOut = json["dmx_output"].as<JsonObject>();
            s.dmxOutput = DmxOutputCfg::deserialize(jsonDmxOut);
        } else {
            s.dmxOutput = DmxOutputCfg{false, 1, -1, -1, -1, 0};
        }
        if (json.containsKey("dmx_input")) {
            JsonObject jsonDmxIn = json["dmx_input"].as<JsonObject>();
            s.dmxInput = DmxInputCfg::deserialize(jsonDmxIn);
        } else {
            s.dmxInput = DmxInputCfg{false, 2, -1, -1, -1, 0};
        }

        // actuators
        JsonArray pwmsArray = json["pwms"].as<JsonArray>();
        for (JsonVariant v : pwmsArray) {
            JsonObject jsonPwm = v.as<JsonObject>();
            s.pwms.push_back(PwmCfg::deserialize(jsonPwm));
        }

        JsonArray rgbwStripsArray = json["rgbw_strips"].as<JsonArray>();
        for (JsonVariant v : rgbwStripsArray) {
            JsonObject jsonStripe = v.as<JsonObject>();
            s.rgbwStrips.push_back(StripeCfg::deserialize(jsonStripe));
        }

        JsonArray rgbStripsArray = json["rgb_strips"].as<JsonArray>();
        for (JsonVariant v : rgbStripsArray) {
            JsonObject jsonStripe = v.as<JsonObject>();
            s.rgbStrips.push_back(StripeCfg::deserialize(jsonStripe));
        }

        JsonArray servosArray = json["servos"].as<JsonArray>();
        for (JsonVariant v : servosArray) {
            JsonObject jsonServo = v.as<JsonObject>();
            s.servos.push_back(ServoCfg::deserialize(jsonServo));
        }


        // sensors
        JsonArray digitalReadSensorsArray = json["digital_reads"].as<JsonArray>();
        for (JsonVariant v : digitalReadSensorsArray) {
            JsonObject jsonDigitalRead = v.as<JsonObject>();
            s.digitalReadSensors.push_back(DigitalReadSensorCfg::deserialize(jsonDigitalRead));
        }

        JsonArray analogReadSensorsArray = json["analog_reads"].as<JsonArray>();
        for (JsonVariant v : analogReadSensorsArray) {
            JsonObject jsonAnalogRead = v.as<JsonObject>();
            s.analogReadSensors.push_back(AnalogReadSensorCfg::deserialize(jsonAnalogRead));
        }

        JsonArray humTempsArray = json["hum_temps"].as<JsonArray>();
        for (JsonVariant v : humTempsArray) {
            JsonObject jsonHumTemp = v.as<JsonObject>();
            s.humTemps.push_back(HumTempSensorCfg::deserialize(jsonHumTemp));
        }

        JsonArray touchSensorsArray = json["touch_sensors"].as<JsonArray>();
        for (JsonVariant v : touchSensorsArray) {
            JsonObject jsonTouchSensor = v.as<JsonObject>();
            s.touchSensors.push_back(TouchSensorCfg::deserialize(jsonTouchSensor));
        }

        // sensor pipelines
        if (json.containsKey("sensor_pipelines")) {
            JsonArray pipelinesArray = json["sensor_pipelines"].as<JsonArray>();
            for (JsonVariant v : pipelinesArray) {
                JsonObject jsonPipeline = v.as<JsonObject>();
                s.sensorPipelines.push_back(SensorPipelineCfg::deserialize(jsonPipeline));
            }
        }

        // dmx processors
        if (json.containsKey("dmx_processors")) {
            JsonArray processorsArray = json["dmx_processors"].as<JsonArray>();
            for (JsonVariant v : processorsArray) {
                JsonObject jsonProc = v.as<JsonObject>();
                s.dmxProcessors.push_back(DmxProcessorCfg::deserialize(jsonProc));
            }
        }

        // 
        // plugins
        JsonArray pluginsArray = json["plugins"].as<JsonArray>();
        for (JsonVariant v : pluginsArray) {
            JsonObject jsonPlugin = v.as<JsonObject>();
            s.plugins.push_back(PluginCfg::deserialize(jsonPlugin));
        }
    };

    void serialize(JsonDocument& json) {
        json["dmx_offset"] = dmxChOffset;
        json["wifi_ssid"] = wifiSsid;
        json["wifi_pass"] = wifiPass;
        json["hostname"] = hostname;
        json["hb_int"] = hbInt;
        json["udp_port"] = udpPort;
        json["lights_test"] = lightsTest;
        json["max_idle"] = maxIdle;
        json["reboot_after_wifi_failed"] = rebootAfterWifiFailed;
        json["disable_wifi_power_save"] = disableWifiPowerSave;
        json["disable_wifi_reconnect"] = disableWifiReconnect;
        json["disable_artnet"] = disableArtnet;
        json["log_level"] = logLevel;

        if (mqtt.server != "") {
            JsonObject jsonMqtt = json["mqtt"].to<JsonObject>();
            MqttCfg::serialize(jsonMqtt, mqtt);
        }

        if (dmxOutput.enabled) {
            JsonObject jsonDmxOut = json["dmx_output"].to<JsonObject>();
            DmxOutputCfg::serialize(jsonDmxOut, dmxOutput);
        }

        if (dmxInput.enabled) {
            JsonObject jsonDmxIn = json["dmx_input"].to<JsonObject>();
            DmxInputCfg::serialize(jsonDmxIn, dmxInput);
        }

        // actuators
        
        if (pwms.size() > 0) {
            JsonArray jsonPwms = json["pwms"].to<JsonArray>();
            for (auto pwm : this->pwms) {
                JsonObject jsonPwmItem = jsonPwms.add<JsonObject>();
                PwmCfg::serialize(jsonPwmItem, pwm);
            }
        }

        if (rgbwStrips.size() > 0) {
            JsonArray jsonRgbw = json["rgbw_strips"].to<JsonArray>();
            for (auto stripe : rgbwStrips) {
                JsonObject jsonStripe = jsonRgbw.add<JsonObject>();
                StripeCfg::serialize(jsonStripe, stripe);
            }
        }

        if (rgbStrips.size() > 0) {
            JsonArray jsonRgb = json["rgb_strips"].to<JsonArray>();
            for (auto stripe : rgbStrips) {
                JsonObject jsonStripe = jsonRgb.add<JsonObject>();
                StripeCfg::serialize(jsonStripe, stripe);
            }
        }

        if (servos.size() > 0) {
            JsonArray servosArray = json["servos"].to<JsonArray>();
            for (auto servo : servos) {
                JsonObject jsonServo = servosArray.add<JsonObject>();
                ServoCfg::serialize(jsonServo, servo);
            }
        }

        // sensors
        if (digitalReadSensors.size() > 0) {
            JsonArray digitalReadSensors = json["digital_reads"].to<JsonArray>();
            for (auto digitalReadSensor : this->digitalReadSensors) {
                JsonObject jsonDigitalRead = digitalReadSensors.add<JsonObject>();
                DigitalReadSensorCfg::serialize(jsonDigitalRead, digitalReadSensor);
            }
        }

        if (analogReadSensors.size() > 0) {
            JsonArray analogReadSensors = json["analog_reads"].to<JsonArray>();
            for (auto analogReadSensor : this->analogReadSensors) {
                JsonObject jsonAnalogRead = analogReadSensors.add<JsonObject>();
                AnalogReadSensorCfg::serialize(jsonAnalogRead, analogReadSensor);
            }
        }

        if (humTemps.size() > 0) {
            JsonArray humTemps = json["hum_temps"].to<JsonArray>();
            for (auto humTemp : this->humTemps) {
                JsonObject jsonHumTemp = humTemps.add<JsonObject>();
                HumTempSensorCfg::serialize(jsonHumTemp, humTemp);
            }
        }

        if (touchSensors.size() > 0) {
            JsonArray touchSensors = json["touch_sensors"].to<JsonArray>();
            for (auto touchSensor : this->touchSensors) {
                JsonObject jsonTouchSensor = touchSensors.add<JsonObject>();
                TouchSensorCfg::serialize(jsonTouchSensor, touchSensor);
            }
        }

        // sensor pipelines
        if (sensorPipelines.size() > 0) {
            JsonArray pipelinesArray = json["sensor_pipelines"].to<JsonArray>();
            for (auto& pipeline : this->sensorPipelines) {
                JsonObject jsonPipeline = pipelinesArray.add<JsonObject>();
                SensorPipelineCfg::serialize(jsonPipeline, pipeline);
            }
        }

        // dmx processors
        if (dmxProcessors.size() > 0) {
            JsonArray processorsArray = json["dmx_processors"].to<JsonArray>();
            for (auto& processor : this->dmxProcessors) {
                JsonObject jsonProc = processorsArray.add<JsonObject>();
                DmxProcessorCfg::serialize(jsonProc, processor);
            }
        }

        Log::info((std::string("Serializing ") + std::to_string(plugins.size()) + " plugins").c_str());
        if (plugins.size() > 0) {
            JsonArray plugins = json["plugins"].to<JsonArray>();
            for (size_t i = 0; i < this->plugins.size(); i++) {
                try {
                    JsonObject jsonPlugin = plugins.add<JsonObject>();
                    if (jsonPlugin.isNull()) {
                        Log::error("Failed to create JsonObject for plugin");
                        continue;
                    }
                    PluginCfg::serialize(jsonPlugin, this->plugins[i]);
                    Log::info("Successfully serialized plugin");
                } catch (const std::exception& e) {
                    Log::error("Exception while serializing plugin");
                } catch (...) {
                    Log::error("Unknown exception while serializing plugin");
                }
            }
        }

    };

    std::string asJson() {
        JsonDocument jsonDoc;
        serialize(jsonDoc);
        std::string output;
        size_t result = serializeJson(jsonDoc, output);
        if (result == 0) {
            Log::error("JSON serialization failed.");
            return "{}"; // Return empty JSON on failure
        }
        return output;
    };

    void setDefaults() {
        this->dmxChOffset = 0;
        this->wifiSsid = "";  // default set by main
        this->wifiPass = "";  // default set by main
        this->hostname = "";
        this->hbInt = 5000;
        this->udpPort = 5824;
        this->lightsTest = true;
        this->maxIdle = 0;
    };
};

template <typename T>
class SettingsManager {
    private:
        std::string nspace;
        bool dirty;
        Preferences preferences;
        T settings;

        void onError(std::string message) {
            Log::error(message.c_str());
        }

        void fromJsonDoc(JsonDocument& jsonDoc) {
            T newSettings;
            T::deserialize(newSettings, jsonDoc);
            if (newSettings != this->settings) {
                Log::info("Settings changed.");
                this->settings = newSettings;
                dirty = true;
            }
        }

    public:
        SettingsManager(std::string nspace):
            nspace(nspace),
            dirty(false) {
        }

        void load() {
            // Serial.println("Loading settings...");
            preferences.begin(this->nspace.c_str(), true);
            std::string jsonStr = preferences.getString("json").c_str();
            Log::info(("Loaded settings json: " + jsonStr).c_str());

            JsonDocument json_doc;
            deserializeJson(json_doc, jsonStr);
            T::deserialize(this->settings, json_doc);
            preferences.end();
            dirty = false;

            Log::info("Deserialized settings");
        }

        void save() {
            if (!dirty) {
                return;
            }
            if (!preferences.begin(this->nspace.c_str(), false)) {
                onError(std::string("Error opening preferences with namespace: ") + this->nspace);
                return;
            }

            Log::info("Saving settings ...");
            JsonDocument jsonDoc;
            this->settings.serialize(jsonDoc);
            std::string output;
            size_t result = serializeJson(jsonDoc, output);
            if (result == 0) {
                onError("JSON serialization failed.");
                return;
            }

            size_t bytesWritten = preferences.putString("json", output.c_str());
            if (bytesWritten != output.length()) {
                onError(std::string("Error writing to preferences: ") + this->nspace);
            }
            preferences.end();
            dirty = false;
        }

        void fromJson(std::string jsonString) {
            // parse jsonString to jsonDoc
            JsonDocument jsonDoc;
            DeserializationError error = deserializeJson(jsonDoc, jsonString);
            if (error) {
                onError(std::string("JSON deserialization failed: ") + error.c_str());
                return;
            }
            fromJsonDoc(jsonDoc);
        }

        /**
         * Applies potentially partial json to current settings.
         * Only top level keys are merged, nested keys are replaced.
         */
        void mergeJson(std::string jsonString) {
            JsonDocument newJsonDoc;
            DeserializationError error = deserializeJson(newJsonDoc, jsonString);
            if (error) {
                onError(std::string("JSON merge deserialization failed: ") + error.c_str());
                return;
            }

            JsonDocument currentJsonDoc;
            this->settings.serialize(currentJsonDoc);            

            // merge jsons by copying values from newJsonDoc to currentJsonDoc
            for (JsonPair kv : newJsonDoc.as<JsonObject>()) {
                currentJsonDoc[kv.key()] = kv.value();
            }
            fromJsonDoc(currentJsonDoc);
        }

        T& getSettings() {
            return this->settings;
        }

        void setDefaults() {
            T oldSettings = this->settings;
            this->settings.setDefaults();
            if (oldSettings != this->settings) {
                // Serial.println("Settings changed.");
                dirty = true;
            }
        }

        bool isDirty() {
            return dirty;
        }

};
