#pragma once

#include "DmxProcessor.h"
#include "DmxSequenceProcessor.h"
#include "DmxManager.h"
#include "scheduler.h"
#include "Log.h"
#include <vector>
#include <map>
#include <set>

class DmxProcessorManager : public ScheduledTask {
    private:
        struct TriggerBinding {
            std::string name;
            DmxCfg controlChannel;
            int minValue;
            int maxValue;
            DmxProcessor* processor;
            bool lastEnabled;
        };

        DmxManager* dmxManager;
        Scheduler* scheduler;
        std::map<std::string, DmxProcessor*> namedProcessors;
        std::vector<TriggerBinding> triggerBindings;

        void addTriggerBinding(const std::string& triggerName,
                               const DmxCfg& controlChannel,
                               int minValue,
                               int maxValue,
                               DmxProcessor* processor) {
            if (processor == nullptr) {
                return;
            }
            if (controlChannel.channel < 1 || controlChannel.channel > 512) {
                Log::warningln("Skipping trigger '%s': invalid control channel %u@%u", triggerName.c_str(), controlChannel.channel, controlChannel.universe);
                return;
            }

            dmxManager->addUniverse(controlChannel.universe);

            triggerBindings.push_back({
                triggerName,
                controlChannel,
                minValue,
                maxValue,
                processor,
                false
            });

            Log::infoln("  - Added trigger '%s': %u@%u in [%d, %d]", triggerName.c_str(), controlChannel.channel, controlChannel.universe, minValue, maxValue);
        }

public:
    /**
     * Create a DMX processor manager
     * @param dmxMgr Pointer to DmxManager instance
     * @param sched Pointer to Scheduler instance
     * @param intervalMs Task interval in milliseconds (default: 20ms for smooth effects)
     */
    DmxProcessorManager(DmxManager* dmxMgr, Scheduler* sched, unsigned long intervalMs = 20)
        : ScheduledTask(intervalMs, "DmxProcMgr"),
          dmxManager(dmxMgr),
          scheduler(sched) {}
    
    void initialize(const Settings& settings) {
        namedProcessors.clear();
        triggerBindings.clear();

        if (settings.dmxProcessors.empty()) {
            Log::info("No DMX processors configured");
            return;
        }
        
        Log::infoln("Initializing DMX processors ...");
        
        // Create all processors (templates registered automatically by factory)
        for (const auto& procCfg : settings.dmxProcessors) {
            Log::infoln("Creating DMX processor: type=%s, name=%s", 
                        procCfg.type.c_str(), 
                        procCfg.name.empty() ? "(unnamed)" : procCfg.name.c_str());
            
            DmxProcessor* processor = DmxProcessorFactory::createProcessor(procCfg, scheduler, dmxManager);
            if (processor == nullptr) {
                Log::errorln("Failed to create DMX processor of type: %s", procCfg.type.c_str());
                continue;
            }
            
            // Register universes used by this processor
            std::set<uint16_t> universes = processor->getUsedUniverses();
            for (uint16_t universe : universes) {
                Log::infoln("  - Adding output universe %d", universe);
                dmxManager->addUniverse(universe);
            }
            if (!procCfg.name.empty()) {
                namedProcessors[procCfg.name] = processor;
            }
            if (procCfg.initialStateOn) {
                Log::infoln("  - Initial state: ON");
                processor->setEnabled(true);
            }
        }

        // Trigger configuration via dmx_triggers
        for (const auto& trigCfg : settings.dmxTriggers) {
            auto procIt = namedProcessors.find(trigCfg.sequence);
            if (procIt == namedProcessors.end()) {
                Log::warningln("Skipping dmx trigger '%s': target sequence '%s' not found", trigCfg.name.c_str(), trigCfg.sequence.c_str());
                continue;
            }

            std::string triggerName = trigCfg.name.empty() ? trigCfg.sequence : trigCfg.name;
            addTriggerBinding(triggerName, trigCfg.controlChannel, trigCfg.minValue, trigCfg.maxValue, procIt->second);
        }
        
        Log::infoln("DMX processor manager initialized. triggers=%d", triggerBindings.size());
    }

    
    /**
     * Task callback - runs all enabled processors
     */
    void callback() override {
        if (triggerBindings.empty()) {
            return;
        }

        auto& dmxData = dmxManager->getDmxData();
        for (auto& binding : triggerBindings) {
            bool shouldEnable = false;
            auto uniIt = dmxData.find(binding.controlChannel.universe);
            if (uniIt != dmxData.end()) {
                uint8_t value = uniIt->second[binding.controlChannel.get0BasedChannel()];
                shouldEnable = value >= binding.minValue && value <= binding.maxValue;
            }

            if (shouldEnable != binding.lastEnabled) {
                binding.processor->setEnabled(shouldEnable);
                binding.lastEnabled = shouldEnable;
                Log::traceln("[DmxTrigger] %s -> %s", binding.name.c_str(), shouldEnable ? "ON" : "OFF");
            }
        }
    }
    
};
