#pragma once

#include <vector>
#include <ArduinoLog.h>
#include <NeoPixelBus.h>
#include <ESP32Servo.h>

class Thing {
    private:
        String name;
    public:
        virtual int numChannels() = 0;
        virtual void setData(uint8_t* data) = 0;

        void setName(String name) {
            this->name = name;
        }

        String getName() {
            return name;
        }
};

class Switchabe {
    public:
        virtual void on() = 0;
        virtual void off() = 0;
};

class SwitchableThing : public Thing, public Switchabe {
};

template<typename T_COLOR> 
class SliceThingBase : public SwitchableThing {
    protected:
        int pxFrom;
        int pxTo;
        static NeoGamma<NeoGammaTableMethod> colorGamma;

        virtual void doSetColor(uint16_t px, T_COLOR color, uint8_t dimm) = 0;
        boolean dimmable;

    public:
        SliceThingBase(int pxFrom, int pxTo, bool dimmable):
                dimmable(dimmable), 
                pxFrom(pxFrom), 
                pxTo(pxTo) {
        }

        int size() {
            return pxTo - pxFrom + 1;
        }

        void setColor(T_COLOR color, uint8_t dimm = 255) {
            for (int px = 0; px < size(); px++) {
                doSetColor(px, color, dimm);
            }
        }

        void setColor(uint16_t px, T_COLOR color, uint8_t dimm = 255) {
            // make sure px is in range
            if (px < 0 || px >= size()) {
                return;
            }
            doSetColor(px, color, dimm);
        }

        void on() {
            for (int px = 0; px < size(); px++) {
                doSetColor(px, T_COLOR(255), 255);
            }
        }

        void off() {
            for (int px = 0; px < size(); px++) {
                doSetColor(px, T_COLOR(0), 0);
            }
        }

        boolean isDimmable() {
            return dimmable;
        }
};

class LedThing : public SwitchableThing {
    private:
        int pin;
        int currentValue = 0;
        bool dirty = false;
        
        // Function to map 8-bit input to non-linear 14-bit range
        static uint16_t map8bitTo14bit(uint8_t input) {
            // Normalize the 8-bit input to [0, 1]
            float normalizedInput = input / 255.0;

            // Apply the non-linear transformation using a power function
            // float transformed = std::pow(normalizedInput, gamma);
            float transformed = 0.9151414 * std::pow((normalizedInput + 0.03), 3);
            if (transformed > 1.0) transformed = 1.0;
            if (transformed < 0.0) transformed = 0.0;

            // Scale to 14-bit range [0, 16383]
            uint16_t output = static_cast<uint16_t>(round(transformed * 16383));
            return output;
        }

    public:
        static uint16_t gammaTable[256];
        
        LedThing(int pin) {
            this->pin = pin;
            pinMode(pin, OUTPUT);
        }

        int numChannels() {
            return 1;
        }

        void setData(uint8_t* data) {
            // auto newValue = NeoGammaTableMethod::Correct(data[0]);
            auto newValue = gammaTable[data[0]];
            if (newValue == currentValue) {
                return;
            } else {
                currentValue = newValue;
                dirty = true;
            }
        }

        void on() {
            uint8_t data[1] = {255};
            setData(data);
        }

        void off() {
            uint8_t data[1] = {0};
            setData(data);
        }

        void commit() {
            if (!dirty) {
                return;
            }
            analogWrite(pin, currentValue);
            dirty = false;
        }

        static void set8bitTo14BitMapping() {
            for (int i = 0; i < 256; i++) {
                gammaTable[i] = map8bitTo14bit(i);
                // Serial.println(String(i) + " -> " + gammaTable[i]);
            }
        }

        int getPin() {
            return pin;
        }
};

class RgbThing : public SliceThingBase<RgbColor> {
  private:
    NeoPixelBus<NeoGrbFeature, NeoEsp32RmtNWs2812xMethod>* strip;
    
