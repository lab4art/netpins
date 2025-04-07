#pragma once

#define _TASK_OO_CALLBACKS

#include <Arduino.h>
#include <TaskScheduler.h>
#include <ArduinoLog.h>
#include <NeoPixelBus.h>
#include <Things.h>

class AnimationTask: private Task {
    private:
        std::function<void()> onFrame = []() {};

    public:
        AnimationTask(
            unsigned int framerate,
            Scheduler* scheduler):
            Task(1000 / framerate, -1, scheduler, true) {
        }

        bool Callback() {
            onFrame();
            return true;
        }

        void setOnFrame(std::function<void()> onFrame) {
            this->onFrame = onFrame;
        }

};

class Animation {
  private:
    AnimationTask task;
    bool repeat;
    uint8_t frameRate;
    unsigned long frames = 0;
    unsigned long remainingFrames = 0;
    
    /**
     * Duration is set before the animation starts.
     * It must be constant during the animation because it is used to calculate the number of frames (progress).
     */
    // unsigned int nextDuration = 0;
    std::function<void()> onEnd = []() {};
    std::function<void()> onStart = []() {};
  
  protected:

  public:
    Animation(Scheduler* aScheduler, bool repeat = true, unsigned int frameRate = 50/*Hz*/):
            repeat(repeat),
            frameRate(frameRate),
            task(frameRate, aScheduler) {
        task.setOnFrame([this]() {
            if (remainingFrames == frames) {
                onStart();
            }
            if (remainingFrames > 0) {
                // Log.traceln("Remaining frames: %d, progress: %s", remainingFrames, String(getProgress(), 4));
                this->animate();
                this->remainingFrames--;
            }
            if (remainingFrames == 0) {
                onEnd();
                if (this->repeat) {
                    restart();
                }
            }
        });
    }

    virtual void animate() {}
    
    void restart(float progress = 0.0f) {
        // frames = nextDuration * frameRate / 1000; // store total iterations, function is returning remaining iterations
        remainingFrames = frames * (1 - progress);
        Log.traceln("Staring animation with refreshRate: %d, frames: %d, remaining frames: %d, repeat: %d", frameRate, frames, remainingFrames, repeat);
    }

    /**
     * Get the progress of the animation as a float between 0 and 1.
     * 
     * frames = 5
     * remainingFrames = 5
     * 
     * remainingFrames:
     * 5 -> progress = 0 (start)
     * 4
     * 3
     * 2
     * 1 -> progress = 1 (end)
     * 
     * frame - time - value
     * 0 - 0.0 - 0 
     * 1 - 0.2 - 0.25
     * 2 - 0.4 - 0.5
     * 3 - 0.6 - 0.75
     * 4 - 0.8 - 1
     */
    float getProgress() {
        float delta = 1.0f / ((float)frames - 1.0f); // one frame less to caltulate delta, to get a value between 0 and 1
        return 1.0f - (float)remainingFrames * delta + delta; 
    }

    void setDuration(unsigned int duration) {
        frames = duration * frameRate / 1000; // store total iterations, function is returning remaining iterations
    }

    bool isRunning() {
        return getProgress() > 0.0f && getProgress() < 1.0f;
    }

    void setRepeat(bool repeat) {
        if (repeat && getProgress() == 1) {
            restart();
        }
        this->repeat = repeat;
    }

    void setOnEnd(std::function<void()> onEnd) {
        this->onEnd = onEnd;
    }
};

class PWMFadeAnimation: public Animation {
    private:
        LedThing* led;
        uint8_t value1; // value to fade from (off)
        uint8_t value2; // value to fade to (on)
        boolean fadeInMode = false; // if false, fadeOut

        uint8_t linearBlend(uint8_t left, uint8_t right, float progress) {
            return left + (right - left) * progress;
        }

        uint8_t getCurrentValue() {
            if (fadeInMode) {
                return linearBlend(value1, value2, getProgress());
            } else {
                return linearBlend(value2, value1, getProgress());
            }
        }

    public:
        PWMFadeAnimation(
            Scheduler* aScheduler, 
            LedThing* led):
            Animation(aScheduler, false),
            led(led) {
        }

        void animate() {
            uint8_t data[1] = {getCurrentValue()};
            // Log.traceln("Setting PWM fade animation data: %d", data[0]);
            led->setData(data);
        }

        void setValue1(uint8_t value) {
            value1 = value;
            if (!fadeInMode && !isRunning()) {
                uint8_t data[1] = {value};
                led->setData(data);
            }
        }

        void setValue2(uint8_t value) {
            value2 = value;
            if (fadeInMode && !isRunning()) {
                uint8_t data[1] = {value};
                led->setData(data);
            }
        }

        void fadeIn(std::uint16_t fadeInDuration) {
            if (!isRunning()) {
                Log.traceln("Fresh Fade in to: %d, duration: %d, progress: %s.", this->value2, fadeInDuration, String(getProgress(), 4));
                this->fadeInMode = true;
                setDuration(fadeInDuration);
                restart();
            } else if (!fadeInMode) {
                Log.traceln("Middle Fade in to: %d, duration: %d, progress: %s.", this->value2, fadeInDuration, String(getProgress(), 4));
                float currentProgress = getProgress();
                this->fadeInMode = true;
                setDuration(fadeInDuration * getProgress());
                restart(1 - currentProgress);
            }
        }
        void fadeOut(std::uint16_t fadeOutDuration) {
            if (!isRunning()) {
                Log.traceln("Fresh Fade out to: %d, duration: %d, progress: %s.", this->value1, fadeOutDuration, String(getProgress(), 4));
                this->fadeInMode = false;
                setDuration(fadeOutDuration);
                restart();
            } else if (fadeInMode) {
                Log.traceln("Middle Fade out to: %d, duration: %d, progress: %s.", this->value1, fadeOutDuration, String(getProgress(), 4));
                float currentProgress = getProgress();
                this->fadeInMode = false;
                setDuration(fadeOutDuration);
                restart(1 - currentProgress);
            }
        }

