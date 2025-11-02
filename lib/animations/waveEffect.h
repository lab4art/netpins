#pragma once

#include <Things.h>
#include <animations.h>
#include <settings.h>
#include <netpinsCommons.h>

struct WaveEffectCfg {
    std::string rgbStripName;
    DmxCfg dmxCfg;
    std::uint16_t maxFadeTime = 10000; // ms, maximum fade time
    bool dimmable = false;

    bool operator==(const WaveEffectCfg& other) const {
        return rgbStripName == other.rgbStripName &&
            dmxCfg == other.dmxCfg &&
            maxFadeTime == other.maxFadeTime &&
            dimmable == other.dimmable;
    }

    bool operator!=(const WaveEffectCfg& other) const {
        return !(*this == other);
    }

    static WaveEffectCfg deserialize(std::string jsonString) {
        WaveEffectCfg w;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        if (error) {
            Serial.println("Failed to deserialize WaveEffectCfg");
            return w; // Return default config on error
        }
        JsonObject json = doc.as<JsonObject>();
        w.rgbStripName = json["rgb_strip_name"].as<std::string>();
        if (json.containsKey("max_fade_time")) {
            w.maxFadeTime = json["max_fade_time"].as<std::uint16_t>();
        }
        if (json.containsKey("dmx")) {
            w.dmxCfg = DmxCfg::deserialize(json["dmx"].as<std::string>());
        }
        if (json.containsKey("max_fade_time")) {
            w.maxFadeTime = json["max_fade_time"].as<std::uint16_t>();
        }
        if (json.containsKey("dimmable")) {
            w.dimmable = json["dimmable"].as<bool>();
        }
        return w;
    }

    static void serialize(JsonObject& jsonWave, const WaveEffectCfg& w) {
        jsonWave["rgb_strip_name"] = w.rgbStripName;
        jsonWave["dmx"] = DmxCfg::serialize(w.dmxCfg);
        jsonWave["max_fade_time"] = w.maxFadeTime;
        jsonWave["dimmable"] = w.dimmable;
    }
};

class WaveEffect : public Thing {

    private:
        std::vector<FadeAnimation*> fades;
        uint16_t current = 0;
        uint16_t maxFadeTimeMillis;
        bool firstColor = true;
        bool dimmable = false;
        unsigned int frameRate;

    public:
        WaveEffect(
                std::vector<RgbThing*> lines,
                uint16_t maxFadeTimeMillis,
                bool dimmable = false,
                unsigned int frameRate = 50):
            maxFadeTimeMillis(maxFadeTimeMillis),
            dimmable(dimmable),
            frameRate(frameRate) {

            for (auto line : lines) {
                FadeAnimation* fadeAnimation = new FadeAnimation(line, frameRate, false);
                if (line->isDimmable()) {
                    dimmable = true;
                }
                fadeAnimation->setName(std::string("Fade ") + line->getName().c_str());
                fadeAnimation->setDuration(maxFadeTimeMillis);
                fadeAnimation->setOnEnd([this](){
                    current++;
                    if (current >= fades.size()) {
                        current = 0;
                        firstColor = !firstColor;
                    }
                    
                    Log.traceln("Restarting wave fade: %s, firstColor: %d", fades[current]->getName().c_str(), firstColor);
                    fades[current]->setFirstColor(firstColor);
                    fades[current]->restart();
                });
                Log.noticeln("Adding fade animation %s for line: %s", fadeAnimation->getName().c_str(), line->getName().c_str());
                fades.push_back(fadeAnimation);
            }
        }

        void schedule(Scheduler* scheduler) {
            for (auto& fade : fades) {
                fade->schedule(scheduler);
            }
        }

        virtual ~WaveEffect() {
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
                auto minFadeTime = 2000 / frameRate; // 2x frame time to allow at least 2 frames
                if (fadeTime < minFadeTime) {
                    fadeTime = minFadeTime;
                }
                // Log.noticeln("Setting fade time: %d from input %d", fadeTime, data[6]);
                fade->setDuration(fadeTime);
            }
        }

        void setMaxFadeTime(unsigned int maxFadeTime) {
            this->maxFadeTimeMillis = maxFadeTime;
        }

        void restart() {
            Log.traceln("Restarting wave effect, total fades: %d", fades.size());
            current = 0;
            if (!fades.empty()) {
                fades[0]->restart();
            }
        }
};