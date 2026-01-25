#pragma once

#include <functional>
#include <vector>
#include <string>
#include <Log.h>

class ScheduledTask {
    private:
        // std::function<void()> callbackFunc = []() {};
        unsigned long interval;
        unsigned long lastExecution = 0;
        bool enabled;
        std::string name;

    public:
        ScheduledTask(unsigned long interval, std::string name, bool enabled = true):
            interval(interval),
            enabled(enabled),
            name(name) {
        }

        virtual void callback() {}

        void loop() {
            if (!enabled) {
                // Log::traceln("Task %s skipped: enabled=%d, interval=%lu", name.c_str(), enabled, interval);
                return;
            }
            // Usefull for non-blocking tasks
            if (interval == 0) {
                callback();
                return;
            }
            // Log::traceln("Checking task: %s, interval: %lu, lastExecution: %lu, currentMillis: %lu", name.c_str(), interval, lastExecution, millis());
            if (millis() - lastExecution >= interval) {
                // Log::traceln("Executing task: %s", name.c_str());
                lastExecution = millis();
                callback();
            }
        }

        void enable() {
            this->enabled = true;
        }

        void disable() {
            this->enabled = false;
        }

};

/**
 * One-shot task that executes after a delay and then disables itself
 */
class OneShotTask : public ScheduledTask {
private:
    std::function<void()> func;
    unsigned long executeAt;
    bool fired;
    
public:
    OneShotTask(unsigned long delayMs, std::function<void()> callback)
        : ScheduledTask(0, "OneShot", false),
          func(callback),
          executeAt(0),
          fired(false) {
    }
    
    void arm(unsigned long delayMs) {
        executeAt = millis() + delayMs;
        fired = false;
        enable();
    }
    
    void cancel() {
        disable();
        fired = true;
    }
    
    void callback() override {
        if (fired) return;
        if (millis() >= executeAt) {
            fired = true;
            disable();
            func();
        }
    }
};

class Scheduler {
    private:
        std::vector<ScheduledTask*> tasks;

    public:
        Scheduler() {
        }

        void addTask(ScheduledTask* task) {
            tasks.push_back(task);
        }

        void loop() {
            for (auto task : tasks) {
                task->loop();
            }
        }
};