        void togleFade(
                std::uint16_t fadeInDuration,
                std::uint16_t fadeOutDuration) {
            if (fadeInMode) {
                fadeOut(fadeOutDuration);
            } else {
                fadeIn(fadeInDuration);
            }
        }
};

/**
 * Animation that moves a single pixel along a line, a tail is left behind.
 */
class TailAnimation: public Animation {
  public:
    enum Direction {
        RIGHT,
        LEFT
    };

  private:
    RgbThing* line;
    RgbColor color1;
    RgbColor color2;
    int tailLength;
    Direction direction;

    int previousHeadPosition = 0;
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
        tailLength(tailLength),
        direction(direction),
        Animation(aScheduler, repeat) {
    }

  private:
    void moveRight() { 
        // define a head based on the progress of the animation
        int headPosition = getProgress() * (line->size() + tailLength);
        if (!reachedEndCalled && headPosition >= line->size()) {
            if (onHeadReachedEnd) {
                onHeadReachedEnd();
                reachedEndCalled = true;
            }
        }
        // Serial.println(String("[") + name + "] New head position: " + headPosition + ", progress: " + getProgress());
        // Serial.println(String("[") + name + "] New head position: " + headPosition);

        // draw tail as fade of color1 to color2
        // at hight speeds the head can jump over multiple pixels, calculate the effective tail length, not to leave behind color1 pixels
        int headJump = headPosition - previousHeadPosition;
        u_int32_t effectivetail = tailLength + headJump;
        // for (int i = 0; i <= effectivetail; i++) {
        for (int i = 0; i < effectivetail; i++) {
            if (headPosition - i < 0 || headPosition - i >= line->size()) {
                continue;
            }
            RgbColor color;
            if (i > tailLength) {
                color = color2;
            } else {
                // blend factor normalized to 0-1
                float blendFactor = (float)i / (float)effectivetail;
                color = RgbColor::LinearBlend(color1, color2, blendFactor);
            }
            line->setColor(headPosition - i, color);
            // Serial.println(String("Setting color ") + color.R + "-" + color.G + "-" + color.B + ", i: " + i + ", blendFactor: " + blendFactor + ", position: " + (headPosition - i));
        }
        previousHeadPosition = headPosition;
    }

    void moveLeft() { 
        // define a head based on the progress of the animation
        int headPosition = (1 - getProgress()) * (line->size() + tailLength);
        if (!reachedEndCalled && headPosition <= 0) {
            if (onHeadReachedEnd) {
                onHeadReachedEnd();
                reachedEndCalled = true;
            }
        }
        // Serial.println(String("[") + name + "] New head position: " + headPosition + ", progress: " + getProgress());

        // draw tail as fade of color1 to color2
        // at hight speeds the head can jump over multiple pixels, calculate the effective tail length, not to leave behind color1 pixels
        int headJump = headPosition - previousHeadPosition;
        u_int32_t effectivetail = tailLength + headJump;
        for (int i = 0; i < effectivetail; i++) {
            if (headPosition + i < 0 || headPosition + i >= line->size()) {
                continue;
            }
            RgbColor color;
            if (i > tailLength) {
                color = color2;
            } else {
                // blend factor normalized to 0-1
                float blendFactor = (float)i / (float)effectivetail;
                color = RgbColor::LinearBlend(color1, color2, blendFactor);
            }
            line->setColor(headPosition + i, color);
        }
        previousHeadPosition = headPosition;
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
            line->setColor(headPosition - i, color);
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
        this->tailLength = tailLength;
    }
};

class FadeAnimation: public Animation {

  private:
    RgbThing* line;
    RgbColor color1;
    RgbColor color2;
    uint8_t dimm = 255;
    RgbColor currentColor;
    RgbColor newColor;
    bool firstColor;

    unsigned int fadeTimeMillis; // TODO hold

  public:
    FadeAnimation(
        Scheduler* aScheduler,
        RgbThing* line,
        unsigned int fadeTimeMillis,
        bool repeat = false): 
          Animation(aScheduler, repeat),
          line(line),
          fadeTimeMillis(fadeTimeMillis) { 
        color1 = RgbColor(0,0,0);
        color2 = RgbColor(0,0,0);
        newColor = RgbColor(0,0,0);
        currentColor = RgbColor(0,0,0);
    }
    
    void animate() {
        if (getProgress() == 0) {
            if (firstColor) {
                currentColor = newColor;
                newColor = color1;
            } else {
                currentColor = newColor;
                newColor = color2;
            }
        }
        float blendFactor = getProgress();
        line->setColor(RgbColor::LinearBlend(currentColor, newColor, blendFactor), dimm);
    }

    void setColor1(RgbColor color1) {
        this->color1 = color1;
    }

    void setColor2(RgbColor color2) {
        this->color2 = color2;
    }

    void setDimm(uint8_t dimm) {
        this->dimm = dimm;
    }

    void setFirstColor(bool isFfirstColor) {
        firstColor = isFfirstColor;
    }
};

class Wave : public Thing {

    private:
        std::vector<FadeAnimation*> fades;
        int current = 0;
        unsigned int maxFadeTimeMillis;
        bool firstColor = true;
        bool dimmable = false;

    public:
        Wave(
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
            fades[0]->restart();
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
                if (fadeTime < 100) { // fade time 0 prevets the animation to start
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
};
