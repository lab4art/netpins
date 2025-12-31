#pragma once

#include <string>
#include <vector>
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

    uint16_t get0BasedChannel() {
        if (channel > 512) {
            throw std::invalid_argument("DMX channel must be between 1 and 512");
        }
        return channel - 1;
    }
};

/**
 * value_range: # map read value range to dmx value 0-255
 *   from: 0
 *   to: 1023
 */
struct ValueRange {
    int from;
    int to;

    bool operator==(const ValueRange& other) const {
        return from == other.from &&
            to == other.to;
    }

    bool operator!=(const ValueRange& other) const {
        return !(*this == other);
    }

    static ValueRange deserialize(JsonObject& json) {
        ValueRange vr;
        vr.from = json["from"].as<int>();
        vr.to = json["to"].as<int>();
        return vr;
    }

    static void serialize(JsonObject& jsonVr, const ValueRange& vr) {
        jsonVr["from"] = vr.from;
        jsonVr["to"] = vr.to;
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

struct SensorMappingCfg {
    std::string sensorName;
    DmxCfg dmxCfg;
    ValueRange valueRange;

    bool operator==(const SensorMappingCfg& other) const {
        return sensorName == other.sensorName &&
            dmxCfg == other.dmxCfg &&
            valueRange == other.valueRange;
    };

    bool operator!=(const SensorMappingCfg& other) const {
        return !(*this == other);
    };

    static SensorMappingCfg deserialize(JsonObject& json) {
        SensorMappingCfg l;
        l.sensorName = json["sensor"].as<std::string>();
        if (json.containsKey("dmx")) {
            l.dmxCfg = DmxCfg::deserialize(json["dmx"].as<std::string>());
        }
        if (json.containsKey("value_range")) {
            JsonObject jsonVr = json["value_range"].as<JsonObject>();
            l.valueRange = ValueRange::deserialize(jsonVr);
        }
        return l;
    };

    static void serialize(JsonObject& json, const SensorMappingCfg& l) {
        json["sensor"] = l.sensorName;
        json["dmx"] = DmxCfg::serialize(l.dmxCfg);
        JsonObject jsonVr = json["value_range"].to<JsonObject>();
        ValueRange::serialize(jsonVr, l.valueRange);
    };
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
    
    std::vector<SensorMappingCfg> sensorMappings; // sensor to dmx local mappings

    std::vector<PluginCfg> plugins;

    bool lightsTest;
    std::uint16_t maxIdle; // max idle time in min, 0 means no sleep
    unsigned int rebootAfterWifiFailed = 15; // reboot after 15 failed wifi connections, 0 means no reboot
    bool disableWifiPowerSave;
    bool disableArtnet = false;
    int logLevel = 2; // default to INFO level (-1=SILENT, 0=ERROR, 1=WARNING, 2=INFO, 3=TRACE)

    MqttCfg mqtt;

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
            disableArtnet == other.disableArtnet &&
            logLevel == other.logLevel &&
            mqtt == other.mqtt &&

            pwms == other.pwms &&
            rgbwStrips == other.rgbwStrips &&
            rgbStrips == other.rgbStrips &&
            servos == other.servos &&

            humTemps == other.humTemps &&
            touchSensors == other.touchSensors &&
            digitalReadSensors == other.digitalReadSensors &&
            analogReadSensors == other.analogReadSensors &&

            sensorMappings == other.sensorMappings &&

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
        if (json.containsKey("disable_artnet")) {
            s.disableArtnet = json["disable_artnet"].as<bool>();
        } else {
            s.disableArtnet = false;
        }
        if (json.containsKey("log_level")) {
            s.logLevel = json["log_level"].as<int>();
        } else {
            s.logLevel = 2; // default to INFO
        }
        if (json.containsKey("mqtt")) {
            JsonObject jsonMqtt = json["mqtt"].as<JsonObject>();
            s.mqtt = MqttCfg::deserialize(jsonMqtt);
        } else {
            s.mqtt = MqttCfg();
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

        // sensor mappings
        JsonArray sensorMappingsArray = json["sensor_mappings"].as<JsonArray>();
        for (JsonVariant v : sensorMappingsArray) {
            JsonObject jsonSensorMapping = v.as<JsonObject>();
            s.sensorMappings.push_back(SensorMappingCfg::deserialize(jsonSensorMapping));
        }

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
        json["disable_artnet"] = disableArtnet;
        json["log_level"] = logLevel;

        if (mqtt.server != "") {
            JsonObject jsonMqtt = json["mqtt"].to<JsonObject>();
            MqttCfg::serialize(jsonMqtt, mqtt);
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

        // sensor mappings
        if (sensorMappings.size() > 0) {
            JsonArray sensorMappings = json["sensor_mappings"].to<JsonArray>();
            for (auto sensorMapping : this->sensorMappings) {
                JsonObject jsonSensorMapping = sensorMappings.add<JsonObject>();
                SensorMappingCfg::serialize(jsonSensorMapping, sensorMapping);
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
