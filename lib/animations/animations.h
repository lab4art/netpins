#pragma once

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
            ScheduledTask(1000 / framerate, "AnimationTask", false) {
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
    AnimationTask* task = nullptr;
    std::string name = "";
    bool repeat;
    unsigned int frameRate;
    unsigned long frames = 0; // total number of frames for the animation
    unsigned long remainingFrames = 0;
    
    /**
     * Duration is set before the animation starts.
     * It must be constant during the animation because it is used to calculate the number of frames (progress).
     */
    // unsigned int nextDuration = 0;
    std::function<void()> onEnd = []() {};
    std::function<void()> onStart = []() {};
    bool onEndCalled = false;
  
  protected:
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
        if (frames == 0) {
            return 1.0f;
        }
        float delta = 1.0f / ((float)frames - 1.0f); // one frame less to caltulate delta, to get a value between 0 and 1
        return 1.0f - (float)remainingFrames * delta + delta; 
    }

    bool isFirstFrame() const {
        return remainingFrames == frames;
    }

  public:
    Animation(bool repeat = true, unsigned int frameRate = 50/*Hz*/):
            repeat(repeat),
            frameRate(frameRate) {
    }

    virtual void animate() {}

    void schedule(Scheduler* scheduler) {
        task = new AnimationTask(frameRate);
        task->setOnFrame([this]() {
            // Log.traceln("Animation total frames: %d, remaining frames: %d, progress: %s", frames, remainingFrames, String(getProgress(), 4));
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
                if (!onEndCalled) {
                    onEndCalled = true;
                    onEnd();
                    // Log.traceln("Animation '%s' ended.", name.c_str());
                }
                if (this->repeat) {
                    restart();
                    // Log.traceln("Animation '%s' restarted.", name.c_str());
                }
            }
        });
        scheduler->addTask(task);
    }

    void restart(float progress = 0.0f) {
        // Log.traceln("Restarting animation '%s' with total frames: %d at progress: %s", name.c_str(), frames, String(progress, 4));
        onEndCalled = false;
        remainingFrames = frames * (1 - progress);
        task->enable();
        // Log.traceln("Restarted animation '%s' with refreshRate: %d, frames: %d, remaining frames: %d, repeat: %d", 
        //         name.c_str(), frameRate, frames, remainingFrames, repeat);
    }

    void setDuration(unsigned int duration) {
        frames = duration * frameRate / 1000;
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

    void setName(std::string name) {
        this->name = name;
    }

    std::string getName() const {
        return name;
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
    bool firstColor; // TODO remove, should be generic fade, provide collor1 and color2 and potentially switch them

  public:
    FadeAnimation(
        RgbThing* line,
        unsigned int frameRate,
        bool repeat = false):
          Animation(repeat, frameRate),
          line(line) { 
        color1 = RgbColor(0,0,0);
        color2 = RgbColor(0,0,0);
        newColor = RgbColor(0,0,0);
        currentColor = RgbColor(0,0,0);
        firstColor = true;
    }
    
    void animate() {
        // Check if this is the first frame of the animation cycle
        if (isFirstFrame()) {
            // Log.traceln("FadeAnimation '%s' 1st frame. newColor: %d %d %d, color1: %d %d %d, color2: %d %d %d. Is firstColor: %d", getName().c_str(), newColor.R, newColor.G, newColor.B, color1.R, color1.G, color1.B, color2.R, color2.G, color2.B, firstColor);
            if (firstColor) {
                currentColor = newColor;
                newColor = color1;
            } else {
                currentColor = newColor;
                newColor = color2;
            }
            // Log.traceln("FadeAnimation '%s' 1st frame. currentColor: %d %d %d, newColor: %d %d %d", getName().c_str(), currentColor.R, currentColor.G, currentColor.B, newColor.R, newColor.G, newColor.B);
        }
        
        float progress = getProgress();
        RgbColor blendedColor = RgbColor::LinearBlend(currentColor, newColor, progress);
        // Log.traceln("FadeAnimation '%s' animating. Progress: %s. Blended color: %d %d %d. Dimm: %d", getName().c_str(), String(progress, 4), blendedColor.R, blendedColor.G, blendedColor.B, dimm);
        line->setColor(blendedColor, dimm);
    }

    void setColor1(RgbColor color) {
        this->color1 = color;
    }

    void setColor2(RgbColor color) {
        this->color2 = color;
    }

    void setDimm(uint8_t dimm) {
        this->dimm = dimm;
    }

    void setFirstColor(bool firstColor) {
        this->firstColor = firstColor;
    }
};
