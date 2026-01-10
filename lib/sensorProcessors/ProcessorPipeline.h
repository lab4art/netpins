#pragma once

#include "SensorProcessor.h"
#include <vector>
#include <memory>

/**
 * Pipeline that chains multiple sensor processors together
 * Data flows through processors in sequence: input -> P1 -> P2 -> P3 -> output
 */
class ProcessorPipeline {
    private:
        std::vector<std::shared_ptr<SensorProcessor>> processors;
        std::string name;
        bool enabled;
        
    public:
        ProcessorPipeline(const std::string& pipelineName = "Pipeline")
            : name(pipelineName), enabled(true) {}
        
        /**
         * Add a processor to the end of the pipeline
         */
        void addProcessor(std::shared_ptr<SensorProcessor> processor) {
            if (processor) {
                processors.push_back(processor);
                Log::traceln("Pipeline '%s': Added processor '%s' (position %d)", 
                           name.c_str(), processor->getName().c_str(), processors.size());
            }
        }
        
        /**
         * Insert a processor at specific position
         */
        void insertProcessor(size_t index, std::shared_ptr<SensorProcessor> processor) {
            if (processor && index <= processors.size()) {
                processors.insert(processors.begin() + index, processor);
                Log::traceln("Pipeline '%s': Inserted processor '%s' at position %d", 
                           name.c_str(), processor->getName().c_str(), index);
            }
        }
        
        /**
         * Remove processor at specific position
         */
        void removeProcessor(size_t index) {
            if (index < processors.size()) {
                Log::traceln("Pipeline '%s': Removed processor '%s' from position %d", 
                           name.c_str(), processors[index]->getName().c_str(), index);
                processors.erase(processors.begin() + index);
            }
        }
        
        /**
         * Remove all processors with given name
         */
        void removeProcessorByName(const std::string& processorName) {
            auto it = processors.begin();
            while (it != processors.end()) {
                if ((*it)->getName() == processorName) {
                    Log::traceln("Pipeline '%s': Removed processor '%s'", 
                               name.c_str(), processorName.c_str());
                    it = processors.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        /**
         * Clear all processors from the pipeline
         */
        void clear() {
            Log::traceln("Pipeline '%s': Cleared all processors", name.c_str());
            processors.clear();
        }
        
        /**
         * Process sensor data through the entire pipeline
         */
        SensorData process(float rawValue, unsigned long timestamp = 0) {
            if (!enabled) {
                return SensorData(rawValue, timestamp);
            }
            
            if (timestamp == 0) {
                timestamp = millis();
            }
            
            SensorData data(rawValue, timestamp);
            
            // Pass data through each processor in sequence
            for (auto& processor : processors) {
                if (processor->isEnabled()) {
                    data = processor->process(data);
                }
            }
            
            return data;
        }
        
        /**
         * Process using existing SensorData
         */
        SensorData process(const SensorData& data) {
            if (!enabled) {
                return data;
            }
            
            SensorData result = data;
            
            // Pass data through each processor in sequence
            for (auto& processor : processors) {
                if (processor->isEnabled()) {
                    result = processor->process(result);
                }
            }
            
            return result;
        }
        
        /**
         * Reset all processors in the pipeline
         */
        void reset() {
            for (auto& processor : processors) {
                processor->reset();
            }
            Log::traceln("Pipeline '%s': Reset all processors", name.c_str());
        }
        
        /**
         * Get number of processors in pipeline
         */
        size_t size() const {
            return processors.size();
        }
        
        /**
         * Check if pipeline is empty
         */
        bool isEmpty() const {
            return processors.empty();
        }
        
        /**
         * Get processor at specific index
         */
        std::shared_ptr<SensorProcessor> getProcessor(size_t index) {
            if (index < processors.size()) {
                return processors[index];
            }
            return nullptr;
        }
        
        /**
         * Get all processors
         */
        const std::vector<std::shared_ptr<SensorProcessor>>& getProcessors() const {
            return processors;
        }
        
        /**
         * Enable/disable entire pipeline
         */
        void setEnabled(bool enable) {
            enabled = enable;
            Log::traceln("Pipeline '%s': %s", name.c_str(), enable ? "Enabled" : "Disabled");
        }
        
        bool isEnabled() const {
            return enabled;
        }
        
        /**
         * Get pipeline name
         */
        const std::string& getName() const {
            return name;
        }
        
        /**
         * Print pipeline structure for debugging
         */
        void printStructure() const {
            Log::infoln("Pipeline '%s' (%s, %d processors):", 
                       name.c_str(), enabled ? "enabled" : "disabled", processors.size());
            for (size_t i = 0; i < processors.size(); i++) {
                auto& p = processors[i];
                Log::infoln("  [%d] %s (%s)", i, p->getName().c_str(), 
                          p->isEnabled() ? "enabled" : "disabled");
            }
        }
};

/**
 * Builder class for creating pipelines with fluent interface
 */
class PipelineBuilder {
    private:
        ProcessorPipeline pipeline;
        
    public:
        PipelineBuilder(const std::string& name = "Pipeline") : pipeline(name) {}
        
        PipelineBuilder& add(std::shared_ptr<SensorProcessor> processor) {
            pipeline.addProcessor(processor);
            return *this;
        }
        
        template<typename T, typename... Args>
        PipelineBuilder& add(Args&&... args) {
            auto processor = std::make_shared<T>(std::forward<Args>(args)...);
            pipeline.addProcessor(processor);
            return *this;
        }
        
        ProcessorPipeline build() {
            return std::move(pipeline);
        }
        
        ProcessorPipeline* buildPtr() {
            return new ProcessorPipeline(std::move(pipeline));
        }
};
