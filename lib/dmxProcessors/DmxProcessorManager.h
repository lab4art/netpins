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

        DmxManager* dmxManager;
        Scheduler* scheduler;

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
            if (procCfg.initialStateOn) {
                Log::infoln("  - Initial state: ON");
                processor->setEnabled(true);
            }
        }
        
        Log::infoln("DMX processor manager initialized.");
    }

    
    /**
     * Task callback - runs all enabled processors
     */
    void callback() override {
        // noop since each processor has its own scheduled tasks for fading and holds
    }
    
};
