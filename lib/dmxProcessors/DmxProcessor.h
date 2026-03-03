#pragma once

#include <cstdint>
#include <array>
#include <map>
#include <set>

/**
 * Base class for DMX processors
 * 
 * DMX processors operate on DMX universe data, transforming channels
 * from source to target. They can apply effects like strobing, fading,
 * cue lists, color mixing, etc.
 * 
 * Unlike sensor processors which transform sensor readings, DMX processors
 * transform DMX channel values directly.
 */
class DmxProcessor {
    protected:
        const char* name;
        bool enabled;
        
    public:
        DmxProcessor(const char* processorName) 
            : name(processorName), enabled(true) {}
        
        virtual ~DmxProcessor() = default;
        
        /**
         * Process DMX data
         * @param currentTime Current timestamp in milliseconds
         */
        // virtual void process(unsigned long currentTime) = 0;
        
        /**
         * Enable or disable the processor
         */
        virtual void setEnabled(bool enable) { enabled = enable; }
        bool isEnabled() const { return enabled; }
        
        /**
         * Get processor name
         */
        const char* getName() const { return name; }
        
        /**
         * Get set of universes this processor writes to
         * Override this if processor outputs to DMX universes
         */
        virtual std::set<uint16_t> getUsedUniverses() const {
            return std::set<uint16_t>();  // Default: no universes
        }
};
