#include "DmxManager.h"
#include <Log.h>
#include <Arduino.h>

DmxManager::DmxManager(unsigned long* lastCommandReceivedAt) 
    : dmxListener(nullptr), lastDmxCommit(0), lastCommandReceivedAt(lastCommandReceivedAt) {
}

DmxManager::~DmxManager() {
    if (dmxListener != nullptr) {
        delete dmxListener;
    }
}

void DmxManager::initialize(Settings& settings) {
    dmxListener = new DmxListener(settings.dmxChOffset);
    dmxListener->initializeDmxData(dmxData);
    dmxListener->restoreDmxData(dmxData);
}

void DmxManager::onDmxFrame(const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {
    // process only if we listen to this universe
    if (dmxListener->isListeningToUniverse(metadata.universe) == false) {
        return;
    }
    
    if (lastCommandReceivedAt != nullptr) {
        *lastCommandReceivedAt = millis();
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

void DmxManager::processAndCommit(unsigned long commitIntervalMs, std::function<void()> commitCallback) {
    /*
    30ms = 30fps
    20ms = 50fps
    13ms = 75fps
    */
    if (millis() - lastDmxCommit > commitIntervalMs) {
        for (auto& universeData : dmxData) {
            dmxListener->processDmxData(universeData.first, universeData.second);
        }
        
        if (commitCallback) {
            commitCallback();
        }
        
        lastDmxCommit = millis();
    }
}

std::set<uint16_t> DmxManager::getListeningUniverses() {
    if (dmxListener == nullptr) {
        return std::set<uint16_t>();
    }
    return dmxListener->getListeningUniverses();
}
