#include <unity.h>

// Mock millis() and delay() for native testing
static unsigned long mockMillis = 0;

unsigned long millis() {
    return mockMillis;
}

void delay(unsigned long ms) {
    mockMillis += ms;
}

#include <scheduler.h>
#include <chrono>
#include <thread>

// Test task that counts executions
class CounterTask : public ScheduledTask {
private:
    int count = 0;

public:
    CounterTask(unsigned long interval, std::string name, bool enabled = true) 
        : ScheduledTask(interval, name, enabled) {}

    void callback() override {
        count++;
    }

    int getCount() const {
        return count;
    }

    void resetCount() {
        count = 0;
    }
};

void setUp(void) {
    mockMillis = 0;
    Log::setLogLevel(LOG_LEVEL_TRACE);
}

void tearDown(void) {
    // Clean up after each test
}

void test_scheduler_basic_execution(void) {
    Scheduler scheduler;
    CounterTask task(100, "test-task");
    scheduler.addTask(&task);

    // Initially no executions
    TEST_ASSERT_EQUAL(0, task.getCount());

    // Loop at t=0, should not execute
    scheduler.loop();
    TEST_ASSERT_EQUAL(0, task.getCount());

    // Advance to t=50, still not executed
    mockMillis = 50;
    scheduler.loop();
    TEST_ASSERT_EQUAL(0, task.getCount());

    // Advance to t=100, should execute once
    mockMillis = 100;
    scheduler.loop();
    TEST_ASSERT_EQUAL(1, task.getCount());

    // Loop again at same time, should not execute again
    scheduler.loop();
    TEST_ASSERT_EQUAL(1, task.getCount());

    // Advance to t=200, should execute again
    mockMillis = 200;
    scheduler.loop();
    TEST_ASSERT_EQUAL(2, task.getCount());
}

void test_scheduler_multiple_tasks(void) {
    Scheduler scheduler;
    CounterTask task1(100, "task-1");
    CounterTask task2(200, "task-2");
    CounterTask task3(50, "task-3");

    scheduler.addTask(&task1);
    scheduler.addTask(&task2);
    scheduler.addTask(&task3);

    // At t=50, only task3 should execute
    mockMillis = 50;
    scheduler.loop();
    TEST_ASSERT_EQUAL(0, task1.getCount());
    TEST_ASSERT_EQUAL(0, task2.getCount());
    TEST_ASSERT_EQUAL(1, task3.getCount());

    // At t=100, task1 and task3 should have executed
    mockMillis = 100;
    scheduler.loop();
    TEST_ASSERT_EQUAL(1, task1.getCount());
    TEST_ASSERT_EQUAL(0, task2.getCount());
    TEST_ASSERT_EQUAL(2, task3.getCount());

    // At t=200, all tasks should have executed
    mockMillis = 200;
    scheduler.loop();
    TEST_ASSERT_EQUAL(2, task1.getCount());
    TEST_ASSERT_EQUAL(1, task2.getCount());
    TEST_ASSERT_EQUAL(3, task3.getCount());
}

void test_scheduler_enable_disable(void) {
    Scheduler scheduler;
    CounterTask task(100, "test-task");
    scheduler.addTask(&task);

    // Execute once
    mockMillis = 100;
    scheduler.loop();
    TEST_ASSERT_EQUAL(1, task.getCount());

    // Disable task
    task.disable();
    mockMillis = 200;
    scheduler.loop();
    TEST_ASSERT_EQUAL(1, task.getCount()); // Should not execute

    // Re-enable task
    task.enable();
    mockMillis = 300;
    scheduler.loop();
    TEST_ASSERT_EQUAL(2, task.getCount()); // Should execute again
}

void test_scheduler_zero_interval(void) {
    Scheduler scheduler;
    CounterTask task(0, "zero-interval-task");
    scheduler.addTask(&task);

    // Task with interval=0 should never execute
    mockMillis = 1000;
    scheduler.loop();
    TEST_ASSERT_EQUAL(0, task.getCount());
}

void test_scheduler_task_created_disabled(void) {
    Scheduler scheduler;
    CounterTask task(100, "disabled-task", false);
    scheduler.addTask(&task);

    // Task created disabled should not execute
    mockMillis = 100;
    scheduler.loop();
    TEST_ASSERT_EQUAL(0, task.getCount());

    // Enable and it should execute
    task.enable();
    mockMillis = 200;
    scheduler.loop();
    TEST_ASSERT_EQUAL(1, task.getCount());
}

void test_scheduler_irregular_intervals(void) {
    Scheduler scheduler;
    CounterTask task(100, "test-task");
    scheduler.addTask(&task);

    mockMillis = 100;
    scheduler.loop();
    TEST_ASSERT_EQUAL(1, task.getCount());

    mockMillis = 350;
    scheduler.loop();
    TEST_ASSERT_EQUAL(2, task.getCount());

    // Current simplified scheduler does not handle late executions.
    // If execution is late all subsequent executions are delayed.
    mockMillis = 400;
    scheduler.loop();
    TEST_ASSERT_EQUAL(2, task.getCount());

    mockMillis = 450;
    scheduler.loop();
    TEST_ASSERT_EQUAL(3, task.getCount());
}

void test_scheduler_long_interval(void) {
    Scheduler scheduler;
    CounterTask task(10000, "long-task");
    scheduler.addTask(&task);

    // Should not execute before interval
    mockMillis = 9999;
    scheduler.loop();
    TEST_ASSERT_EQUAL(0, task.getCount());

    // Should execute at exact interval
    mockMillis = 10000;
    scheduler.loop();
    TEST_ASSERT_EQUAL(1, task.getCount());

    // Should execute again after another interval
    mockMillis = 20000;
    scheduler.loop();
    TEST_ASSERT_EQUAL(2, task.getCount());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_scheduler_basic_execution);
    RUN_TEST(test_scheduler_multiple_tasks);
    RUN_TEST(test_scheduler_enable_disable);
    RUN_TEST(test_scheduler_zero_interval);
    RUN_TEST(test_scheduler_task_created_disabled);
    RUN_TEST(test_scheduler_irregular_intervals);
    RUN_TEST(test_scheduler_long_interval);
    
    return UNITY_END();
}
