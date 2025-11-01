#pragma once

#include <Things.h>
#include <animations.h>
#include <settings.h>
#include <colorUtils.h>


struct TailAnimationCfg {
    std::string rgbStripName;
    RgbColor color1;
    RgbColor color2;
    std::uint8_t dimm = 255;
    std::uint16_t duration = 10000; // ms, duration of the animation
    std::uint16_t headLength;
    std::uint16_t tailLength;
    Direction direction;
    std::uint16_t speedUpStep = 100; // ms, how much to speed up the animation when moving
    std::uint16_t speedDownStep = 500; // ms, how much to slow down the animation when not moving
    std::uint16_t minDuration = 1000; // ms, minimum duration of the animation
    std::vector<RgbColor> colors;

    bool operator==(const TailAnimationCfg& other) const {
        return rgbStripName == other.rgbStripName &&
            color1 == other.color1 &&
            color2 == other.color2 &&
            dimm == other.dimm &&
            duration == other.duration &&
            headLength == other.headLength &&
            tailLength == other.tailLength &&
            direction == other.direction &&
            speedUpStep == other.speedUpStep &&
            speedDownStep == other.speedDownStep &&
            minDuration == other.minDuration &&
            colors == other.colors;
    }

    bool operator!=(const TailAnimationCfg& other) const {
        return !(*this == other);
    }

    static TailAnimationCfg deserialize(std::string jsonString) {
        TailAnimationCfg t;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        if (error) {
            Serial.println("Failed to deserialize TailAnimationCfg");
            return t; // Return default config on error //TODO remove, we want to fail not go back to the defaults
        }
        JsonObject json = doc.as<JsonObject>();
        t.rgbStripName = json["rgb_strip_name"].as<std::string>();
        t.color1 = ColorUtils::parseHexColor(json["color1"].as<std::string>());
        t.color2 = ColorUtils::parseHexColor(json["color2"].as<std::string>());
        t.dimm = json["dimm"].as<std::uint8_t>();
        t.duration = json["duration"].as<std::uint16_t>();
        t.headLength = json["head_length"].as<std::uint16_t>();
        t.tailLength = json["tail_length"].as<std::uint16_t>();
        if (json.containsKey("direction")) {
            std::string directionStr = json["direction"].as<std::string>();
            if (directionStr == "left") {
                t.direction = LEFT;
            } else {
                t.direction = RIGHT;
            }
        }
        if (json.containsKey("speed_up_step")) {
            t.speedUpStep = json["speed_up_step"].as<std::uint16_t>();
        }
        if (json.containsKey("speed_down_step")) {
            t.speedDownStep = json["speed_down_step"].as<std::uint16_t>();
        }
        if (json.containsKey("min_duration")) {
            t.minDuration = json["min_duration"].as<std::uint16_t>();
        }
        if (json.containsKey("colors")) {
            JsonArray colorsArray = json["colors"].as<JsonArray>();
            for (JsonVariant v : colorsArray) {
                auto colorStr = v.as<std::string>();
                t.colors.push_back(ColorUtils::parseHexColor(colorStr));
            }
        }
        return t;
    }

