#pragma once

#include <Log.h>
#include "SensorProcessor.h"

/**
 * Motion State Processor - detects motion states with timing logic
 * Outputs simple state values that can be used by downstream processors
 * or DMX processors for effects like strobing.
 * 
 * Output States (hardcoded):
 * - 0: No motion detected (after timeout)
 * - 1: Motion detected (< persistence time)
 * - 2: Motion persisted (>= persistence time)
 * 
 * Example: Motion sensor outputs 1 for motion, 2 after 30s persistence,
 * then 0 after 30s timeout. DMX strobe processor can trigger when it sees state 2.
 */
class MotionStateProcessor : public SensorProcessor {
    private:
        // Configuration
        unsigned long persistenceTimeMs;   // Time before entering persisted state (e.g., 30000ms)
        unsigned long noMotionTimeoutMs;   // Time of no motion before reset (e.g., 30000ms)
        float threshold;                   // Motion detection threshold
        
        // State tracking
        unsigned long motionStartTime;     // When motion was first detected
        unsigned long lastMotionTime;      // Last time motion was detected
        bool motionActive;                 // Is motion currently detected?
        bool isPersisted;                  // Has motion persisted long enough?
        
    public:
        /**
         * Constructor
         * @param persistMs Time motion must persist before entering persisted state (milliseconds)
         * @param noMotionMs Time of no motion before reset (milliseconds)
         * @param thresh Motion detection threshold (default: 0.0)
         */
        MotionStateProcessor(
            unsigned long persistMs = 30000,
            unsigned long noMotionMs = 30000,
            float thresh = 0.0f
        )
            : SensorProcessor("MotionState"),
              persistenceTimeMs(persistMs),
              noMotionTimeoutMs(noMotionMs),
              threshold(thresh),
              motionStartTime(0),
              lastMotionTime(0),
              motionActive(false),
              isPersisted(false) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            unsigned long currentTime = data.timestamp;
            
            // Check if motion is detected
            bool currentMotion = data.value > threshold;
            
            if (currentMotion) {
                // Motion detected
                if (!motionActive) {
                    // Motion just started
                    motionStartTime = currentTime;
                    motionActive = true;
                    isPersisted = false;
                }
                
                lastMotionTime = currentTime;
                
                // Check if motion has persisted long enough
                unsigned long motionDuration = currentTime - motionStartTime;
                
                if (motionDuration >= persistenceTimeMs) {
                    // Motion has persisted - output state 2
                    isPersisted = true;
                    result.value = 2.0f;
                } else {
                    // Normal motion - output state 1
                    isPersisted = false;
                    result.value = 1.0f;
                }
            } else {
                // No motion detected
                
                // Check if we've exceeded the no-motion timeout
                unsigned long noMotionDuration = currentTime - lastMotionTime;
                
                if (motionActive && noMotionDuration >= noMotionTimeoutMs) {
                    // Timeout exceeded - reset everything
                    motionActive = false;
                    isPersisted = false;
                    motionStartTime = 0;
                    result.value = 0.0f;
                } else if (motionActive) {
                    // Within timeout period - maintain current state
                    result.value = isPersisted ? 2.0f : 1.0f;
                } else {
                    // No active motion state
                    result.value = 0.0f;
                }
            }
            // Log::traceln("MotionStateProcessor: input=%.2f, motion=%d, persisted=%d, output=%.2f",
            //           data.value, currentMotion ? 1 : 0, isPersisted ? 1 : 0, result.value);
            return result;
        }
        
        void reset() override {
            motionActive = false;
            isPersisted = false;
            motionStartTime = 0;
            lastMotionTime = 0;
        }
        
        // Getters for debugging/monitoring
        bool isMotionActive() const { return motionActive; }
        bool isMotionPersisted() const { return isPersisted; }
        unsigned long getMotionDuration(unsigned long currentTime) const {
            return motionActive ? (currentTime - motionStartTime) : 0;
        }
};

// Legacy alias for backward compatibility
using StrobeProcessor = MotionStateProcessor;