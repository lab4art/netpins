#pragma once

#include "DmxProcessor.h"
#include "settings.h"
#include "animations.h"
#include <vector>
#include <set>
#include <map>
#include <string>

/**
 * DMX Cue List Processor with Fade and Nested Sequences
 * 
 * Manages a list of DMX cues with smooth fading between them.
 * Each cue is a configurable map of DMX channels to values with fade-in time.
 * Supports nested sequences via include references.
 * Can be triggered manually, by time, or by DMX channel values.
 * 
 * Example: Create lighting cues that fade smoothly from one to another
 */
class DmxSequenceProcessor : public DmxProcessor {
    // Static sequence template registry for includes
    static std::map<std::string, DmxSequenceProcessor*>& getSequenceRegistry() {
        static std::map<std::string, DmxSequenceProcessor*> registry;
        return registry;
    }
    private:
        struct Cue {
            std::map<DmxCfg, uint8_t> channels;  // Map of channel -> value (empty if include)
            std::string includeName;              // Name of included sequence (empty if channels)
            unsigned long fadeInMs;               // Fade-in time for this cue
            unsigned long holdTimeMs;             // How long to hold/play include before next cue
            
            bool isInclude() const { return !includeName.empty(); }
        };
        
        std::vector<Cue> cues;
        size_t currentCueIndex;
        size_t nextCueIndex;
        
        // Fade state
        GenericFadeAnimation<DmxCfg>* fadeAnimation;
        OneShotTask* holdTask;
        Scheduler* scheduler;
        DmxManager* dmxManager;  // Reference to DMX manager for accessing data
        
        bool loop;
        bool wasEnabled;
        
        // Subsection playback state (for include cues)
        bool playingSubsection;
        DmxSequenceProcessor* currentSubsection;
        size_t subsectionParentCue;     // Parent cue index that triggered subsection
        unsigned long subsectionStartTime;
        unsigned long subsectionDuration;
        OneShotTask* subsectionEndTask;
        
        std::string templateName;  // Name if this is a template sequence
        
