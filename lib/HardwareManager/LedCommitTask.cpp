#include "LedCommitTask.h"
#include "HardwareManager.h"

LedCommitTask::LedCommitTask(HardwareManager* hardwareManager, unsigned long intervalMs)
    : ScheduledTask(intervalMs, "LedCommit"),
      hardwareManager(hardwareManager) {
}

void LedCommitTask::callback() {
    if (hardwareManager != nullptr) {
        hardwareManager->commitNeoStip();
    }
}
