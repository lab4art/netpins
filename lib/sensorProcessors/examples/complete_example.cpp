/**
 * Complete example showing sensor pipeline usage
 * 
 * This example demonstrates:
 * 1. Creating pipelines programmatically
 * 2. Processing sensor data through pipelines
 * 3. Integration with DMX output
 * 4. Loading pipelines from configuration
 */

#include <Arduino.h>
#include <sensorProcessors.h>
#include <SensorPipelineIntegration.h>

// Mock DMX data structure
std::map<uint16_t, std::array<uint8_t, 512>> dmxData;

// Pipeline manager
SensorPipelineManager* pipelineManager;

// Individual pipelines for demonstration
ProcessorPipeline* tempPipeline;
ProcessorPipeline* motionPipeline;
ProcessorPipeline* lightPipeline;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Sensor Processor Pipeline Example ===\n");
    
    // Initialize DMX data
    dmxData[1] = std::array<uint8_t, 512>();
    
    // Create pipeline manager
    pipelineManager = new SensorPipelineManager(dmxData);
    
    // Setup pipelines using the helper function
    setupSensorPipelines(*pipelineManager);
    
    // Also create some standalone pipelines for testing
    Serial.println("\n=== Creating Test Pipelines ===");
    
    // Temperature pipeline: noise filtering + smoothing + mapping
    tempPipeline = PipelineBuilder("test_temp")
        .add<MedianFilterProcessor>(5)
        .add<ExponentialMovingAverageProcessor>(0.3f)
        .add<RangeMappingProcessor>(-40.0f, 100.0f, 0.0f, 255.0f)
        .buildPtr();
    tempPipeline->printStructure();
    
    // Motion pipeline: debounce + persistence + timeout
    motionPipeline = PipelineBuilder("test_motion")
        .add<DebounceProcessor>(100)
        .add<ScaleProcessor>(255.0f)
        .add<PersistenceProcessor>(5000, 255.0f, 0.0f)
        .add<TimeoutProcessor>(30000, 0.0f, 0.0f)
        .buildPtr();
    motionPipeline->printStructure();
    
    // Light pipeline: smoothing + hysteresis
    lightPipeline = PipelineBuilder("test_light")
        .add<LowPassFilterProcessor>(0.15f)
        .add<HysteresisProcessor>(500.0f, 400.0f, 255.0f, 0.0f)
        .buildPtr();
    lightPipeline->printStructure();
    
    Serial.println("\n=== Setup Complete ===\n");
}

void testTemperaturePipeline() {
    Serial.println("\n--- Temperature Pipeline Test ---");
    
    // Simulate noisy temperature readings with some spikes
    float tempReadings[] = {
        25.0, 25.2, 25.1, 45.0,  // spike at 45
        25.3, 25.2, 25.4, 25.3,
        26.0, 26.2, 26.1, 26.3
    };
    
    Serial.println("Raw -> Processed (DMX)");
    for (float temp : tempReadings) {
        auto result = tempPipeline->process(temp, millis());
        Serial.printf("%.1f°C -> %.1f (DMX: %d)\n", 
                     temp, result.value, (uint8_t)result.value);
        delay(100);  // Simulate time passing
    }
}

void testMotionPipeline() {
    Serial.println("\n--- Motion Pipeline Test ---");
    Serial.println("Simulating motion detection over time...");
    
    unsigned long startTime = millis();
    
    // Simulate motion events
    struct MotionEvent {
        unsigned long time;
        float value;
        const char* description;
    };
    
    MotionEvent events[] = {
        {0,     1.0f, "Motion detected"},
        {50,    1.0f, "Still detecting"},
        {100,   1.0f, "Still detecting"},
        {150,   0.0f, "Motion stopped (brief)"},
        {200,   1.0f, "Motion again"},
        {5200,  1.0f, "5s of motion - should trigger"},
        {5500,  0.0f, "Motion stopped"},
        {35500, 0.0f, "30s later - should timeout"},
    };
    
    for (const auto& event : events) {
        auto result = motionPipeline->process(event.value, startTime + event.time);
        Serial.printf("[%5lu ms] Input: %.0f -> Output: %.0f | %s\n",
                     event.time, event.value, result.value, event.description);
    }
}