    public:
        DmxSequenceProcessor(Scheduler* sched, DmxManager* dmxMgr)
            : DmxProcessor("DmxSequence"),
              currentCueIndex(0),
              nextCueIndex(0),
              fadeAnimation(nullptr),
              holdTask(nullptr),
              scheduler(sched),
              dmxManager(dmxMgr),
              loop(true),
              wasEnabled(false),
              playingSubsection(false),
              currentSubsection(nullptr),
              subsectionParentCue(0),
              subsectionStartTime(0),
              subsectionDuration(0),
              subsectionEndTask(nullptr) {
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
                // Log::traceln("[DmxSequence] Fade complete, now at cue %d", currentCueIndex);
                
                // Check if this is an include cue
                if (enabled && !cues.empty() && cues[currentCueIndex].isInclude()) {
                    startSubsection(currentCueIndex);
                } else if (enabled && !cues.empty()) {
                    // Regular cue - schedule next cue after hold time
                    unsigned long holdMs = cues[currentCueIndex].holdTimeMs;
                    if (holdMs > 0 && (loop || currentCueIndex < cues.size() - 1)) {
                        holdTask->arm(holdMs);
                        // Log::traceln("[DmxSequence] Scheduled next cue in %lu ms", holdMs);
                    }
                }
            });
            
            // Create hold task that advances to next cue
            holdTask = new OneShotTask(0, [this]() {
                if (enabled) {
                    nextCue();
                }
            });
            
            // Create subsection end task
            subsectionEndTask = new OneShotTask(0, [this]() {
                if (playingSubsection && enabled) {
                    stopSubsection();
                }
            });
            
            fadeAnimation->schedule(scheduler);
            scheduler->addTask(holdTask);
            scheduler->addTask(subsectionEndTask);
        }
        
        ~DmxSequenceProcessor() {
            delete fadeAnimation;
            delete holdTask;
            delete subsectionEndTask;
        }
        
        /**
         * Add a cue to the cue list
         * @param channels Map of channel -> value for this cue
         * @param fadeInMs Fade-in time for this cue in milliseconds
         * @param holdMs How long to hold this cue before auto-advancing
         */
        void addCue(const std::map<DmxCfg, uint8_t>& channels, 
                      unsigned long fadeInMs = 1000,
                      unsigned long holdMs = 5000) {
            cues.push_back({channels, "", fadeInMs, holdMs});
        }
        
        /**
         * Add an include cue (references another sequence)
         * @param includeName Name of sequence to include
         * @param fadeInMs Fade-in time (currently unused for includes)
         * @param holdMs How long to play the included sequence before next cue
         */
        void addIncludeCue(const std::string& includeName,
                          unsigned long fadeInMs = 0,
                          unsigned long holdMs = 5000) {
            cues.push_back({{}, includeName, fadeInMs, holdMs});
        }
        
        /**
         * Go to a specific cue
         */
        void goToCue(size_t cueIndex) {
            if (cueIndex >= cues.size()) return;
            if (dmxManager == nullptr) return;
            
            // Stop any active subsection before transitioning
            if (playingSubsection) {
                subsectionEndTask->cancel();
                if (currentSubsection) {
                    currentSubsection->fadeAnimation->cancel();
                    currentSubsection->holdTask->cancel();
                }
                playingSubsection = false;
                currentSubsection = nullptr;
            }
            
            nextCueIndex = cueIndex;
            
            const Cue& targetCue = cues[nextCueIndex];
            
            // Check if this is an include cue
            if (targetCue.isInclude()) {
                Log::traceln("[DmxSequence] Going to include cue %d: %s", cueIndex, targetCue.includeName.c_str());
                // Trigger fade callback immediately for includes (no fade needed)
                currentCueIndex = nextCueIndex;
                startSubsection(cueIndex);
                return;
            }
            
            auto& dmxData = dmxManager->getDmxData();
            
            // Prepare channel fade data
            std::vector<GenericFadeAnimation<DmxCfg>::ChannelFade> channelData;
            
            for (const auto& pair : targetCue.channels) {
                const DmxCfg& chId = pair.first;
                uint8_t targetValue = pair.second;
                
                // Get current value from DMX data
                uint8_t startValue = 0;
                auto universeIt = dmxData.find(chId.universe);
                if (universeIt != dmxData.end()) {
                    startValue = universeIt->second[chId.channel - 1];
                }
                
                channelData.push_back({chId, startValue, targetValue});
                // Log::traceln("[DmxSequence] Ch %d@%d: %d -> %d", chId.channel, chId.universe, startValue, targetValue);
            }
            
            fadeAnimation->setChannels(channelData);
            fadeAnimation->setDuration(cues[nextCueIndex].fadeInMs);
            fadeAnimation->restart();
            
            // Log::traceln("[DmxSequence] Going to cue %d with fade %lu ms", cueIndex, cues[nextCueIndex].fadeInMs);
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
        
    private:
        /**
         * Start playing a subsection (included sequence)
         */
        void startSubsection(size_t parentCueIndex) {
            const Cue& cue = cues[parentCueIndex];
            if (!cue.isInclude()) return;
            
            auto& registry = getSequenceRegistry();
            auto it = registry.find(cue.includeName);
            if (it == registry.end()) {
                Log::errorln("[DmxSequence] Include not found: %s", cue.includeName.c_str());
                // Skip to next cue
                nextCue();
                return;
            }
            
            Log::traceln("[DmxSequence] Starting subsection: %s for %lu ms", cue.includeName.c_str(), cue.holdTimeMs);
            
            playingSubsection = true;
            currentSubsection = it->second;
            subsectionParentCue = parentCueIndex;
            subsectionDuration = cue.holdTimeMs;
            subsectionStartTime = millis();
            
            // Start the subsection sequence (don't use setEnabled, manage directly)
            currentSubsection->reset();
            currentSubsection->goToCue(0);
            
            // Schedule end of subsection
            if (subsectionDuration > 0) {
                subsectionEndTask->arm(subsectionDuration);
            }
        }
        
        /**
         * Stop playing subsection and continue parent sequence
         */
        void stopSubsection() {
            if (!playingSubsection) return;
            
            Log::traceln("[DmxSequence] Subsection ended, continuing parent");
            
            playingSubsection = false;
            subsectionEndTask->cancel();
            
            // Stop subsection completely - cancel all tasks
            if (currentSubsection) {
                currentSubsection->fadeAnimation->cancel();
                currentSubsection->holdTask->cancel();
            }
            currentSubsection = nullptr;
            
            // Continue parent sequence
            nextCue();
        }
        
    public:
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
                
                // Stop subsection if playing
                if (playingSubsection) {
                    stopSubsection();
                }
                
                wasEnabled = false;
            }
            DmxProcessor::setEnabled(enable);
        }
        
        // no-op process since fading is handled by animation task
        void process(unsigned long currentTime) override {
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
            
            if (playingSubsection) {
                stopSubsection();
            }
        }
        
        /**
         * Register this sequence as a template for includes
         */
        void registerAsTemplate(const std::string& name) {
            templateName = name;
            auto& registry = getSequenceRegistry();
            registry[name] = this;
            Log::infoln("[DmxSequence] Registered template: %s", name.c_str());
        }
        
        /**
         * Get template name
         */
        const std::string& getTemplateName() const {
            return templateName;
        }

};