  protected:
    void doSetColor(uint16_t px, RgbColor color, uint8_t dimm) {
        // update only if the color is different
        auto stripPx = pxFrom + px;
        auto gColor = colorGamma.Correct(color);
        auto dimmFactor = dimm / 255.0;
        auto gColorDimm = RgbColor(gColor.R * dimmFactor, gColor.G * dimmFactor, gColor.B * dimmFactor);
        if (strip->GetPixelColor(stripPx) != gColorDimm) {
            //  Log.traceln("Setting pixel %d to %d %d %d, dimm %d", stripPx, gColorDimm.R, gColorDimm.G, gColorDimm.B, dimm);
            strip->SetPixelColor(stripPx, gColorDimm);
        }
    }

  public:
        RgbThing(NeoPixelBus<NeoGrbFeature, NeoEsp32RmtNWs2812xMethod>* strip, int pxFrom, int pxTo, bool dimmable):
                strip(strip),
                SliceThingBase<RgbColor>(pxFrom, pxTo, dimmable) {
            Log.traceln("RgbThing created. FromPx: %d, ToPx: %d. Dimmable: %d", pxFrom, pxTo, dimmable);
        }

        int numChannels() {
            return dimmable ? 4 : 3;
        }
        
        void setData(uint8_t* data) { // data is a pointer to the first element of the array
            if (dimmable) {
                setColor(RgbColor(data[0], data[1], data[2]), data[3]);
            } else {
                setColor(RgbColor(data[0], data[1], data[2]));
            }
        }
};

class RgbwThing : public SliceThingBase<RgbwColor> {
    private:
        NeoPixelBus<NeoGrbwFeature, NeoEsp32RmtNSk6812Method>* strip;
        static NeoGamma<NeoGammaTableMethod> colorGamma;

    protected:
        void doSetColor(uint16_t px, RgbwColor color, uint8_t dimm) {
            // update only if the color is different
            auto stripPx = pxFrom + px;
            auto gColor = colorGamma.Correct(color);
            auto dimmFactor = dimm / 255.0;
            auto gColorDimm = RgbwColor(gColor.R * dimmFactor, gColor.G * dimmFactor, gColor.B * dimmFactor, gColor.W * dimmFactor);
            if (strip->GetPixelColor(stripPx) != gColorDimm) {
                // Log.traceln("Setting pixel %d to %d %d %d %d, w/ dimm %d.", stripPx, gColor.R, gColor.G, gColor.B, gColor.W, dimm);
                strip->SetPixelColor(stripPx, gColorDimm);
            }
        }

    public:
        RgbwThing(NeoPixelBus<NeoGrbwFeature, NeoEsp32RmtNSk6812Method>* strip, int pxFrom, int pxTo, bool dimmable):
                strip(strip),
                SliceThingBase<RgbwColor>(pxFrom, pxTo, dimmable) {
            Log.traceln("RgbwThing created. FromPx: %d, ToPx: %d. Dimmable: %d", pxFrom, pxTo, dimmable);
        }

        int numChannels() {
            return dimmable ? 5 : 4;
        }
        
        void setData(uint8_t* data) { // data is a pointer to the first element of the array
            if (dimmable) {
                Log.traceln("Setting color to %d %d %d %d, dimm %d", data[0], data[1], data[2], data[3], data[4]);
                setColor(RgbwColor(data[0], data[1], data[2], data[3]), data[4]);
            } else {
                Log.traceln("Setting color to %d %d %d %d, not dimmable", data[0], data[1], data[2], data[3]);
                setColor(RgbwColor(data[0], data[1], data[2], data[3]));
            }
        }
};

class ServoThing : public Thing {
    private:
        int currentValue = 0;
        bool dirty = false;
        Servo servo;
        int maxAngle;

