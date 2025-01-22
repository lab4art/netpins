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

class SensorThing {
    private:
        unsigned long pullMillis;
        unsigned long lastPullMillis = 0;

    protected:
        uint8_t pin;

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


    public:
        SensorThing(uint8_t pin, unsigned long pullMillis): 
                pin(pin),
                pullMillis(pullMillis) {
        }

        /**
         * Returns true if the sensor value was read, false otherwise.
         */
        virtual boolean read() = 0;

        uint8_t getPin() {
            return pin;
        }
};

class FloatSensor : public SensorThing {
    private:
        float value;
        std::function<void(float)> onChangeListener = nullptr;

    public:
        FloatSensor(uint8_t pin, unsigned long pullMillis):
                SensorThing(pin, pullMillis) {
        }

        float getValue() {
            return value;
        }

        boolean setValue(float value) {
            if (this->value == value) {
                return false;
            } else {
                this->value = value;
                if (onChangeListener != nullptr) {
                    onChangeListener(value);
                }
                return true;
            }
        }

        void setOnChangeListener(std::function<void(float)> onChangeListener) {
            this->onChangeListener = onChangeListener;
        }
};

class BooleanSensor : public FloatSensor {

    public:
        BooleanSensor(uint8_t pin, unsigned long pullMillis):
                FloatSensor(pin, pullMillis) {
        }

        boolean getValue() {
            return FloatSensor::getValue() == 1.0f;
        }

        boolean setValue(boolean value) {
            return FloatSensor::setValue(value ? 1.0f : 0.0f);
        }
};

class DigitalReadSensor : public FloatSensor {

    public:
        DigitalReadSensor(uint8_t pin, unsigned long pullMillis):
                FloatSensor(pin, pullMillis) {
            pinMode(pin, INPUT);
        }

        boolean read() {
            if (!shouldPull()) {
                return false;
            }
            setValue(digitalRead(pin));
            return true;
        }
};

class TouchSensor : public BooleanSensor {
    private:
        int threshold;

        uint8_t veryfingTouch = 0;
        boolean unveryfiedTouch = false;
        MovingAverage* nonTouchedAverage = new MovingAverage(10);

        /**
         * Return true if touch is detected, false otherwise.
         */
        boolean doReadTouch() {
            int currentRead = touchRead(pin);
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
                BooleanSensor(pin, pullMillis),
                threshold(threshold) {
            pinMode(pin, INPUT);
        }

        // threat as touched when 3 consecutive reads are detected
        boolean read() {
            if (shouldPull()) {
                veryfingTouch = 1;
                unveryfiedTouch = doReadTouch();
                if (unveryfiedTouch) {
                    return false;
                } else {
                    BooleanSensor::setValue(false);
                    return true;
                }
            } else if (unveryfiedTouch && (millis() - getLastPullMillis() > 10 * veryfingTouch)) {
                // if short time passed since last touched, verify the touch
                // Log.infoln("Verifying touch. unveryfiedTouch: %d, veryfingTouch: %d", unveryfiedTouch ? 1 : 0, veryfingTouch);
                if (doReadTouch()) {
                    veryfingTouch++;
                    if (veryfingTouch == 3) {
                        unveryfiedTouch = false; // prevent verification untill next pull time
                        BooleanSensor::setValue(true);
                        return true;
                    } else {
                        return false;
                    }
                } else {
                    unveryfiedTouch = false; // if no-touch prevent verification untill next pull time
                    BooleanSensor::setValue(false);
                    return true;
                }
            } else {
                return false;
            }
        }
};

class HumTempSensor : public SensorThing {
    private:
        DHT_Unified* dht;
        FloatSensor* tempSensor;
        FloatSensor* humiditySensor;

    public:
        HumTempSensor(uint8_t pin, unsigned long pullMillis):
                SensorThing(pin, pullMillis) {
            pinMode(pin, INPUT_PULLUP);
            dht = new DHT_Unified(pin, DHT22);
            dht->begin();
        }

        boolean read() {
            if (!shouldPull()) {
                return false;
            }
            sensors_event_t tempEvent;
            sensors_event_t humidityEvent;
            dht->temperature().getEvent(&tempEvent);
            dht->humidity().getEvent(&humidityEvent);
            tempSensor->setValue(tempEvent.temperature);
            humiditySensor->setValue(humidityEvent.relative_humidity);
            return true;
        }

        float getTemperature() {
            return tempSensor->getValue();
        }

        float getHumidity() {
            return humiditySensor->getValue();
        }

        void setTempChangeListener(std::function<void(float)> onChangeListener) {
            tempSensor->setOnChangeListener(onChangeListener);
        }

        void setHumidityChangeListener(std::function<void(float)> onChangeListener) {
            humiditySensor->setOnChangeListener(onChangeListener);
        }
};

class DistanceSensor : public FloatSensor {
    private:
        VL53L1X sensor;
        int lastValue = 0;
        int threshold = 100;

    public:
        DistanceSensor(uint8_t pin, unsigned long pullMillis, int threshold):
                FloatSensor(pin, pullMillis),
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

        boolean read() {
            if (!shouldPull()) {
                return false;
            }
            // sensor.read(false);
            // sensor.read(true);
            int currentValue = sensor.ranging_data.range_mm;
            if (abs(lastValue - currentValue) > threshold) {
                lastValue = currentValue;
                setValue(currentValue);
            }
            return true;
        }

};