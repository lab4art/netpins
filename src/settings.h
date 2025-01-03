#pragma once

#include <string>
#include <vector>
#include <ArduinoJson.h>
#include <Preferences.h>

struct StripeCfg {
    std::uint8_t pin;
    std::uint16_t size;
    bool dimmer;
    // first pixel of each slice
    std::vector<std::uint16_t> slices;

    bool operator==(const StripeCfg& other) const {
        return pin == other.pin &&
            size == other.size &&
            dimmer == other.dimmer &&
            slices == other.slices;
    }

    bool operator!=(const StripeCfg& other) const {
        return !(*this == other);
    }

    static StripeCfg deserialize(JsonObject& json) {
        StripeCfg s;
        s.pin = json["pin"].as<std::uint8_t>();
        s.size = json["size"].as<std::uint16_t>();
        if (json.containsKey("dimmer")) { // backward compatibility
            s.dimmer = json["dimmer"].as<bool>();
        } else {
            s.dimmer = false;
        }
        JsonArray slicesArray = json["slices"].as<JsonArray>();
        for (JsonVariant v : slicesArray) {
            auto slice = v.as<std::uint16_t>();
            s.slices.push_back(slice);
        }
        return s;
    }

    static void serialize(JsonObject& jsonStripe, const StripeCfg& s) {
        jsonStripe["pin"] = s.pin;
        jsonStripe["size"] = s.size;
        jsonStripe["dimmer"] = s.dimmer;
        JsonArray slices = jsonStripe["slices"].to<JsonArray>();
        for (auto slice : s.slices) {
            slices.add(slice);
        }
    }
};

struct WaveCfg {
    // used to calculate fade time from 8bit input
    std::uint32_t maxFadeTime = 10000;
    // index number of the rgb slices that are part of the wave. Fist slice defined in the config has index 0
    std::vector<uint8_t> sliceIndexes;

    bool operator==(const WaveCfg& other) const {
        return maxFadeTime == other.maxFadeTime &&
            sliceIndexes == other.sliceIndexes;
    }

    bool operator!=(const WaveCfg& other) const {
        return !(*this == other);
    }

    static WaveCfg deserialize(JsonObject& json) {
        WaveCfg w;
        w.maxFadeTime = json["max_fade_time"].as<std::uint32_t>();
        JsonArray sliceIndexesArray = json["slice_indexes"].as<JsonArray>();
        for (JsonVariant v : sliceIndexesArray) {
            auto sliceIndex = v.as<std::uint8_t>();
            w.sliceIndexes.push_back(sliceIndex);
        }
        return w;
    }

    static void serialize(JsonObject& jsonWave, const WaveCfg& w) {
        jsonWave["max_fade_time"] = w.maxFadeTime;
        JsonArray sliceIndexes = jsonWave["slice_indexes"].to<JsonArray>();
        for (auto sliceIndex : w.sliceIndexes) {
            sliceIndexes.add(sliceIndex);
        }
    }
};

struct ServoCfg {
    std::uint8_t pin;
    std::uint8_t maxAngle;
    std::uint16_t minPulseWidth = 0;
    std::uint16_t maxPulseWidth = 0;

    bool operator==(const ServoCfg& other) const {
        return pin == other.pin &&
            maxAngle == other.maxAngle &&
            minPulseWidth == other.minPulseWidth &&
            maxPulseWidth == other.maxPulseWidth;
    }

    bool operator!=(const ServoCfg& other) const {
        return !(*this == other);
    }

    static ServoCfg deserialize(JsonObject& json) {
        ServoCfg s;
        s.pin = json["pin"].as<std::uint8_t>();
        s.maxAngle = json["max_angle"].as<std::uint8_t>();
        if (json.containsKey("min_pulse_width")) {
            s.minPulseWidth = json["min_pulse_width"].as<std::uint16_t>();
        }
        if (json.containsKey("max_pulse_width")) {
            s.maxPulseWidth = json["max_pulse_width"].as<std::uint16_t>();
        }
        return s;
    }

