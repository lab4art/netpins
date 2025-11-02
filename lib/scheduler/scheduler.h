#pragma once

#include <functional>
#include <vector>
#include <Arduino.h>

class ScheduledTask {
    private:
        // std::function<void()> callbackFunc = []() {};
        unsigned long interval;
        unsigned long lastExecution = 0;
        boolean enabled;
        String name;

    public:
        ScheduledTask(unsigned long interval, String name, boolean enabled = true):
            interval(interval),
            enabled(enabled),
            name(name) {
        }

        virtual void callback() {}

        void loop() {
            if (!enabled || interval == 0) {
                return;
            }
            if (millis() - lastExecution >= interval) {
                lastExecution = millis();
                // Log.traceln("Scheduling: %s", name.c_str());
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