    public:
        ServoThing(int pin, int maxAngle, int minPulseWidth = 500, int maxPulseWidth = 2500) {
            this->maxAngle = maxAngle;
            pinMode(pin, OUTPUT);
            servo.setPeriodHertz(50);
            servo.attach(pin, minPulseWidth, maxPulseWidth);
        }

        int numChannels() {
            return 1;
        }

        void setData(uint8_t* data) {
            if (data[0] == currentValue) {
                return;
            } else {
                currentValue = data[0];
                dirty = true;
            }
        }

        void commit() {
            if (!dirty) {
                return;
            }
            int angle = map(currentValue, 0, 255, 0, this->maxAngle);
            servo.write(angle);
            dirty = false;
        }
};

class ThingGroup : public SwitchableThing {
    private:
        int numOfChannels;
    protected:
        std::vector<SwitchableThing*> things;
        boolean dimmable;

    public:
        ThingGroup(std::vector<SwitchableThing*> things, boolean dimmable):
            things(things),
            dimmable(dimmable) {
                int sumChannels = 0;
                for (auto& thing : things) {
                    sumChannels += thing->numChannels();
                }
                if (dimmable) {
                    numOfChannels = sumChannels + 1;
                } else {
                    numOfChannels = sumChannels;
                }
        }

        int numChannels() {
            return numOfChannels;
        }

        boolean isDimmable() {
            return dimmable;
        }

        void on() {
            for (auto& thing : things) {
                thing->on();
            }
        }

        void off() {
            for (auto& thing : things) {
                thing->off();
            }
        }
};
class RgbThingGroup : public ThingGroup {
    private:
        std::vector<RgbThing*> rgbThings;

    public:
        RgbThingGroup(std::vector<RgbThing*> rgbThings, boolean dimmable):
            ThingGroup(std::vector<SwitchableThing*>(rgbThings.begin(), rgbThings.end()), dimmable),
            rgbThings(rgbThings) {
        }

        void setData(uint8_t* data) {
            int dimm;
            if (dimmable) {
                dimm = data[numChannels() - 1];
            } else {
                dimm = 255;
            }

            int currentChannel = 0;
            for (auto& thing : rgbThings) {
                int numChannels = thing->numChannels();
                uint8_t* thingData = data + currentChannel;
                thing->setColor(RgbColor(thingData[0], thingData[1], thingData[2]), dimm);
                currentChannel += numChannels;
            }
        }

        std::vector<RgbThing*> things() {
            return rgbThings;
        }
};

class RgbwThingGroup : public ThingGroup {
    private:
        std::vector<RgbwThing*> rgbwThings;

    public:
        RgbwThingGroup(std::vector<RgbwThing*> rgbwThings, boolean dimmable):
            ThingGroup(std::vector<SwitchableThing*>(rgbwThings.begin(), rgbwThings.end()), dimmable),
            rgbwThings(rgbwThings) {
        }

        void setData(uint8_t* data) {
            int groupDimm;
            if (dimmable) {
                groupDimm = data[numChannels() - 1];
            } else {
                groupDimm = 255;
            }
            int currentChannel = 0;
            for (auto& thing : rgbwThings) {
                int numChannels = thing->numChannels();
                uint8_t* thingData = data + currentChannel;
                if (thing->isDimmable()) {
                    // Log.traceln("Setting group data to dimmable thing with %d channels. Color: %d %d %d %d, dimm: %d", numChannels, thingData[0], thingData[1], thingData[2], thingData[3], thingData[4]);
                    thing->setColor(RgbwColor(thingData[0], thingData[1], thingData[2], thingData[3]), thingData[4]);
                } else {
                    // Log.traceln("Setting group data to non-dimmable thing. Group dimm: %d", groupDimm);
                    thing->setColor(RgbwColor(thingData[0], thingData[1], thingData[2], thingData[3]), groupDimm);
                }
                currentChannel += numChannels;
            }
        }
};

LedThing* findLedThing(std::vector<LedThing*> leds, int pin);
