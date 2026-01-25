#pragma once

/**
 * Sensor Processor Pipeline Library
 * 
 * A modular system for processing sensor data through configurable pipelines.
 * Supports chaining multiple processors for filtering, smoothing, mapping, 
 * thresholding, and time-based operations.
 * 
 * Usage:
 *   #include <sensorProcessors.h>
 * 
 * Quick Start:
 *   auto pipeline = PipelineBuilder("temp")
 *       .add<MovingAverageProcessor>(5)
 *       .add<RangeMappingProcessor>(-40, 100, 0, 255)
 *       .build();
 *   
 *   auto result = pipeline.process(sensorValue);
 *   uint8_t dmxValue = (uint8_t)result.value;
 * 
 * See CONFIG_EXAMPLES.md and USAGE.md for detailed documentation.
 */

// Core components
#include "SensorProcessor.h"
#include "ProcessorPipeline.h"

// Processor categories
#include "RangeMappingProcessors.h"
#include "TimeBasedProcessors.h"
#include "ThresholdProcessors.h"
#include "SmoothingProcessors.h"

// Factory for creating from configuration
#include "SensorProcessorFactory.h"

// Configuration-based pipeline management
#include "ConfigurablePipelineManager.h"

#endif // SENSORPROCESSORS_H