void testLightPipeline() {
    Serial.println("\n--- Light Pipeline Test ---");
    Serial.println("Testing hysteresis behavior...");
    
    // Simulate light sensor readings crossing threshold
    float lightReadings[] = {
        300, 350, 400, 420,     // Below lower threshold
        450, 480, 490,          // In hysteresis zone
        510, 550, 600,          // Above upper threshold
        550, 490, 480,          // Back in hysteresis zone
        420, 380, 350           // Below lower threshold again
    };
    
    Serial.println("Light Level -> DMX Output (500=upper, 400=lower)");
    for (float light : lightReadings) {
        auto result = lightPipeline->process(light, millis());
        Serial.printf("%.0f lux -> DMX %d %s\n",
                     light, (uint8_t)result.value,
                     result.value > 128 ? "[ON]" : "[OFF]");
        delay(50);
    }
}

void testManagerIntegration() {
    Serial.println("\n--- Pipeline Manager Integration Test ---");
    
    // Process various sensor readings through the manager
    pipelineManager->processSensorValue("dht_temperature", 28.5f);
    pipelineManager->processSensorValue("pir_motion", 1.0f);
    pipelineManager->processSensorValue("light_sensor", 450.0f);
    
    // Print DMX values
    Serial.println("\nCurrent DMX values (Universe 1):");
    for (int i = 0; i < 5; i++) {
        Serial.printf("  Channel %d: %d\n", i, dmxData[1][i]);
    }
    
    // Demonstrate enable/disable
    Serial.println("\nDisabling temperature sensor...");
    pipelineManager->setSensorEnabled("dht_temperature", false);
    
    pipelineManager->processSensorValue("dht_temperature", 35.0f);
    Serial.printf("Temperature channel (should be unchanged): %d\n", dmxData[1][0]);
    
    // Re-enable
    Serial.println("\nRe-enabling temperature sensor...");
    pipelineManager->setSensorEnabled("dht_temperature", true);
    pipelineManager->processSensorValue("dht_temperature", 35.0f);
    Serial.printf("Temperature channel (should be updated): %d\n", dmxData[1][0]);
}

void testProcessorTypes() {
    Serial.println("\n--- Testing Individual Processor Types ---");
    
    // Test range mapping
    {
        Serial.println("\n1. Range Mapping (-40 to 100°C -> 0 to 255):");
        RangeMappingProcessor mapper(-40, 100, 0, 255);
        float temps[] = {-40, -20, 0, 25, 50, 75, 100};
        for (float t : temps) {
            auto result = mapper.process(SensorData(t, millis()));
            Serial.printf("  %.0f°C -> %.0f\n", t, result.value);
        }
    }
    
    // Test threshold with hysteresis
    {
        Serial.println("\n2. Hysteresis (upper: 30, lower: 25):");
        HysteresisProcessor hyst(30, 25, 255, 0);
        float values[] = {20, 22, 26, 28, 31, 33, 29, 27, 24, 22};
        for (float v : values) {
            auto result = hyst.process(SensorData(v, millis()));
            Serial.printf("  %.0f -> %.0f\n", v, result.value);
        }
    }
    
    // Test moving average
    {
        Serial.println("\n3. Moving Average (window: 3):");
        MovingAverageProcessor avg(3);
        float values[] = {10, 20, 30, 40, 50, 60};
        for (float v : values) {
            auto result = avg.process(SensorData(v, millis()));
            Serial.printf("  %.0f -> %.1f\n", v, result.value);
        }
    }
    
    // Test dead zone
    {
        Serial.println("\n4. Dead Zone (center: 128, radius: 10):");
        DeadZoneProcessor deadzone(128, 10);
        float values[] = {100, 115, 125, 128, 132, 140, 150};
        for (float v : values) {
            auto result = deadzone.process(SensorData(v, millis()));
            Serial.printf("  %.0f -> %.0f\n", v, result.value);
        }
    }
}

void loop() {
    static bool testsRun = false;
    
    if (!testsRun) {
        delay(2000);
        
        // Run all tests
        testTemperaturePipeline();
        delay(1000);
        
        testMotionPipeline();
        delay(1000);
        
        testLightPipeline();
        delay(1000);
        
        testManagerIntegration();
        delay(1000);
        
        testProcessorTypes();
        delay(1000);
        
        Serial.println("\n=== All Tests Complete ===");
        Serial.println("\nSee CONFIG_EXAMPLES.md and USAGE.md for more information.");
        
        testsRun = true;
    }
    
    // In a real application, you would read sensors here and process them:
    /*
    float temperature = readTemperatureSensor();
    pipelineManager->processSensorValue("dht_temperature", temperature);
    
    bool motionDetected = readMotionSensor();
    pipelineManager->processSensorValue("pir_motion", motionDetected ? 1.0f : 0.0f);
    
    // Update DMX output with processed values
    updateDmxOutput(dmxData);
    */
    
    delay(1000);
}
