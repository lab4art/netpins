#pragma once

#include <Things.h>
#include <animations.h>
#include <settings.h>
#include <netpinsCommons.h>


struct TailAnimationCfg {
    std::string rgbStripName;
    DmxCfg dmxCfg;
    std::uint16_t maxDuration = 10000; // ms, duration of the animation
    Direction direction;

    bool operator==(const TailAnimationCfg& other) const {
        return rgbStripName == other.rgbStripName &&
            dmxCfg == other.dmxCfg &&
            maxDuration == other.maxDuration &&
            direction == other.direction;
    }

    bool operator!=(const TailAnimationCfg& other) const {
        return !(*this == other);
    }

    static TailAnimationCfg deserialize(std::string jsonString) {
        TailAnimationCfg t;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        if (error) {
            Log::error("Failed to deserialize TailAnimationCfg");
            return t; // Return default config on error //TODO remove, we want to fail not go back to the defaults
        }
        JsonObject json = doc.as<JsonObject>();
        t.rgbStripName = json["rgb_strip_name"].as<std::string>();
        t.dmxCfg = DmxCfg::deserialize(json["dmx"].as<std::string>());
        t.maxDuration = json["max_duration"].as<std::uint16_t>();
        if (json.containsKey("direction")) {
            std::string directionStr = json["direction"].as<std::string>();
            if (directionStr == "left") {
                t.direction = LEFT;
            } else {
                t.direction = RIGHT;
            }
        }
        return t;
    }

    static void serialize(JsonObject& jsonTail, const TailAnimationCfg& t) {
        jsonTail["rgb_strip_name"] = t.rgbStripName;
        jsonTail["dmx"] = DmxCfg::serialize(t.dmxCfg);
        jsonTail["max_duration"] = t.maxDuration;
        jsonTail["direction"] = (t.direction == RIGHT) ? "right" : "left";
    }
};

/**
 * Animation that moves a single pixel along a line, a tail is left behind.
 */
class TailAnimation: public Animation {

  private:
    RgbThing* line;
    RgbColor color1;
    RgbColor color2;
    uint8_t dimm = 255;
    int tailLength = 1;
    int headLength = 0;
    Direction direction;

  public:
    TailAnimation(
            RgbThing* line,
            Direction direction,
            bool repeat):
        line(line),
        direction(direction),
        Animation(repeat) {
    }
    
  private:
    void moveRight() {
        int headPosition = (int)(getProgress() * line->size());
        // Log.traceln("TailAnimation '%s' moving right. HeadPosition: %d, progress: %s", 
        //         getName().c_str(), headPosition, String(getProgress(), 4));

        for (int i = 0; i < line->size(); i++) {
            line->setColor(i, color2, dimm);
        }

        // Draw tail behind the head
        for (int i = 0; i <= tailLength; i++) {
            float blendFactor = (float)i / (float)tailLength;
            RgbColor color = RgbColor::LinearBlend(color1, color2, blendFactor);
            
            int pixelPos = (headPosition - i + line->size()) % line->size(); // Handle circular wrap
            line->setColor(pixelPos, color, dimm);
        }

        // Draw head fade-in ahead of the head
        if (headLength > 0) {
            for (int i = 1; i <= headLength; i++) {
                float blendFactor = (float)i / (float)headLength;
                RgbColor color = RgbColor::LinearBlend(color1, color2, blendFactor);
                
                int pixelPos = (headPosition + i) % line->size(); // Handle circular wrap
                line->setColor(pixelPos, color, dimm);
            }
        }
    }

    void moveLeft() {
        int headPosition = (1 - getProgress()) * line->size();
        // Log.traceln("TailAnimation '%s' moving left. HeadPosition: %d, progress: %s", 
        //         getName().c_str(), headPosition, String(getProgress(), 4));

        for (int i = 0; i < line->size(); i++) {
            line->setColor(i, color2, dimm);
        }

        // Draw tail behind the head
        for (int i = 0; i <= tailLength; i++) {
            float blendFactor = (float)i / (float)tailLength;
            RgbColor color = RgbColor::LinearBlend(color1, color2, blendFactor);
            
            int pixelPos = (headPosition + i + line->size()) % line->size(); // Handle circular wrap
            line->setColor(pixelPos, color, dimm);
        }

        // Draw head fade-in ahead of the head
        if (headLength > 0) {
            for (int i = 1; i <= headLength; i++) {
                float blendFactor = (float)i / (float)headLength;
                RgbColor color = RgbColor::LinearBlend(color1, color2, blendFactor);
                
                int pixelPos = (headPosition - i + line->size()) % line->size(); // Handle circular wrap
                line->setColor(pixelPos, color, dimm);
            }
        }
    }

  public:

    void animate() {
        if (direction == RIGHT) {
            moveRight();
        } else {
            moveLeft();
        }
    }

    void setColor1(RgbColor color1) {
        this->color1 = color1;
    }

    void setColor2(RgbColor color2) {
        this->color2 = color2;
    }

    void setTailLength(int tailLength) {
        // TODO validate tailLength
        this->tailLength = tailLength;
    }

    void setDimm(uint8_t dimm) {
        this->dimm = dimm;
    }

    void setHeadLength(int headLength) {
        // TODO validate headLength
        this->headLength = headLength;
    }
};

class TailAnimationThing: public Thing {
    private:
        TailAnimation* tailAnimation;
        unsigned int maxDuration;
        int currentValues[10] = {0};
    
    public:
        TailAnimationThing(
                TailAnimation* tailAnimation, 
                int maxDuration):
                tailAnimation(tailAnimation),
                maxDuration(maxDuration) {
        }

        int numChannels() {
            return 10;
        }

        /*
            * Data format:
            * [0-2] - color1 R,G,B
            * [3-5] - color2 R,G,B
            * [6] - dimm (0-255)
            * [7] - duration (0-255) mapped to (0 - maxDuration)
            * [8] - head length (in pixels)
            * [9] - tail length (in pixels)
        */
        void setData(uint8_t* data) {
            if (memcmp(currentValues, data, 10) == 0) {
                // Log::traceln("TailAnimationThing '%s' received same data, skipping update.", getName().c_str());
                return; // No change in data, skip update
            }
            unsigned int durationMs = (unsigned int)(data[7] / 255.0f * maxDuration + 0.5f);
            Log::traceln("TailAnimationThing '%s' updating with new data. Color1: %u %u %u, Color2: %u %u %u, Dimm: %u, Duration: %ums, HeadLength: %u, TailLength: %u", 
                getName().c_str(), (unsigned int)data[0], (unsigned int)data[1], (unsigned int)data[2],
                (unsigned int)data[3], (unsigned int)data[4], (unsigned int)data[5], (unsigned int)data[6],
                durationMs, (unsigned int)data[8], (unsigned int)data[9]);

            tailAnimation->setColor1(RgbColor(data[0], data[1], data[2]));
            tailAnimation->setColor2(RgbColor(data[3], data[4], data[5]));
            tailAnimation->setDimm(data[6]);
            tailAnimation->setDuration(data[7]/255.0 * maxDuration);
            tailAnimation->setHeadLength(data[8]);
            auto tailLength = data[9];
            if (tailLength < 1) {
                tailLength = 1;
            }
            tailAnimation->setTailLength(tailLength);

            // Update current values
            memcpy(currentValues, data, 10);
        }

};