    static void serialize(JsonObject& jsonServo, const ServoCfg& s) {
        jsonServo["pin"] = s.pin;
        jsonServo["max_angle"] = s.maxAngle;
        if (s.minPulseWidth != 0) {
            jsonServo["min_pulse_width"] = s.minPulseWidth;
        }
        if (s.maxPulseWidth != 0) {
            jsonServo["max_pulse_width"] = s.maxPulseWidth;
        }
    }
};

struct Settings {
    std::string wifiSsid;
    std::string wifiPass;
    std::string hostname;
    std::uint32_t hbInt;
    std::uint16_t udpPort;

    std::vector<std::uint8_t> leds;
    std::vector<StripeCfg> rgbwStrips;
    std::vector<StripeCfg> rgbStrips;
    std::vector<ServoCfg> servos;
    // waves
    // stripes that are part of the animation must be excluded from the dmx listener
    std::vector<WaveCfg> waves;

    bool lightsTest;
    std::uint16_t maxIdle; // max idle time in min, 0 means no sleep
    bool enableDmxStore;

    bool operator==(const Settings& other) const {
        return wifiSsid == other.wifiSsid &&
            wifiPass == other.wifiPass &&
            hostname == other.hostname &&
            hbInt == other.hbInt &&
            udpPort == other.udpPort &&
            leds == other.leds &&
            rgbwStrips == other.rgbwStrips &&
            rgbStrips == other.rgbStrips &&
            servos == other.servos &&
            waves == other.waves &&
            lightsTest == other.lightsTest &&
            maxIdle == other.maxIdle &&
            enableDmxStore == other.enableDmxStore;
    }

    bool operator!=(const Settings& other) const {
        return !(*this == other);
    }

    static void deserialize(Settings& s, JsonDocument& json) {
        s.wifiSsid = json["wifi_ssid"].as<std::string>();
        s.wifiPass = json["wifi_pass"].as<std::string>();
        s.hostname = json["hostname"].as<std::string>();
        s.hbInt = json["hb_int"].as<std::uint32_t>();
        s.udpPort = json["udp_port"].as<std::uint16_t>();
        s.lightsTest = json["lights_test"].as<bool>();
        s.maxIdle = json["max_idle"].as<std::uint16_t>();
        s.enableDmxStore = json["enable_dmx_store"].as<bool>();

        JsonArray ledsArray = json["leds"].as<JsonArray>();
        for (JsonVariant v : ledsArray) {
            s.leds.push_back(v.as<std::uint8_t>());
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

        JsonArray wavesArray = json["waves"].as<JsonArray>();
        for (JsonVariant v : wavesArray) {
            JsonObject jsonWave = v.as<JsonObject>();
            s.waves.push_back(WaveCfg::deserialize(jsonWave));
        }
    };

    void serialize(JsonDocument& json) {
        json["wifi_ssid"] = wifiSsid;
        json["wifi_pass"] = wifiPass;
        json["hostname"] = hostname;
        json["hb_int"] = hbInt;
        json["udp_port"] = udpPort;
        json["lights_test"] = lightsTest;
        json["max_idle"] = maxIdle;
        json["enable_dmx_store"] = enableDmxStore;

        JsonArray jsonLeds = json["leds"].to<JsonArray>();
        for (auto led : this->leds) {
            jsonLeds.add(led);
        }

        JsonArray jsonRgbw = json["rgbw_strips"].to<JsonArray>();
        for (auto stripe : rgbwStrips) {
            JsonObject jsonStripe = jsonRgbw.createNestedObject();
            StripeCfg::serialize(jsonStripe, stripe);
        }

        JsonArray jsonRgb = json["rgb_strips"].to<JsonArray>();
        for (auto stripe : rgbStrips) {
            JsonObject jsonStripe = jsonRgb.createNestedObject();
            StripeCfg::serialize(jsonStripe, stripe);
        }

        JsonArray servosArray = json["servos"].to<JsonArray>();
        for (auto servo : servos) {
            JsonObject jsonServo = servosArray.createNestedObject();
            ServoCfg::serialize(jsonServo, servo);
        }

        JsonArray waves = json["waves"].to<JsonArray>();
        for (auto wave : this->waves) {
            JsonObject jsonWave = waves.createNestedObject();
            WaveCfg::serialize(jsonWave, wave);
        }
    };

    String asJson() {
        // Serial.println("Serializing settings ...");
        JsonDocument jsonDoc;
        serialize(jsonDoc);
        String output;
        serializeJson(jsonDoc, output);
        return output;
    };

    void setDefaults() {
            this->wifiSsid = WIFI_SSID;
            this->wifiPass = WIFI_PASS;
            this->hostname = "";
            this->hbInt = 5000;
            this->udpPort = 5824;
            this->lightsTest = true;
            this->maxIdle = 0;
            this->enableDmxStore = false;
    };
};

struct DmxSettings {
    std::uint16_t universe;
    std::uint16_t channel;

