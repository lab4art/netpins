#pragma once

/**
 * DMX Processors Library
 * 
 * A system for processing and transforming DMX channel data directly.
 * Unlike sensor processors which transform sensor readings, DMX processors
 * operate on DMX universe data to apply effects, transitions, and patterns.
 * 
 * Use cases:
 * - Cue lists and scene management
 * - Smooth fades and transitions
 * - Strobe effects (using fast scene transitions)
 * - Color mixing and effects
 * - Pattern generation
 * - Value limiting and scaling
 * 
 * Usage:
 *   #include <dmxProcessors.h>
 * 
 * Quick Start:
 *   auto cueList = new DmxCueListProcessor();
 *   cueList->addScene(0, {{1, 255}, {2, 255}}, 1000, 5000);
 *   cueList->process(dmxData, millis());
 */

// Base class
#include "DmxProcessor.h"

// Effect processors
#include "DmxCueListProcessor.h"

// Factory for creating processors from config
#include "DmxProcessorFactory.h"

// Manager for running processors controlled by DMX channels
#include "DmxProcessorManager.h"
