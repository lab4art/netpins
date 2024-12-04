#pragma once

#include <Things.h>
#include <animations.h>

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
                TailAnimation::Direction direction = TailAnimation::Direction::RIGHT,
                bool repeat = false,
                String name = "Tail Ani") {
            tailAnimation = new TailAnimation(
                aScheduler, 
                line, 
                direction,
                repeat,
                name);
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