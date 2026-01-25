#pragma once

/**
 * DMX Processors Library
 * 
 * A system for processing and transforming DMX channel data directly.
 * Unlike sensor processors which transform sensor readings, DMX processors
 * operate on DMX universe data to apply effects, transitions, and patterns.
 * 
 * Use cases:
 * - Cue lists and cue management
 * - Smooth fades and transitions
 * - Strobe effects (using fast cue transitions)
 * - Color mixing and effects
 * - Pattern generation
 * - Value limiting and scaling
 * 
 * Usage:
 *   #include <dmxProcessors.h>
 * 
 * Quick Start:
 *   auto sequence = new DmxSequenceProcessor();
 *   sequence->addCue(0, {{1, 255}, {2, 255}}, 1000, 5000);
 *   sequence->process(dmxData, millis());
 */

// Base class
#include "DmxProcessor.h"

// Effect processors
#include "DmxSequenceProcessor.h"

// Factory for creating processors from config
#include "DmxProcessorFactory.h"

// Manager for running processors controlled by DMX channels
#include "DmxProcessorManager.h"