    static void serialize(JsonObject& jsonTail, const TailAnimationCfg& t) {
        jsonTail["rgb_strip_name"] = t.rgbStripName;
        jsonTail["color1"] = ColorUtils::toHexColor(t.color1);
        jsonTail["color2"] = ColorUtils::toHexColor(t.color2);
        jsonTail["dimm"] = t.dimm;
        jsonTail["duration"] = t.duration;
        jsonTail["head_length"] = t.headLength;
        jsonTail["tail_length"] = t.tailLength;
        jsonTail["direction"] = (t.direction == RIGHT) ? "right" : "left";
        jsonTail["speed_up_step"] = t.speedUpStep;
        jsonTail["speed_down_step"] = t.speedDownStep;
        jsonTail["min_duration"] = t.minDuration;
        JsonArray colors = jsonTail["colors"].to<JsonArray>();
        for (const auto& color : t.colors) {
            colors.add(ColorUtils::toHexColor(color));
        }
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
    int tailLength;
    int headLength = 0;
    Direction direction;

    int previousHeadPosition;
    bool reachedEndCalled = false;

    // callback function called when head Reached End
    std::function<void()> onHeadReachedEnd;

  public:
    TailAnimation(
            Scheduler* aScheduler, 
            RgbThing* line,
            Direction direction = RIGHT,
            bool repeat = false):
        line(line),
        direction(direction),
        Animation(aScheduler, repeat) {
            if (direction == RIGHT) {
                previousHeadPosition = 0;
            } else {
                previousHeadPosition = line->size() - 1;
            }
    }

  private:
    void moveRight() {
        int headPosition = (int)(getProgress() * line->size());
        if (!reachedEndCalled && getProgress() >= 1.0f) {
            if (onHeadReachedEnd) {
                onHeadReachedEnd();
                reachedEndCalled = true;
            }
        }

        for (int i = 0; i < line->size(); i++) {
            line->setColor(i, color2, dimm);
        }

        for (int i = 0; i <= tailLength; i++) {
            float blendFactor = (float)i / (float)tailLength;
            RgbColor color = RgbColor::LinearBlend(color1, color2, blendFactor);
            
            int pixelPos = (headPosition - i + line->size()) % line->size(); // Handle circular wrap
            line->setColor(pixelPos, color, dimm);
        }
    }

    void moveLeft() {
        int headPosition = (1 - getProgress()) * line->size();
        if (!reachedEndCalled && getProgress() >= 1.0f) {
            if (onHeadReachedEnd) {
                onHeadReachedEnd();
                reachedEndCalled = true;
            }
        }

        for (int i = 0; i < line->size(); i++) {
            line->setColor(i, color2, dimm);
        }

        for (int i = 0; i <= tailLength; i++) {
            float blendFactor = (float)i / (float)tailLength;
            RgbColor color = RgbColor::LinearBlend(color1, color2, blendFactor);
            
            int pixelPos = (headPosition + i + line->size()) % line->size(); // Handle circular wrap
            line->setColor(pixelPos, color, dimm);
        }
    }

    void fadeRight() { 
        // define a head based on the progress of the animation
        int headPosition = getProgress() * (line->size() + tailLength);
        if (!reachedEndCalled && headPosition >= line->size()) {
            if (onHeadReachedEnd) {
                onHeadReachedEnd();
                reachedEndCalled = true;
            }
        }

        // draw tail as fade of color1 to color2
        // at hight speeds the head can jump over multiple pixels, calculate the effective tail length, not to leave behind color1 pixels
        int headJump = headPosition - previousHeadPosition;
        u_int32_t effectivetail = tailLength + headJump;
        // Serial.println(String("[") + name + "] New head position: " + headPosition + ", previousHeadPosition: " + previousHeadPosition + ", headJump: " + headJump + ", effectivetail: " + effectivetail + " progress: " + getProgress());
        // for (int i = 0; i <= effectivetail; i++) {
        for (int i = 0; i < effectivetail; i++) { // TODO test this compared to ^
            if (headPosition - i < 0 || headPosition - i >= line->size()) {
                Log.warningln("Head position out of bounds: %d", headPosition - i);
                continue;
            }

            RgbColor color;
            if (i > tailLength) {
                color = color2;
            } else {
                // blend factor normalized to 0-1
                // blend factor peaking at middle of tail
                float blendFactor;
                /*
                0 -> 1
                1 -> 0.5
                2 -> 0
                3 -> 0.5
                4 -> 1
                 */
                if (i < tailLength / 2) {
                    blendFactor = 1.0f - (float)(i) / (float)(tailLength / 2);
                } else {
                    // blendFactor = (float)(i - tailLength / 2) / (float)(tailLength / 2);
                    blendFactor = (float)(i / (tailLength / 2)) - 1;
                }
                color = RgbColor::LinearBlend(color1, color2, blendFactor);
                // Serial.println(String("[") + name + "] Setting color " + color.R + "-" + color.G + "-" + color.B + ", i: " + i + ", blendFactor: " + blendFactor + ", position: " + (headPosition - i));
            }
            line->setColor(headPosition - i, color, dimm);
        }
        previousHeadPosition = headPosition;
    }

  public:

    void animate() {
        if (getProgress() == 0) {
            this->previousHeadPosition = 0;
            this->reachedEndCalled = false;
        }

        if (direction == RIGHT) {
            moveRight();
        } else {
            moveLeft();
        }
        // fadeRight();
    }

    void setOnHeadReachedEnd(std::function<void()> onHeadReachedEnd) {
        this->onHeadReachedEnd = onHeadReachedEnd;
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

    void setHeadLength(int headLength) {
        // TODO validate headLength
        this->headLength = headLength;
    }

    void setDimm(uint8_t dimm) {
        this->dimm = dimm;
    }
};

class TailAnimationThing: public Thing {
    private:
        TailAnimation* tailAnimation;
        unsigned int maxDuration;
    
    public:
        TailAnimationThing(
                Scheduler* aScheduler, 
                RgbThing* line, 
                int tailLength = 5,
                int maxDuration = 30000, 
                Direction direction = Direction::RIGHT,
                bool repeat = false) {
            tailAnimation = new TailAnimation(
                aScheduler, 
                line, 
                direction,
                repeat);
        }

        int numChannels() {
            return 8;
        }

        void setData(uint8_t* data) {
            // TODO set only if changed
            tailAnimation->setColor1(RgbColor(data[0], data[1], data[2]));
            tailAnimation->setColor2(RgbColor(data[3], data[4], data[5]));
            tailAnimation->setDuration(data[6]/255.0 * maxDuration);
            if (data[6] > 0) {
                tailAnimation->setRepeat(true);
            } else {
                tailAnimation->setRepeat(false);
            }
            tailAnimation->setTailLength(data[7]);
        }

};


