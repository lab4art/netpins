#pragma once

#include "DmxProcessor.h"
#include "settings.h"
#include "animations.h"
#include <vector>
#include <set>
#include <map>
#include <string>


/**
 * Support recursive cofig, without actual recursion of function calls.
 * 
 * When entering an include, push the current sequence (or just the reference or id) state onto a stack,
 * and pop the stack when the included sequence ends.
 * 
 * Craete a stack of hold timers to handle duration. No need for Animation stack, includes does not fade.
 * 
 */

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
    static std::map<std::string, DmxSequenceProcessor*>& getSequenceProcessorRegistry() {
        static std::map<std::string, DmxSequenceProcessor*> registry;
        return registry;
    }
    private:
        struct Cue {
            std::map<DmxCfg, uint8_t> channels;  // Map of channel -> value (empty if include)
            std::string includeName;              // Name of included sequence (for reference only)
            unsigned long fadeInMs;               // Fade-in time for this cue
            unsigned long holdTimeMs;             // How long to hold/play include before next cue
            bool isInclude() const { return !includeName.empty(); }
        };
        
        std::vector<Cue> cues;
        size_t currentCueIndex;

        OneShotTask* holdTask;
        
        DmxSequenceProcessor* activeIncludeSequence;
        OneShotTask* includeDurationTask;


        // Fade state
        GenericFadeAnimation<DmxCfg>* fadeAnimation = nullptr;
        // Stack of hold tasks, one per context/include depth
        Scheduler* scheduler;
        DmxManager* dmxManager;  // Reference to DMX manager for accessing data

        bool loop;
        bool wasEnabled;

        std::string sequenceName;

        std::function<void()> onEnd = []() {};
        
    public:
        DmxSequenceProcessor(Scheduler* sched, DmxManager* dmxMgr)
            : DmxProcessor("DmxSequence"),
              scheduler(sched),
              dmxManager(dmxMgr),
              loop(true),
              wasEnabled(false) {
            fadeAnimation = new GenericFadeAnimation<DmxCfg>(50, false);
            fadeAnimation->setName("DmxCueFade");

            fadeAnimation->setCallback([this](const DmxCfg& chId, uint8_t value) {
                auto& dmxData = dmxManager->getDmxData();
                dmxData[chId.universe][chId.channel - 1] = value;
            });

            fadeAnimation->setOnEnd([this]() {
                if (!enabled) return;
                // enable hold timer if holdMs > 0
                unsigned long holdMs = cues[currentCueIndex].holdTimeMs;
                if (holdMs > 0) {
                    holdTask->arm(holdMs);
                } else {
                    nextCue();
                }
            });

            fadeAnimation->schedule(scheduler);

            holdTask = new OneShotTask([this]() {
                if (!enabled) return;
                nextCue();
            });
            scheduler->addTask(holdTask);

            includeDurationTask = new OneShotTask([this]() {
                if (!enabled) return;
                // stop included sequence
                if (activeIncludeSequence) {
                    activeIncludeSequence->setEnabled(false);
                    activeIncludeSequence = nullptr;
                }
                nextCue();
            });
            scheduler->addTask(includeDurationTask);
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
                          unsigned long holdMs = 0) {
            auto& registry = getSequenceProcessorRegistry();
            if (registry.find(includeName) == registry.end()) {
                Log::errorln("[DmxSequence] Sequence include not found: %s", includeName.c_str());
                return;
            }
            cues.push_back({{}, includeName, fadeInMs, holdMs});
        }

        void addSequenceToRegistry(const std::string& name) {
            sequenceName = name;
            auto& registry = getSequenceProcessorRegistry();
            registry[name] = this;
            Log::infoln("[DmxSequence] Added sequence: %s", name.c_str());
        }

        void setEnabled(bool enable) override {
            if (enable && !wasEnabled && !cues.empty()) {
                goToCue(0);
                wasEnabled = true;
            }
            if (!enable) {
                fadeAnimation->cancel();
                holdTask->cancel();
                includeDurationTask->cancel();
                if (activeIncludeSequence) {
                    activeIncludeSequence->setEnabled(false);
                    activeIncludeSequence = nullptr;
                }
                currentCueIndex = 0;
                onEnd();
                wasEnabled = false;
            }

            DmxProcessor::setEnabled(enable);
        }

    private:

        void goToCue(size_t cueIndex) {
            if (cueIndex >= cues.size()) return;
            if (dmxManager == nullptr) return;

             // TODO verify index increments
            const Cue& targetCue = cues[cueIndex];

            if (targetCue.isInclude()) {
                activeIncludeSequence = startInclude(targetCue.includeName);
                if (activeIncludeSequence == nullptr) {
                    // Failed to start include, skip to next cue
                    nextCue();
                    return;
                }
                includeDurationTask->arm(targetCue.holdTimeMs);
                return;
            }

            auto channelData = getDmxChannels(targetCue);
            fadeAnimation->setChannels(channelData);
            fadeAnimation->setDuration(targetCue.fadeInMs);
            fadeAnimation->restart();
            currentCueIndex = cueIndex;
        }

        /**
         * Go to next cue
         */
        void nextCue() {
            if (cues.empty()) return;
            size_t next = currentCueIndex + 1;
            if (next >= cues.size()) {
                next = loop ? 0 : cues.size();
            }

            if (next == cues.size()) {
                // End of sequence reached
                setEnabled(false);
            } else {
                goToCue(next);
            }
        }
        
        /**
         * Start playing a subsection (included sequence).
         * DmxProcessorManager must make sure only one sequence is running at the time, although trigger conditions may overlap.
         */
        DmxSequenceProcessor* startInclude(const std::string& includeName) {
            auto& registry = getSequenceProcessorRegistry();
            auto it = registry.find(includeName);
            if (it == registry.end()) {
                Log::errorln("[DmxSequence] Template not found for include: %s", includeName.c_str());
                return nullptr;
            }
            DmxSequenceProcessor* included = it->second;
            included->setOnEnd([this]() {
                Log::traceln("[DmxSequence] Included sequence ended, continuing parent");
                nextCue();
            });
            included->setEnabled(true);
            return included;
        }
        
    public:

        std::vector<GenericFadeAnimation<DmxCfg>::ChannelFade> getDmxChannels(const DmxSequenceProcessor::Cue &targetCue) {
            auto& dmxData = dmxManager->getDmxData();
            std::vector<GenericFadeAnimation<DmxCfg>::ChannelFade> channelData;
            for (const auto &pair : targetCue.channels)
            {
                const DmxCfg &chId = pair.first;
                uint8_t targetValue = pair.second;
                uint8_t startValue = 0;
                auto universeIt = dmxData.find(chId.universe);
                if (universeIt != dmxData.end())
                {
                    startValue = universeIt->second[chId.channel - 1];
                }
                channelData.push_back({chId, startValue, targetValue});
            }
            return channelData;
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

        void setLoop(bool enable) { 
            loop = enable; 
        }
        
        void setOnEnd(std::function<void()> onEndCallback) {
            onEnd = onEndCallback;
        }
};
