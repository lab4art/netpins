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
 *   manager->addProcessorWithHysteresis(strobeProcessor, {0, 100}, 2.0f, 2.0f);  // Enable when ch 100 >= 2
 *   manager->addProcessorWithHysteresis(ambientProcessor, {0, 100}, 0.0f, 1.9f);  // Enable >= 0, disable < 1.9
 */
class DmxProcessorManager : public ScheduledTask {
private:
    struct ProcessorControl {
        DmxProcessor* processor;
        DmxCfg controlChannel;    // Channel to monitor
        float enableThreshold;     // Enable when >= this value
        float disableThreshold;    // Disable when < this value
        bool lastState;            // Last enabled state
        
        ProcessorControl(DmxProcessor* proc, DmxCfg ch, float enableTh, float disableTh)
            : processor(proc), controlChannel(ch), 
              enableThreshold(enableTh), disableThreshold(disableTh),
              lastState(false) {}
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
            if (procCfg.enableThreshold >= 0.0f) {
                processors.emplace_back(processor, procCfg.controlChannel, procCfg.enableThreshold, procCfg.disableThreshold);
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
            if (ctrl.enableThreshold < 0.0f) {
                ctrl.processor->setEnabled(true);
                ctrl.processor->process(dmxData, currentTime);
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
            
            // Determine if this is reverse threshold mode (low values enable)
            bool isReverse = (ctrl.enableThreshold < ctrl.disableThreshold);
            
            // Apply hysteresis logic
            bool shouldEnable = false;
            if (isReverse) {
                // Reverse mode: enable at LOW values, disable at HIGH values
                if (ctrl.lastState) {
                    // Currently enabled - stay enabled if below disable threshold
                    shouldEnable = (channelValue < ctrl.disableThreshold);
                } else {
                    // Currently disabled - enable if at or below enable threshold
                    shouldEnable = (channelValue <= ctrl.enableThreshold);
                }
            } else {
                // Normal mode: enable at HIGH values, disable at LOW values
                if (ctrl.lastState) {
                    // Currently enabled - stay enabled if at or above disable threshold
                    shouldEnable = (channelValue >= ctrl.disableThreshold);
                } else {
                    // Currently disabled - enable if at or above enable threshold
                    shouldEnable = (channelValue >= ctrl.enableThreshold);
                }
            }
            
            // Log::traceln("DmxProcessorManager: Processor '%s' control channel %d@%d ch value=%d, enableTh=%.2f, disableTh=%.2f, lastState=%d => shouldEnable=%d",
            //              ctrl.processor->getName(),
            //              ctrl.controlChannel.channel,
            //              ctrl.controlChannel.universe,
            //              channelValue,
            //              ctrl.enableThreshold,
            //              ctrl.disableThreshold,
            //              ctrl.lastState ? 1 : 0,
            //              shouldEnable ? 1 : 0);

            // Update processor state
            if (shouldEnable != ctrl.lastState) {
                ctrl.processor->setEnabled(shouldEnable);
                ctrl.lastState = shouldEnable;
            }
            
            // Run processor if enabled
            if (shouldEnable) {
                ctrl.processor->process(dmxData, currentTime);
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
