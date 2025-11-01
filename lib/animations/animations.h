#pragma once

#include "config.h"
#include <Arduino.h>
#include <ArduinoLog.h>
#include <NeoPixelBus.h>
#include <Things.h>
#include <settings.h>
#include <scheduler.h>

class AnimationTask: public ScheduledTask {
    private:
        std::function<void()> onFrame = []() {};

    public:
        AnimationTask(unsigned int framerate):
            ScheduledTask(1000 / framerate) {
        }

        void callback() override {
            onFrame();
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
    Animation(bool repeat = true, unsigned int frameRate = 50/*Hz*/):
            repeat(repeat),
            frameRate(frameRate) {
    }

    virtual void animate() {}

    void schedule(Scheduler* scheduler) {
        AnimationTask* task = new AnimationTask(1000 / frameRate);
        task->setOnFrame([this]() {
            // Log.traceln("Animation frame: %d, remaining frames: %d, progress: %s", frames, remainingFrames, String(getProgress(), 4));
            if (remainingFrames == frames) {
                onStart();
            }
            if (remainingFrames > 0) {
                // Log.traceln("Remaining frames: %d, progress: %s", remainingFrames, String(getProgress(), 4));
                this->animate();
                // Log.traceln("Animated.");
                this->remainingFrames--;
            }
            if (remainingFrames == 0) {
                onEnd();
                if (this->repeat) {
                    restart();
                    Log.traceln("Animation restarted.");
                }
            }
        });
        scheduler->addTask(task);
    }

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
        // restart(getProgress());
    }

    unsigned int getDuration() {
        return frames * 1000 / frameRate; // return duration in ms
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


