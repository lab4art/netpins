#include <unity.h>
#include <scheduler.h>
#include <DmxManager.h>
#include <DmxSequenceProcessor.h>
#include <map>
#include <string>

// Mock millis() and delay() for native testing
static unsigned long mockMillis = 0;
unsigned long millis() { return mockMillis; }
void delay(unsigned long ms) { mockMillis += ms; }

// Helper: Minimal DMX Manager mock
class MockDmxManager : public DmxManager {
public:
    std::map<uint16_t, std::vector<uint8_t>> dmxData;
    std::map<uint16_t, std::vector<uint8_t>>& getDmxData() override { return dmxData; }
};

void setUp(void) { mockMillis = 0; }
void tearDown(void) {}

void test_include_end_parent_continue() {
    Scheduler scheduler;
    MockDmxManager dmxManager;
    DmxSequenceProcessor parent(&scheduler, &dmxManager);
    DmxSequenceProcessor included(&scheduler, &dmxManager);

    // Register included sequence
    included.addCue({{{1, 1}, 10}}, 100, 200); // Fade, then hold
    included.addCue({{{1, 1}, 20}}, 100, 200); // Fade, then hold
    included.addSequenceToRegistry("included");

    // Parent: fade, hold, include, fade, hold
    parent.addCue({{{1, 1}, 1}}, 100, 200); // Fade, hold
    parent.addIncludeCue("included", 0, 400); // Include for 400ms
    parent.addCue({{{1, 1}, 99}}, 100, 200); // Fade, hold

    parent.setLoop(false);
    parent.setEnabled(true);

    // Simulate time: parent fade+hold
    mockMillis = 0; scheduler.loop(); // Start
    mockMillis = 100; scheduler.loop(); // Fade done
    mockMillis = 300; scheduler.loop(); // Hold done, include starts

    // Included sequence runs
    mockMillis = 400; scheduler.loop(); // Included fade 1
    mockMillis = 500; scheduler.loop(); // Included hold 1
    mockMillis = 600; scheduler.loop(); // Included fade 2
    mockMillis = 700; scheduler.loop(); // Included hold 2
    mockMillis = 800; scheduler.loop(); // Include duration ends, parent resumes

    // Parent resumes
    mockMillis = 900; scheduler.loop(); // Parent fade
    mockMillis = 1000; scheduler.loop(); // Parent hold
    mockMillis = 1200; scheduler.loop(); // End

    // Check final DMX value
    TEST_ASSERT_EQUAL(99, dmxManager.dmxData[1][0]);
}

void test_parent_cancels_include() {
    Scheduler scheduler;
    MockDmxManager dmxManager;
    DmxSequenceProcessor parent(&scheduler, &dmxManager);
    DmxSequenceProcessor included(&scheduler, &dmxManager);

    included.addCue({{{1, 1}, 10}}, 100, 200);
    included.addCue({{{1, 1}, 20}}, 100, 200);
    included.addSequenceToRegistry("included");

    parent.addCue({{{1, 1}, 1}}, 100, 200);
    parent.addIncludeCue("included", 0, 400);
    parent.addCue({{{1, 1}, 99}}, 100, 200);
    parent.setLoop(false);
    parent.setEnabled(true);

    // Simulate time: parent fade+hold
    mockMillis = 0; scheduler.loop();
    mockMillis = 100; scheduler.loop();
    mockMillis = 300; scheduler.loop();

    // Parent disables before include ends
    parent.setEnabled(false);
    scheduler.loop();

    // Included should be stopped, parent should not resume
    mockMillis = 800; scheduler.loop();
    TEST_ASSERT_FALSE(parent.isEnabled());
    TEST_ASSERT_FALSE(included.isEnabled());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_include_end_parent_continue);
    RUN_TEST(test_parent_cancels_include);
    return UNITY_END();
}
