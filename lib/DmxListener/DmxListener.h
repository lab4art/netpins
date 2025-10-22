#pragma once

#include <Arduino.h>
#include <ArduinoLog.h>
#include <vector>
#include <Things.h>
#include <Preferences.h>
#include <ArduinoLog.h>
#include <settings.h>

/**
 * Mapping between things and dmx universe / channel.
 */
class DmxMapping {
    public:
        Thing* thing;
        DmxCfg dmxCfg;

        DmxMapping(Thing* thing, DmxCfg dmxCfg):
            thing(thing),
            dmxCfg(dmxCfg) {
        }
};

class UniverseStorage {
    private:
        Preferences prefs;

    public:
        void begin(bool readOnly = false) {
            prefs.begin("dmx-state", readOnly);
        }

        void end() {
            prefs.end();
        }

        void storeUniverse(uint16_t universe, const uint8_t data[512]) {
            String key = "u" + String(universe);
            prefs.putBytes(key.c_str(), data, 512);
        }

        bool loadUniverse(uint16_t universe, uint8_t data[512]) {
            String key = "u" + String(universe);
            size_t len = prefs.getBytesLength(key.c_str());
            
            if (len == 512) {
                prefs.getBytes(key.c_str(), data, 512);
                return true;
            }
            
            // Initialize with zeros if not found
            memset(data, 0, 512);
            return false;
        }

        void clear() {
            prefs.clear();
        }
};

/**
 * Each controller has one DmxListener instance to handle DMX data.
 * 
 * Dmx listener controls things, which are simple leds, pxiels on led stipes or group of pixels on led stripe.
 * Things has different number of channles.
 */
class DmxListener {
    private:
        int dmxOffset; // dmx offset where this listener starts listening, 1 based (1-512)
        std::vector<DmxMapping*> dmxMappings;
        Preferences preferences;
        uint8_t lastStoreFlag = 0;
        std::set<uint16_t> dmxUniverses; // set of universes we listen to

    public:
        DmxListener(int dmxOffset):
            dmxOffset(dmxOffset) {
        }

        ~DmxListener() {
            clearMappings();
        }

        void addMapping(Thing* thing, DmxCfg dmxCfg) {
            Log.noticeln("Adding mapping for thing %s on universe %d channel %d", thing->getName().c_str(), dmxCfg.universe, dmxCfg.channel);
            dmxMappings.push_back(new DmxMapping(thing, dmxCfg));
            dmxUniverses.insert(dmxCfg.universe);
        }

        void removeMappingForThing(String thingName) {
            auto it = std::remove_if(dmxMappings.begin(), dmxMappings.end(),
                [&thingName](DmxMapping* mapping) {
                    return mapping->thing->getName().equals(thingName);
                });
            if (it != dmxMappings.end()) {
                dmxMappings.erase(it, dmxMappings.end());
            }
        }

        void clearMappings() {
            for (auto& mapping : dmxMappings) {
                delete mapping;
            }
            dmxMappings.clear();
        }

        void processDmxData(u_int16_t universe, std::array<uint8_t, 512>& data) {
            for (auto& mapping : dmxMappings) {
                // Log.traceln("Checking mapping for thing %s on universe %d channel %d", mapping->thing->getName().c_str(), mapping->dmxCfg.universe, mapping->dmxCfg.channel);
                if (mapping->dmxCfg.universe == universe) {
                    int channel = mapping->dmxCfg.get0BasedChannel();
                    // Log.traceln("Setting data for thing %s on universe %d channel %d", mapping->thing->getName().c_str(), universe, channel + 1);
                    mapping->thing->setData(&data[channel]);
                }
            }
        }

        /** 
         * Store map of dmx universes with channel data if data has changed.
         */
        boolean storeDmxData(const std::map<uint16_t /*universe*/, std::array<uint8_t, 512> /*data*/>& dmxData) {
            UniverseStorage storage;
            storage.begin(false);
            bool changed = false;
            for (const auto& pair : dmxData) {
                uint8_t storedData[512];
                storage.loadUniverse(pair.first, storedData);
                // Compare with new data
                if (memcmp(storedData, pair.second.data(), 512) != 0) {
                    changed = true;
                }
            }
            if (changed) {
                Log.traceln("Storing changed DMX data");
                storage.clear(); // clear old data
                for (const auto& pair : dmxData) {
                    storage.storeUniverse(pair.first, pair.second.data());
                }
            }
            storage.end();
            return changed;
        }

        void restoreDmxData(std::map<uint16_t /*universe*/, std::array<uint8_t, 512> /*data*/>& dmxData) {
            UniverseStorage storage;
            storage.begin(true);
            for (const auto& mapping : dmxMappings) {
                uint16_t universe = mapping->dmxCfg.universe;
                uint8_t data[512];
                storage.loadUniverse(universe, data);
                std::array<uint8_t, 512> dataArray;
                memcpy((void*)dataArray.data(), data, 512);
                dmxData[universe] = dataArray;
            }
            storage.end();
        }

        /**
         * Clear stored DMX data.
         */
        void clearDmxData() {
            UniverseStorage storage;
            storage.begin(false);
            storage.clear();
            storage.end();
        }

        /**
         * Get the index of the first channel of the thing with the given name.
         * The index is 0 based, so the first channel is 0.
         */
        // int getThingChannelIndex(String name) {
        //     int channel = 0; // channel variable contains the last channel of the last thing compared in the loop
        //     for (auto& thing : dmxMappings) {
        //         if (thing->getName().equals(name)) {
        //             return channel;
        //         }
        //         channel += thing->numChannels();
        //     }
        //     return -1; // not found
        // }

        bool isListeningToUniverse(uint16_t universe) {
            return dmxUniverses.find(universe) != dmxUniverses.end();
        }

        std::set<uint16_t> getListeningUniverses() {
            return dmxUniverses;
        }
};
