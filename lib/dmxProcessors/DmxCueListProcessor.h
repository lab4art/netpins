#pragma once

#include "DmxProcessor.h"
#include "settings.h"
#include "animations.h"
#include <vector>
#include <set>

/**
 * DMX Cue List Processor with Fade
 * 
 * Manages a list of DMX cues (scenes) with smooth fading between them.
 * Each scene is a configurable map of DMX channels to values with fade-in time.
 * Can be triggered manually, by time, or by DMX channel values.
 * 
 * Example: Create lighting scenes that fade smoothly from one to another
 */
class DmxCueListProcessor : public DmxProcessor {
    private:
        struct Scene {
            std::map<DmxCfg, uint8_t> channels;  // Map of channel -> value
            unsigned long fadeInMs;                  // Fade-in time for this scene
            unsigned long holdTimeMs;                // How long to hold before next cue
        };
        
        std::vector<Scene> cues;
        size_t currentCueIndex;
        size_t nextCueIndex;
        
        // Fade state
        GenericFadeAnimation<DmxCfg>* fadeAnimation;
        OneShotTask* holdTask;
        Scheduler* scheduler;
        DmxManager* dmxManager;  // Reference to DMX manager for accessing data
        
        bool loop;
        bool wasEnabled;
        
    public:
        DmxCueListProcessor(Scheduler* sched, DmxManager* dmxMgr)
            : DmxProcessor("DmxCueList"),
              currentCueIndex(0),
              nextCueIndex(0),
              fadeAnimation(nullptr),
              holdTask(nullptr),
              scheduler(sched),
              dmxManager(dmxMgr),
              loop(true),
              wasEnabled(false) {
            // Create fade animation (50 fps, non-repeating)
            fadeAnimation = new GenericFadeAnimation<DmxCfg>(50, false);
            fadeAnimation->setName("DmxCueFade");
            
            // Set callback to apply faded values to DMX data
            fadeAnimation->setCallback([this](const DmxCfg& chId, uint8_t value) {
                if (dmxManager == nullptr) return;
                auto& dmxData = dmxManager->getDmxData();
                auto universeIt = dmxData.find(chId.universe);
                if (universeIt != dmxData.end()) {
                    universeIt->second[chId.channel - 1] = value;
                }
            });
            
            // Set callback for when fade completes - schedule next cue after hold time
            fadeAnimation->setOnEnd([this]() {
                currentCueIndex = nextCueIndex;
                Log::traceln("[DmxCueList] Fade complete, now at cue %d", currentCueIndex);
                
                // Schedule next cue after hold time
                if (enabled && !cues.empty()) {
                    unsigned long holdMs = cues[currentCueIndex].holdTimeMs;
                    if (holdMs > 0 && (loop || currentCueIndex < cues.size() - 1)) {
                        holdTask->arm(holdMs);
                        Log::traceln("[DmxCueList] Scheduled next cue in %lu ms", holdMs);
                    }
                }
            });
            
            // Create hold task that advances to next cue
            holdTask = new OneShotTask(0, [this]() {
                if (enabled) {
                    nextCue();
                }
            });
            
            fadeAnimation->schedule(scheduler);
            scheduler->addTask(holdTask);
        }
        
        ~DmxCueListProcessor() {
            delete fadeAnimation;
            delete holdTask;
        }
        
        /**
         * Add a scene as a cue to the cue list
         * @param channels Map of channel -> value for this scene
         * @param fadeInMs Fade-in time for this scene in milliseconds
         * @param holdMs How long to hold this scene before auto-advancing
         */
        void addScene(const std::map<DmxCfg, uint8_t>& channels, 
                      unsigned long fadeInMs = 1000,
                      unsigned long holdMs = 5000) {
            cues.push_back({channels, fadeInMs, holdMs});
        }
        
        /**
         * Go to a specific cue
         */
        void goToCue(size_t cueIndex) {
            if (cueIndex >= cues.size()) return;
            if (dmxManager == nullptr) return;
            
            nextCueIndex = cueIndex;
            
            const Scene& targetScene = cues[nextCueIndex];
            auto& dmxData = dmxManager->getDmxData();
            
            // Prepare channel fade data
            std::vector<GenericFadeAnimation<DmxCfg>::ChannelFade> channelData;
            
            for (const auto& pair : targetScene.channels) {
                const DmxCfg& chId = pair.first;
                uint8_t targetValue = pair.second;
                
                // Get current value from DMX data
                uint8_t startValue = 0;
                auto universeIt = dmxData.find(chId.universe);
                if (universeIt != dmxData.end()) {
                    startValue = universeIt->second[chId.channel - 1];
                }
                
                channelData.push_back({chId, startValue, targetValue});
                Log::traceln("[DmxCueList] Ch %d@%d: %d -> %d", 
                           chId.channel, chId.universe, startValue, targetValue);
            }
            
            fadeAnimation->setChannels(channelData);
            fadeAnimation->setDuration(cues[nextCueIndex].fadeInMs);
            fadeAnimation->restart();
            
            Log::traceln("[DmxCueList] Going to cue %d with fade %lu ms", cueIndex, cues[nextCueIndex].fadeInMs);
        }
        
        /**
         * Go to next cue
         */
        void nextCue() {
            if (cues.empty()) return;
            size_t next = currentCueIndex + 1;
            if (next >= cues.size()) {
                next = loop ? 0 : currentCueIndex;
            }
            goToCue(next);
        }
        
        void setLoop(bool enable) { 
            loop = enable; 
        }
        
        void setEnabled(bool enable) override {
            if (enable && !wasEnabled && !cues.empty()) {
                // Transitioning from disabled to enabled - start first cue
                reset();
                goToCue(0);
                wasEnabled = true;
            } else if (!enable) {
                // Disabling - cancel scheduled hold task and stop fades
                holdTask->cancel();
                fadeAnimation->cancel();
                wasEnabled = false;
            }
            DmxProcessor::setEnabled(enable);
        }
        
        // no-op process since fading is handled by animation task
        void process(
            std::map<uint16_t, std::array<uint8_t, 512>>& dmxData,
            unsigned long currentTime
        ) override {
            // No processing needed here - fading handled by animation task
        }

        /**
         * Get set of universes this processor writes to
         */
        std::set<uint16_t> getUsedUniverses() const override {
            std::set<uint16_t> universes;
            for (const auto& cue : cues) {
                for (const auto& channelPair : cue.channels) {
                    universes.insert(channelPair.first.universe);
                }
            }
            return universes;
        }

        void reset() override {
            currentCueIndex = 0;
            nextCueIndex = 0;
            holdTask->cancel();
            fadeAnimation->cancel();
        }

};
