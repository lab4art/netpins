#pragma once

#include <Log.h>
#include <vector>
#include <set>
#include <map>
#include <array>
#include <functional>
#include <Things.h>
#include <Preferences.h>
#include <Log.h>
#include <settings.h>
#include <scheduler.h>
#include <ArtnetWiFi.h>
#include <Arduino.h>

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
 * Each controller has one DmxManager instance to handle DMX data.
 * 
 * DmxManager controls things, which are simple leds, pixels on led strips or group of pixels on led strip.
 * Things has different number of channels.
 */
class DmxManager : public ScheduledTask {
    private:
        int dmxOffset; // dmx offset where this listener starts listening, 1 based (1-512)
        std::vector<DmxMapping*> dmxMappings;
        Preferences preferences;
        uint8_t lastStoreFlag = 0;
        std::set<uint16_t> dmxUniverses; // set of universes we listen to
        
        // From DmxManager
        std::map<uint16_t /*universe*/, uint8_t /*lastSequence*/> lastDmxSequences;
        std::map<uint16_t /*universe*/, std::array<uint8_t, 512> /*data*/> dmxData;
        std::function<void()> onCommandReceived;

    public:
        DmxManager(Settings* settings, std::function<void()> onCommandReceived)
            : ScheduledTask(20, "DmxProcess"),
              dmxOffset(settings->dmxChOffset),
              onCommandReceived(onCommandReceived) {
        }

        ~DmxManager() {
            clearMappings();
        }

        void addMapping(Thing* thing, DmxCfg dmxCfg) {
            Log::infoln("Adding mapping for thing '%s' on universe %d channel %d", thing->getName().c_str(), dmxCfg.universe, dmxCfg.channel);
            dmxMappings.push_back(new DmxMapping(thing, dmxCfg));
            addUniverse(dmxCfg.universe);
        }

        /**
         * Add a universe to the list of universes we listen to.
         * This ensures the DMX data map is properly initialized for this universe.
         * 
         * @param universe The universe number to add
         */
        void addUniverse(uint16_t universe) {
            if (dmxUniverses.find(universe) == dmxUniverses.end()) {
                Log::infoln("Adding universe %d to DmxManager", universe);
                dmxUniverses.insert(universe);
                // Initialize the dmx data for this universe if not already present
                if (dmxData.find(universe) == dmxData.end()) {
                    dmxData[universe] = std::array<uint8_t, 512>{};
                }
            }
        }

        void removeMappingForThing(std::string thingName) {
            auto it = std::remove_if(dmxMappings.begin(), dmxMappings.end(),
                [&thingName](DmxMapping* mapping) {
                    return mapping->thing->getName() == thingName;
                });
            if (it != dmxMappings.end()) {
                dmxMappings.erase(it, dmxMappings.end());
            }
        }

        Thing* getThing(std::string thingName) {
            for (auto& mapping : dmxMappings) {
                if (mapping->thing->getName() == thingName) {
                    return mapping->thing;
                }
            }
            return nullptr;
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
                    // Log::traceln("Setting data for thing %s on universe %d channel %d", mapping->thing->getName().c_str(), universe, channel + 1);
                    mapping->thing->setData(&data[channel]);
                }
            }
        }

        /** 
         * Store map of dmx universes with channel data if data has changed.
         */
        bool storeDmxData(const std::map<uint16_t /*universe*/, std::array<uint8_t, 512> /*data*/>& dmxData) {
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
                Log::traceln("Storing changed DMX data");
                storage.clear(); // clear old data
                for (const auto& pair : dmxData) {
                    storage.storeUniverse(pair.first, pair.second.data());
                }
            }
            storage.end();
            return changed;
        }

        /**
         * Creates empty DMX data for all listening universes.
         */
        void initializeDmxData(std::map<uint16_t /*universe*/, std::array<uint8_t, 512> /*data*/>& dmxData) {
            for (const auto& universe : dmxUniverses) {
                dmxData[universe] = std::array<uint8_t, 512>{};
            }
        }

        void restoreDmxData(std::map<uint16_t /*universe*/, std::array<uint8_t, 512> /*data*/>& dmxData) {
            UniverseStorage storage;
            storage.begin(true);
            for (const auto& mapping : dmxMappings) {
                uint16_t universe = mapping->dmxCfg.universe;
                // Only restore if the universe already exists in the map
                if (dmxData.find(universe) != dmxData.end()) {
                    uint8_t data[512];
                    storage.loadUniverse(universe, data);
                    memcpy((void*)dmxData[universe].data(), data, 512);
                }
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
        
        // From DmxManager - Art-Net frame handling
        void onDmxFrame(const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {
            // process only if we listen to this universe
            if (!isListeningToUniverse(metadata.universe)) {
                return;
            }
            
            if (onCommandReceived) {
                onCommandReceived();
            }
            
            // ignore old sequences unless counter flipped (per-universe tracking)
            uint8_t lastSequence = lastDmxSequences[metadata.universe];
            if (metadata.sequence < lastSequence && lastSequence - metadata.sequence < 10) {
                Log::traceln("Ignoring old sequence %d for universe %d, last sequence: %d", metadata.sequence, metadata.universe, lastSequence);
                return;
            }
            lastDmxSequences[metadata.universe] = metadata.sequence;

            memcpy(dmxData[metadata.universe].data(), data, std::min(size, (uint16_t)512));
            
            // do not process the data here, leave IO callback as soon as possible
        }
        
        // ScheduledTask callback
        void callback() override {
            for (auto& universeData : dmxData) {
                processDmxData(universeData.first, universeData.second);
            }
        }
        
        // Accessor for dmxData
        std::map<uint16_t, std::array<uint8_t, 512>>& getDmxData() {
            return dmxData;
        }
};
