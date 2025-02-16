#pragma once

#include <Arduino.h>
#include <ArduinoLog.h>
#include <vector>
#include <Things.h>
#include <Preferences.h>
#include <ArduinoLog.h>

/**
 * Each controller has one DmxListener instance to handle DMX data.
 * 
 * Dmx listener controls things, which are simple leds, pxiels on led stipes or group of pixels on led stripe.
 * Things has different number of channles.
 */
class DmxListener {
    private:
        int firstDmxChannel;
        std::vector<Thing*> thingList;
        std::map<int, Thing*> thingMap;
        Preferences preferences;
        uint8_t lastStoreFlag = 0;
        bool enableStore;

        // store the data to preferences when last byte is flipped from 0 to 255
        void storeDmxData(uint8_t data[512]) {
            if (data[511] == 255 && lastStoreFlag == 0) {
                // check if the data is the same as the last stored data
                uint8_t storedData[512] = {0};
                preferences.begin("dmx-state", true);
                preferences.getBytes("data", storedData, 511);
                preferences.end();
                bool sameData = true;
                for (int i = 0; i < 511; i++) {
                    if (data[i] != storedData[i]) {
                        sameData = false;
                        break;
                    }
                }
                if (sameData) {
                    Log.noticeln("DMX data is the same as the last stored data.");
                    return;
                }

                preferences.begin("dmx-state", false);
                preferences.putBytes("data", data, 511); // do not store the last byte, it's a store flag
                preferences.end();
                Log.infoln("DMX data stored.");
            }
            lastStoreFlag = data[511];
        }

    public:
        DmxListener(int firstDmxChannel, bool enableStore = false):
            firstDmxChannel(firstDmxChannel), 
            enableStore(enableStore) {
        }

        ~DmxListener() {
            clearThings();
        }

        void addThing(Thing* thing) {
            thingList.push_back(thing);
        }

        void removeThing(Thing* thing) {
            auto it = std::remove(thingList.begin(), thingList.end(), thing);
            if (it != thingList.end()) {
                thingList.erase(it);
            }
        }

        void clearThings() {
            for (auto& thing : thingList) {
                delete thing;
            }
            thingList.clear();
        }

        void processDmxData(uint16_t length, uint8_t data[512]) {
            int currentDmxIndex = firstDmxChannel - 1; // 1st channel is 1 (means 0 in the art-net data array)
            for (auto& thing : thingList) {
                // get the data for the thing based on the number of channels it needs
                if (currentDmxIndex + thing->numChannels() > length) {
                    Log.warningln("Missing DMX data for thing. 1st dmx ch %d, num ch: %d. Data length: %d.", currentDmxIndex, thing->numChannels(), length);
                    break;
                } else {
                    // Log.traceln("Processing thing %d, channels: %d, currentDmxChannel: %d", i, thing.numChannels(), currentDmxChannel);
                    // data is a pointer to the first element of the array
                    thing->setData(data + currentDmxIndex);                                
                    currentDmxIndex += thing->numChannels();
                }
            }
            if (enableStore) {
                storeDmxData(data);
            }
        }

        void restoreDmxData(uint8_t dmxData[512]) {
            if (!enableStore) {
                return;
            }
            // uint8_t dmxData[512] = {0};
            preferences.begin("dmx-state", true);
            preferences.getBytes("data", dmxData, 511);
            preferences.end();
            // set the store flag to 0 to prevent storing the data again
            dmxData[511] = 0;
        }
};