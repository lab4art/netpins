#pragma once

#include <scheduler.h>

// Forward declaration
class HardwareManager;

/**
 * LedCommitTask triggers LED hardware commits at scheduled intervals.
 * It calls HardwareManager's high-priority FreeRTOS task to ensure
 * timely LED updates without blocking.
 */
class LedCommitTask : public ScheduledTask {
private:
    HardwareManager* hardwareManager;
    
public:
    /**
     * Constructor
     * 
     * @param hardwareManager Pointer to HardwareManager instance
     * @param intervalMs Commit interval in milliseconds (default 20ms = 50Hz)
     */
    LedCommitTask(HardwareManager* hardwareManager, unsigned long intervalMs = 20);
    
    /**
     * ScheduledTask callback - triggers LED hardware commit
     */
    void callback() override;
};
