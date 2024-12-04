#pragma once

#define _TASK_OO_CALLBACKS

#include <functional>
#include <Arduino.h>
#include <TaskScheduler.h>
#include <ArduinoLog.h>
#include <NeoPixelBus.h>
#include <Things.h>


class Animation: private Task {
  private:
    bool repeat;
    int frames = 0;
    uint8_t frameRate;
    /**
     * Duration is set before the animation starts.
     * It must be constant during the animation because it is used to calculate the number of frames (progress).
     */
    unsigned int nextDuration;
    std::function<void()> onEnd;
  
  protected:
      String name;

  public:
    Animation(Scheduler* aScheduler, bool repeat = true, unsigned long frameRate = 50/*Hz*/):
        repeat(repeat),
        frameRate(frameRate),
          Task(1000 / frameRate, 0, aScheduler, false) {
    }

    virtual void onStart() {}
    virtual void animate() {}
    
    void restart() {
        frames = nextDuration * frameRate / 1000; // store total iterations, function is returning remaining iterations
        setIterations(frames);
        Log.traceln("Staring animation with duration: %d, refreshRate: %d, interval: %d, frames: %d, repeat: %d", nextDuration, frameRate, getInterval(), frames, repeat);
        Task::restart();
    }

    bool OnEnable() {
        onStart();
        return true;
    }

    void onDisable() {
        onEnd();
    }

    bool Callback() {
        animate();
        if (isLastIteration()) {
            onEnd();
            if (repeat) {
                restart();
            }
            return false;
        }
        return true;
    }

    /**
     * Get the progress of the animation as a float between 0 and 1.
     * 
     * frames = 5
     * iterations = 5
     * 
     * iterations:
     * 4 -> progress = 0
     * 3
     * 2
     * 1
     * 0 -> progress = 1
     */
    float getProgress() {
        long currentIteration = frames - 1 - getIterations(); // 1st current iteration is 0
        float progress = (float)currentIteration / (float)(frames - 1);
        // Serial.println(String("Animation [") + name + "] Progress: " + progress + ", currentIteration: " + currentIteration);
        return progress;
    }

    void setRepeat(bool repeat) {
        if (repeat && !isEnabled()) {
            restart();
        }
        this->repeat = repeat;
    }

    void setDuration(unsigned int duration) {
        nextDuration = duration;
    }

    void setOnEnd(std::function<void()> onEnd) {
        this->onEnd = onEnd;
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
            bool repeat = false,
            String name = "Tail Ani"):
        line(line),
        tailLength(tailLength),
        direction(direction),
        Animation(aScheduler, repeat) {
            this->name = name;

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
        Serial.println(String("[") + name + "] New head position: " + headPosition + ", previousHeadPosition: " + previousHeadPosition + ", headJump: " + headJump + ", effectivetail: " + effectivetail + " progress: " + getProgress());
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
                Serial.println(String("[") + name + "] Setting color " + color.R + "-" + color.G + "-" + color.B + ", i: " + i + ", blendFactor: " + blendFactor + ", position: " + (headPosition - i));
            }
            line->setColor(headPosition - i, color);
        }
        previousHeadPosition = headPosition;
    }

  public:
    void onStart() {
        previousHeadPosition = 0;
        reachedEndCalled = false;
    }

    void animate() {
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
            // if (firstColor) {
            //     currentColor = color1;
            //     newColor = color2;
            // } else {
            //     currentColor = color2;
            //     newColor = color1;
            // }
            if (firstColor) {
                currentColor = newColor;
                newColor = color1;
            } else {
                currentColor = newColor;
                newColor = color2;
            }
        }
        float blendFactor = getProgress();
        line->setColor(RgbColor::LinearBlend(currentColor, newColor, blendFactor));
    }

    void setColor1(RgbColor color1) {
        this->color1 = color1;
    }

    void setColor2(RgbColor color2) {
        this->color2 = color2;
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

    public:
        Wave(
            Scheduler* aScheduler,
            std::vector<RgbThing*> lines,
            unsigned int maxFadeTimeMillis):
            maxFadeTimeMillis(maxFadeTimeMillis) {

            for (auto& line : lines) {
                FadeAnimation* fadeAnimation = new FadeAnimation(aScheduler, line, 100); //TODO hold
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
            return 7; // 2x3 for color and 1 for fade time
        }

        void setData(uint8_t* data) {
            auto color1 = RgbColor(data[0], data[1], data[2]);
            auto color2 = RgbColor(data[3], data[4], data[5]);
            for (auto& fade : fades) {
                fade->setColor1(color1);
                fade->setColor2(color2);
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