#ifndef DMX_MANAGER_H
#define DMX_MANAGER_H

#include <map>
#include <array>
#include <set>
#include <ArtnetWiFi.h>
#include <DmxListener.h>
#include <settings.h>

class DmxManager {
private:
    std::map<uint16_t /*universe*/, uint8_t /*lastSequence*/> lastDmxSequences;
    std::map<uint16_t /*universe*/, std::array<uint8_t, 512> /*data*/> dmxData;
    
    DmxListener* dmxListener;
    unsigned long lastDmxCommit;
    unsigned long* lastCommandReceivedAt;
    
public:
    DmxManager(unsigned long* lastCommandReceivedAt);
    ~DmxManager();
    
    // Initialization
    void initialize(Settings& settings);
    
    // DMX frame handling
    void onDmxFrame(const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote);
    
    // Processing
    void processAndCommit(unsigned long commitIntervalMs, std::function<void()> commitCallback);
    
    // Accessors
    DmxListener* getDmxListener() { return dmxListener; }
    std::map<uint16_t, std::array<uint8_t, 512>>& getDmxData() { return dmxData; }
    std::set<uint16_t> getListeningUniverses();
};

#endif // DMX_MANAGER_H