    bool operator==(const DmxSettings& other) const {
        return universe == other.universe &&
            channel == other.channel;
    }

    bool operator!=(const DmxSettings& other) const {
        return !(*this == other);
    }

    static void deserialize(DmxSettings& s, JsonDocument& json) {
        s.universe = json["universe"].as<std::uint16_t>();
        s.channel = json["channel"].as<std::uint16_t>();
    };

    void serialize(JsonDocument& json) {
        json["universe"] = universe;
        json["channel"] = channel;
    };

    String asJson() { // TODO move to base class
        // Serial.println("Serializing settings ...");
        JsonDocument jsonDoc;
        serialize(jsonDoc);
        String output;
        serializeJson(jsonDoc, output);
        return output;
    };

    void setDefaults() {
        this->universe = 0;
        this->channel = 1;
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
            // TODO store last error for UI to display
            Serial.println(message.c_str());
        }

        void fromJsonDoc(JsonDocument& jsonDoc) {
            T newSettings;
            T::deserialize(newSettings, jsonDoc);
            if (newSettings != this->settings) {
                Serial.println("Settings changed.");
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
            String jsonStr = preferences.getString("json");
            Serial.println("Loaded settings json: " + jsonStr);

            JsonDocument json_doc;
            deserializeJson(json_doc, jsonStr);
            T::deserialize(this->settings, json_doc);
            preferences.end();
            dirty = false;

            Serial.println("Deserialized settings: ");
            // Serial.println(this->settings.toString());
        }

        void save() {
            if (!dirty) {
                return;
            }
            if (!preferences.begin(this->nspace.c_str(), false)) {
                onError(std::string("Error opening preferences with namespace: ") + this->nspace);
                return;
            }

            Serial.println("Saving settings: ");
            JsonDocument jsonDoc;
            this->settings.serialize(jsonDoc);
            String output;
            serializeJson(jsonDoc, output);

            size_t bytesWritten = preferences.putString("json", output);
            if (bytesWritten != output.length()) {
                onError(std::string("Error writing to preferences: ") + this->nspace);
            }
            preferences.end();
            dirty = false;
            Serial.println("Settings saved: " + output);
        }

        void fromJson(String jsonString) {
            // parse jsonString to jsonDoc
            JsonDocument jsonDoc;
            deserializeJson(jsonDoc, jsonString);

            fromJsonDoc(jsonDoc);
        }

        /**
         * Applies potentially partial json to current settings.
         * Only top level keys are merged, nested keys are replaced.
         */
        void mergeJson(String jsonString) {
            JsonDocument newJsonDoc;
            deserializeJson(newJsonDoc, jsonString);

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
            // T::setDefaults(this->settings);
            this->settings.setDefaults();
            dirty = true; // TODO set dirty only if settings changed
        }

        bool isDirty() {
            return dirty;
        }

};
