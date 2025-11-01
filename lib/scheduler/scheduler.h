#pragma once

#include <functional>
#include <vector>
#include <Arduino.h>

class ScheduledTask {
    private:
        // std::function<void()> callbackFunc = []() {};
        long interval;
        unsigned long lastExecution = 0;
        
    public:
        ScheduledTask(long interval):
            interval(interval) {
        }

        virtual void callback() {}

        void loop() {
            if (interval < 0) {
                return;
            }
            if (millis() - lastExecution >= interval) {
                lastExecution = millis();
                callback();
            }
        }

        void disable() {
            this->interval = -1;
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
