#pragma once

#include <NeoPixelBus.h>
#include <ESP32Servo.h>

class Thing {
    public:
        virtual int numChannels() = 0;
        virtual void setData(uint8_t* data) = 0;
};

class Switchabe {
    public:
        virtual void on() = 0;
        virtual void off() = 0;
};

template<typename T_COLOR> 
class SliceThingBase : public Thing, public Switchabe {
    protected:
        int pxFrom;
        int pxTo;
        static NeoGamma<NeoGammaTableMethod> colorGamma;

        virtual void doSetColor(uint16_t px, T_COLOR color, uint8_t dimm) = 0;

    public:
        SliceThingBase(int pxFrom, int pxTo):
            pxFrom(pxFrom), pxTo(pxTo) {
        }

        int size() {
            return pxTo - pxFrom + 1;
        }

        void setColor(T_COLOR color, uint8_t dimm = 255) {
            for (int px = 0; px <= pxTo - pxFrom; px++) {
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
            for (int px = 0; px <= pxTo - pxFrom; px++) {
                doSetColor(px, T_COLOR(255), 255);
            }
        }

        void off() {
            for (int px = 0; px <= pxTo - pxFrom; px++) {
                doSetColor(px, T_COLOR(0), 0);
            }
        }

};

class LedThing : public Thing, public Switchabe {
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
};

class RgbThing : public SliceThingBase<RgbColor> {
  private:
    NeoPixelBus<NeoGrbFeature, NeoEsp32RmtNWs2812xMethod>* strip;
    bool dimmable;
    
  protected:
    void doSetColor(uint16_t px, RgbColor color, uint8_t dimm) {
        // update only if the color is different
        auto stripPx = pxFrom + px;
        auto gColor = colorGamma.Correct(color);
        auto gDimm = NeoGammaTableMethod::Correct(dimm);
        auto dimmFactor = gDimm / 255.0;
        auto gColorDimm = RgbColor(gColor.R * dimmFactor, gColor.G * dimmFactor, gColor.B * dimmFactor);
        if (strip->GetPixelColor(stripPx) != gColorDimm) {
            strip->SetPixelColor(stripPx, gColorDimm);
        }
    }

  public:
        RgbThing(NeoPixelBus<NeoGrbFeature, NeoEsp32RmtNWs2812xMethod>* strip, int pxFrom, int pxTo, bool dimmable):
                strip(strip),
                SliceThingBase<RgbColor>(pxFrom, pxTo),
                dimmable(dimmable) {
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

        bool isDimmable() {
            return dimmable;
        }

};

class RgbwThing : public SliceThingBase<RgbwColor> {
    private:
        NeoPixelBus<NeoGrbwFeature, NeoEsp32RmtNSk6812Method>* strip;
        static NeoGamma<NeoGammaTableMethod> colorGamma;
        bool dimmable;

    protected:
        void doSetColor(uint16_t px, RgbwColor color, uint8_t dimm) {
            // update only if the color is different
            auto stripPx = pxFrom + px;
            auto gColor = colorGamma.Correct(color);
            auto gDimm = NeoGammaTableMethod::Correct(dimm);
            auto dimmFactor = gDimm / 255.0;
            auto gColorDimm = RgbwColor(gColor.R * dimmFactor, gColor.G * dimmFactor, gColor.B * dimmFactor, gColor.W * dimmFactor);
            if (strip->GetPixelColor(stripPx) != gColorDimm) {
                strip->SetPixelColor(stripPx, gColorDimm);
            }
        }

    public:
        RgbwThing(NeoPixelBus<NeoGrbwFeature, NeoEsp32RmtNSk6812Method>* strip, int pxFrom, int pxTo, bool dimmable):
                strip(strip),
                SliceThingBase<RgbwColor>(pxFrom, pxTo),
                dimmable(dimmable) {
        }

        int numChannels() {
            return dimmable ? 5 : 4;
        }
        
        void setData(uint8_t* data) { // data is a pointer to the first element of the array
            if (dimmable) {
                setColor(RgbwColor(data[0], data[1], data[2], data[3]), data[4]);
            } else {
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
