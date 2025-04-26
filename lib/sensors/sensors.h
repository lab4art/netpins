#pragma once

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <VL53L1X.h>


class MovingAverage {
    private:
        int* values;
        int size;
        int index = 0;
        int sum = 0;
        int count = 0;

    public:
        MovingAverage(int size):
                size(size) {
            values = new int[size];
            for (int i = 0; i < size; i++) {
                values[i] = 0;
            }
        }

        ~MovingAverage() {
            delete[] values;
        }

        int add(int value) {
            if (count < size) {
                count++;
            }
            sum -= values[index];
            values[index] = value;
            sum += value;
            index = (index + 1) % size;
            return sum / size;
        }

        int get() {
            return sum / size;
        }

        boolean isFull() {
            return count == size;
        }
};

template <typename VALUE_TYPE>
class SensorBase {
    private:
        uint8_t pin;
        unsigned long pullMillis;
        unsigned long lastPullMillis = 0;

        VALUE_TYPE value;
        std::function<void(VALUE_TYPE)> onChangeListener = nullptr;

    protected:
        boolean shouldPull() {
            unsigned long currentMillis = millis();
            if (currentMillis - lastPullMillis > pullMillis) {
                lastPullMillis = currentMillis;
                return true;
            } else {
                return false;
            }
        }
        
        unsigned long getLastPullMillis() {
            return lastPullMillis;
        }

        /*
         * Read the sensor and set the value.
         * Return true if the value has changed, false otherwise.
        */
        virtual boolean doRead() = 0;

        boolean setValue(VALUE_TYPE value) {
            if (this->value == value) {
                return false;
            } else {
                this->value = value;
                if (onChangeListener != nullptr) {
                    //Log.traceln("Invoking on change with value: %d", value);
                    onChangeListener(value);
                }
                return true;
            }
        }

    public:
        SensorBase(uint8_t pin, unsigned long pullMillis): 
                pin(pin),
                pullMillis(pullMillis) {
        }

        boolean read() {
            if (!shouldPull()) {
                return false;
            }
            return doRead();
        }

        uint8_t getPin() {
            return pin;
        }

        VALUE_TYPE getValue() {
            return value;
        }

        void setOnChangeListener(std::function<void(VALUE_TYPE)> onChangeListener) {
            this->onChangeListener = onChangeListener;
        }
};

class DigitalReadSensor : public SensorBase<boolean> {

    public:
        DigitalReadSensor(uint8_t pin, unsigned long pullMillis, uint8_t pinInputMode = INPUT):
                SensorBase(pin, pullMillis) {
            pinMode(pin, pinInputMode);
        }

        boolean doRead() {
            return setValue(digitalRead(getPin()) == HIGH);
        }
};

class AnalogReadSensor : public SensorBase<int> {

    public:
        AnalogReadSensor(uint8_t pin, unsigned long pullMillis):
                SensorBase(pin, pullMillis) {
            pinMode(pin, INPUT);
        }

        boolean doRead() {
            return setValue(analogRead(getPin()));
        }
};

class TouchSensor : public SensorBase<boolean> {
    private:
        int threshold;

        uint8_t veryfingTouch = 0;
        boolean unveryfiedTouch = false;
        MovingAverage* nonTouchedAverage = new MovingAverage(10);

        /**
         * Return true if touch is detected, false otherwise.
         */
        boolean doRead() {
            int currentRead = touchRead(getPin());
            if (nonTouchedAverage->isFull() && abs(nonTouchedAverage->get() - currentRead) > threshold) {
                // Log.infoln("Touched. Avg touch %d, current touch %d", nonTouchedAverage->get(), currentRead);
                return true;
            } else {
                nonTouchedAverage->add(currentRead);
                // Log.infoln("NO touch. Avg touch %d, current touch %d", nonTouchedAverage->get(), currentRead);
                return false;
            }
        }

    public:
        TouchSensor(uint8_t pin, unsigned long pullMillis, int threshold):
                SensorBase(pin, pullMillis),
                threshold(threshold) {
            pinMode(pin, INPUT);
        }

        // threat as touched when 3 consecutive reads are detected
        boolean read() {
            if (shouldPull()) {
                veryfingTouch = 1;
                unveryfiedTouch = doRead();
                if (unveryfiedTouch) {
                    return false;
                } else {
                    return setValue(false);
                }
            } else if (unveryfiedTouch && (millis() - getLastPullMillis() > 10 * veryfingTouch)) {
                // if short time passed since last touched, verify the touch
                // Log.infoln("Verifying touch. unveryfiedTouch: %d, veryfingTouch: %d", unveryfiedTouch ? 1 : 0, veryfingTouch);
                if (doRead()) {
                    veryfingTouch++;
                    if (veryfingTouch == 3) {
                        unveryfiedTouch = false; // prevent verification untill next pull time
                        return setValue(true);
                    } else {
                        return false;
                    }
                } else {
                    unveryfiedTouch = false; // if no-touch prevent verification untill next pull time
                    return setValue(false);
                }
            } else {
                return false;
            }
        }
};

struct HumTemp {
    float humidity;
    float temperature;

    bool operator==(const HumTemp& other) const {
        return humidity == other.humidity && temperature == other.temperature;
    }

    bool operator!=(const HumTemp& other) const {
        return !(*this == other);
    }
};

class HumTempSensor : public SensorBase<HumTemp> {
    private:
        DHT_Unified* dht;

    public:
        HumTempSensor(uint8_t pin, unsigned long pullMillis):
                SensorBase(pin, pullMillis) {
            pinMode(pin, INPUT_PULLUP);
            dht = new DHT_Unified(pin, DHT22);
            dht->begin();
        }

        boolean doRead() {
            sensors_event_t tempEvent;
            sensors_event_t humidityEvent;
            dht->temperature().getEvent(&tempEvent);
            dht->humidity().getEvent(&humidityEvent);
            return setValue({humidityEvent.relative_humidity, tempEvent.temperature});
        }
};

class DistanceSensor : public SensorBase<int> {
    private:
        VL53L1X sensor;
        int threshold = 100; // TODO configurable

    public:
        DistanceSensor(uint8_t pin, unsigned long pullMillis, int threshold):
                SensorBase(pin, pullMillis),
                threshold(threshold) {
            pinMode(pin, INPUT);
            Wire.begin();
            Wire.setClock(400000); // use 400 kHz I2C
            sensor.init();
            // sensor.setTimeout(500);

            sensor.setDistanceMode(VL53L1X::Long);
            sensor.setMeasurementTimingBudget(33000);
            sensor.startContinuous(pullMillis);
            if (!sensor.init()) {
                Serial.println("Failed to detect and initialize sensor!");
            }
        }

        boolean doRead() {
            // sensor.read(false);
            // sensor.read(true);
            int currentValue = sensor.ranging_data.range_mm;
            if (abs(getValue() - currentValue) > threshold) {
                return setValue(currentValue);
            }
            return false;
        }
};


