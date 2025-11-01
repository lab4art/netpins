#pragma once

#include <Things.h>
#include <animations.h>
#include <settings.h>
#include <colorUtils.h>


// struct WaveCfg {
//     // used to calculate fade time from 8bit input
//     std::uint32_t maxFadeTime = 10000;
//     // index number of the rgb slices that are part of the wave. Fist slice defined in the config has index 0
//     std::vector<uint8_t> sliceIndexes;

//     bool operator==(const WaveCfg& other) const {
//         return maxFadeTime == other.maxFadeTime &&
//             sliceIndexes == other.sliceIndexes;
//     }

//     bool operator!=(const WaveCfg& other) const {
//         return !(*this == other);
//     }

//     static WaveCfg deserialize(JsonObject& json) {
//         WaveCfg w;
//         w.maxFadeTime = json["max_fade_time"].as<std::uint32_t>();
//         JsonArray sliceIndexesArray = json["slice_indexes"].as<JsonArray>();
//         for (JsonVariant v : sliceIndexesArray) {
//             auto sliceIndex = v.as<std::uint8_t>();
//             w.sliceIndexes.push_back(sliceIndex);
//         }
//         return w;
//     }

//     static void serialize(JsonObject& jsonWave, const WaveCfg& w) {
//         jsonWave["max_fade_time"] = w.maxFadeTime;
//         JsonArray sliceIndexes = jsonWave["slice_indexes"].to<JsonArray>();
//         for (auto sliceIndex : w.sliceIndexes) {
//             sliceIndexes.add(sliceIndex);
//         }
//     }
// };



struct WaveAnimationCfg {
    std::string rgbStripName;
    RgbColor color1;
    RgbColor color2;
    std::uint8_t dimm = 255;
    std::uint16_t duration = 1000; // ms, duration of each wave step
    std::uint16_t maxFadeTime = 10000; // ms, maximum fade time
    bool dimmable = false;

    bool operator==(const WaveAnimationCfg& other) const {
        return rgbStripName == other.rgbStripName &&
            color1 == other.color1 &&
            color2 == other.color2 &&
            dimm == other.dimm &&
            duration == other.duration &&
            maxFadeTime == other.maxFadeTime &&
            dimmable == other.dimmable;
    }

    bool operator!=(const WaveAnimationCfg& other) const {
        return !(*this == other);
    }

    static WaveAnimationCfg deserialize(std::string jsonString) {
        WaveAnimationCfg w;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        if (error) {
            Serial.println("Failed to deserialize WaveAnimationCfg");
            return w; // Return default config on error
        }
        JsonObject json = doc.as<JsonObject>();
        w.rgbStripName = json["rgb_strip_name"].as<std::string>();
        w.color1 = ColorUtils::parseHexColor(json["color1"].as<std::string>());
        w.color2 = ColorUtils::parseHexColor(json["color2"].as<std::string>());
        w.dimm = json["dimm"].as<std::uint8_t>();
        w.duration = json["duration"].as<std::uint16_t>();
        if (json.containsKey("max_fade_time")) {
            w.maxFadeTime = json["max_fade_time"].as<std::uint16_t>();
        }
        if (json.containsKey("dimmable")) {
            w.dimmable = json["dimmable"].as<bool>();
        }
        return w;
    }

    static void serialize(JsonObject& jsonWave, const WaveAnimationCfg& w) {
        jsonWave["rgb_strip_name"] = w.rgbStripName;
        jsonWave["color1"] = ColorUtils::toHexColor(w.color1);
        jsonWave["color2"] = ColorUtils::toHexColor(w.color2);
        jsonWave["dimm"] = w.dimm;
        jsonWave["duration"] = w.duration;
        jsonWave["max_fade_time"] = w.maxFadeTime;
        jsonWave["dimmable"] = w.dimmable;
    }
};

/**
 * Wave animation that creates sequential fade effects across multiple RGB lines
 */
class WaveAnimation : public Thing {

    private:
        std::vector<FadeAnimation*> fades;
        int current = 0;
        unsigned int maxFadeTimeMillis;
        bool firstColor = true;
        bool dimmable = false;

    public:
        WaveAnimation(
            Scheduler* aScheduler,
            std::vector<RgbThing*> lines,
            unsigned int maxFadeTimeMillis):
            maxFadeTimeMillis(maxFadeTimeMillis) {

            for (auto& line : lines) {
                FadeAnimation* fadeAnimation = new FadeAnimation(aScheduler, line, 100); //TODO hold
                if (line->isDimmable()) {
                    dimmable = true;
                }
                fadeAnimation->setDuration(1000);
                fadeAnimation->setFirstColor(true);
                fadeAnimation->setOnEnd([this](){
                    current++;
                    if (current >= fades.size()) {
                        current = 0;
                        for (auto& fade : fades) {
                            fade->setFirstColor(firstColor);
                        }
                        firstColor = !firstColor;
                    }
                    Log.traceln("Restarting wave fade: %d", current);
                    fades[current]->restart();
                });
                // Log.noticeln("Adding fade animation: %d");
                fades.push_back(fadeAnimation);
            }
            // start the first fade
            if (!fades.empty()) {
                fades[0]->restart();
            }
        }

        virtual ~WaveAnimation() {
            for (auto* fade : fades) {
                delete fade;
            }
            fades.clear();
        }

        int numChannels() {
            // 3(RGB) x 2 + 1(dimmer) + 1 (fade time)
            return dimmable ? 8 : 7;
        }

        void setData(uint8_t* data) {
            auto color1 = RgbColor(data[0], data[1], data[2]);
            auto color2 = RgbColor(data[3], data[4], data[5]);
            for (auto& fade : fades) {
                fade->setColor1(color1);
                fade->setColor2(color2);
                if (dimmable) {
                    fade->setDimm(data[7]);
                }
                // set duration based on the 8bit input
                auto fadeTime = (data[6] * maxFadeTimeMillis) / 255;
                // Log.noticeln("Setting fade time: %d from input %d", fadeTime, data[6]);
                if (fadeTime < 100) { // fade time 0 prevents the animation to start
                    fadeTime = 100;
                }
                fade->setDuration(fadeTime);
            }
        }

        void setColor1(RgbColor color1) {
            for (auto& fade : fades) {
                fade->setColor1(color1);
            }
        }

        void setColor2(RgbColor color2) {
            for (auto& fade : fades) {
                fade->setColor2(color2);
            }
        }

        void setDimm(uint8_t dimm) {
            for (auto& fade : fades) {
                fade->setDimm(dimm);
            }
        }

        void setDuration(unsigned int duration) {
            for (auto& fade : fades) {
                fade->setDuration(duration);
            }
        }

        void setMaxFadeTime(unsigned int maxFadeTime) {
            this->maxFadeTimeMillis = maxFadeTime;
        }

        void restart() {
            current = 0;
            firstColor = true;
            if (!fades.empty()) {
                fades[0]->restart();
            }
        }
};