#pragma once

#include "DmxProcessor.h"
#include "DmxProcessorFactory.h"
#include "DmxManager.h"
#include "scheduler.h"
#include "settings.h"
#include "Log.h"
#include <vector>
#include <map>
#include <set>

/**
 * DMX Processor Manager
 * 
 * Manages a collection of DMX processors and controls their execution
 * based on DMX channel values set by sensors.
 * 
 * Use cases:
 * - Enable strobe when motion state reaches 2
 * - Switch cues based on temperature thresholds
 * - Control effects via sensor-driven DMX channels
 * 
 * Example:
 *   auto manager = new DmxProcessorManager(dmxManager);
 *   manager->addProcessor(strobeProcessor, {0, 100}, 0, 3);  // Enable when ch 100 is between 0-3
 *   manager->addProcessor(ambientProcessor, {0, 100}, 3, 3);  // Enable when ch 100 is exactly 3
 */
class DmxProcessorManager : public ScheduledTask {
private:
    struct ProcessorControl {
        DmxProcessor* processor;
        DmxCfg controlChannel;    // Channel to monitor
        int minValue;              // Minimum value for range (inclusive)
        int maxValue;              // Maximum value for range (inclusive)
        
        ProcessorControl(DmxProcessor* proc, DmxCfg ch, int minVal, int maxVal)
            : processor(proc), controlChannel(ch), 
              minValue(minVal), maxValue(maxVal) {}
    };
    
    DmxManager* dmxManager;
    Scheduler* scheduler;
    std::vector<ProcessorControl> processors;
    
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
    
    /**
     * Destructor - cleans up all managed processors
     */
    ~DmxProcessorManager() {
        for (auto& ctrl : processors) {
            delete ctrl.processor;
        }
    }
    
    /**
     * Initialize processors from configuration
     * Creates processors using DmxProcessorFactory and adds them to the manager
     * Sequences with names are automatically registered as templates for includes.
     * Only sequences with valid control channels (threshold >= 0) are added to the manager.
     * 
     * @param settings Settings containing dmxProcessors configuration
     */
    void initialize(const Settings& settings) {
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
            
            // Only add to managed processors if it has a valid control channel
            if (procCfg.minValue >= 0) {
                processors.emplace_back(processor, procCfg.controlChannel, procCfg.minValue, procCfg.maxValue);
            }
            // Note: Processors without control channels are templates only,
            // controlled by parent sequences via includes
        }
        
        Log::infoln("DMX processor manager initialized with %d active processors", processors.size());
    }

    
    /**
     * Task callback - runs all enabled processors
     */
    void callback() override {
        auto& dmxData = dmxManager->getDmxData();
        unsigned long currentTime = millis();
        
        // Update processor states based on control channels
        for (auto& ctrl : processors) {
            // Check if this is an always-on processor
            if (ctrl.minValue < 0) {
                ctrl.processor->setEnabled(true);
                ctrl.processor->process(currentTime);
                continue;
            }
            
            // Read control channel value
            auto universeIt = dmxData.find(ctrl.controlChannel.universe);
            if (universeIt == dmxData.end()) {
                // Universe doesn't exist - disable processor
                ctrl.processor->setEnabled(false);
                continue;
            }
            
            uint8_t channelValue = universeIt->second[ctrl.controlChannel.channel - 1];
            
            // Check if channel value is within the enabled range
            bool shouldEnable = (channelValue >= ctrl.minValue && channelValue <= ctrl.maxValue);
            
            // Update processor state
            ctrl.processor->setEnabled(shouldEnable);
            
            // Run processor if enabled
            if (shouldEnable) {
                ctrl.processor->process(currentTime);
            }
        }
    }
    
    /**
     * Remove all processors and delete them
     */
    void clear() {
        for (auto& ctrl : processors) {
            delete ctrl.processor;
        }
        processors.clear();
    }
};
