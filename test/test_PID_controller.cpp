/**
 * Unit tests for PID Controller
 * 
 * This test suite validates the PID controller functionality including
 * proportional, integral, and derivative terms for cooling control.
 */

#include <Arduino.h>
#include <PIDController.h>
#include <unity.h>

// Test fixtures
PIDController* pid = nullptr;

void pid_setUp(void) {
    // Set up test fixtures before each test
    // Kp=2.0, Ki=0.5, Kd=1.0, output limits 0-100%
    pid = new PIDController(2.0f, 0.5f, 1.0f, 0.0f, 100.0f);
}

void pid_tearDown(void) {
    // Clean up after each test
    delete pid;
    pid = nullptr;
}

void test_PID_constructor(void) {
    // Test that constructor initializes properly
    TEST_ASSERT_NOT_NULL(pid);
}

void test_PID_compute_firstRun(void) {
    // First run should return 0 and initialize internal state
    float output = pid->compute(25.0f, 25.0f, 0.1f); // setpoint=processVar=25, dt=0.1s
    TEST_ASSERT_EQUAL_FLOAT(0.0f, output);
}

void test_PID_compute_proportionalOnly(void) {
    // Test proportional term: with Ki=Kd=0, output should be Kp * error
    PIDController* p_pid = new PIDController(2.0f, 0.0f, 0.0f, 0.0f, 100.0f);

    p_pid->compute(25.0f, 25.0f, 0.1f);

    // Error = processVariable - setpoint = 30 - 25 = 5
    // Output = Kp * error = 2.0 * 5.0 = 10.0
    float output = p_pid->compute(25.0f, 30.0f, 0.1f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, output);

    delete p_pid;
}

void test_PID_compute_integralTerm(void) {
    // Test integral accumulation over time
    PIDController* i_pid = new PIDController(0.0f, 0.5f, 0.0f, 0.0f, 100.0f);

    i_pid->compute(25.0f, 25.0f, 0.1f);
    i_pid->compute(25.0f, 30.0f, 0.1f);
    float output2 = i_pid->compute(25.0f, 30.0f, 0.1f);

    // After the warm-up + two active samples, integral reaches 0.5
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, output2);

    delete i_pid;
}

void test_PID_compute_derivativeTerm(void) {
    // Test derivative term
    PIDController* d_pid = new PIDController(0.0f, 0.0f, 1.0f, 0.0f, 100.0f);

    d_pid->compute(25.0f, 25.0f, 0.1f);
    float output2 = d_pid->compute(28.0f, 30.0f, 0.1f);

    // With the actual implementation: derivative = (30 - 25) / 0.1 = 50
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, output2);

    delete d_pid;
}

void test_PID_compute_coolingLogic(void) {
    // Output is constrained to 0..100, so cold conditions clamp to zero.
    PIDController* hot_pid = new PIDController(2.0f, 0.5f, 1.0f, 0.0f, 100.0f);
    hot_pid->compute(25.0f, 25.0f, 0.1f);
    float output_hot = hot_pid->compute(25.0f, 30.0f, 0.1f);
    TEST_ASSERT_TRUE(output_hot > 0.0f);

    PIDController* cold_pid = new PIDController(2.0f, 0.5f, 1.0f, 0.0f, 100.0f);
    cold_pid->compute(25.0f, 25.0f, 0.1f);
    float output_cold = cold_pid->compute(25.0f, 20.0f, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, output_cold);

    PIDController* target_pid = new PIDController(2.0f, 0.5f, 1.0f, 0.0f, 100.0f);
    target_pid->compute(25.0f, 25.0f, 0.1f);
    float output_target = target_pid->compute(25.0f, 25.0f, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, output_target);

    delete hot_pid;
    delete cold_pid;
    delete target_pid;
}

void test_PID_outputLimits(void) {
    // Test that output is constrained to limits
    PIDController* limit_pid = new PIDController(10.0f, 0.0f, 0.0f, 0.0f, 50.0f); // Low max output

    limit_pid->compute(0.0f, 0.0f, 0.1f);

    // Large error should hit output limit
    float output = limit_pid->compute(0.0f, 100.0f, 0.1f); // error = 100
    // Expected: 10.0 * 100 = 1000, but clamped to 50
    TEST_ASSERT_EQUAL_FLOAT(50.0f, output);

    delete limit_pid;
}

void test_PID_reset(void) {
    // Test that reset clears integral and reset flags
    pid->compute(25.0f, 25.0f, 0.1f);
    pid->compute(25.0f, 30.0f, 0.1f);
    float output_before = pid->compute(25.0f, 30.0f, 0.1f);

    // Verify integral has accumulated (by checking output is not zero)
    TEST_ASSERT_TRUE(output_before != 0.0f);

    // Reset and check that integral is cleared
    pid->reset();
    pid->compute(25.0f, 25.0f, 0.1f);
    float output_after = pid->compute(25.0f, 25.0f, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, output_after);
}

void test_PID_setGains(void) {
    // Test changing PID gains
    pid->setGains(1.0f, 0.0f, 0.0f);
    pid->compute(25.0f, 25.0f, 0.1f);

    // Test with new gains: Kp=1.0, error=10 -> pTerm should be 10
    float output = pid->compute(25.0f, 35.0f, 0.1f); // error = 10
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 10.0f, output); // Primarily P term
}

void test_PID_setOutputLimits(void) {
    // Test changing output limits
    pid->setOutputLimits(0.0f, 50.0f);
    pid->compute(0.0f, 0.0f, 0.1f);

    // Try to get high output - should be limited to 50
    float output = pid->compute(0.0f, 100.0f, 0.1f); // Large negative error
    TEST_ASSERT_TRUE(output <= 50.0f);
    TEST_ASSERT_TRUE(output >= 0.0f);
}

void run_PID_tests(void) {
    pid_setUp();
    RUN_TEST(test_PID_constructor);
    pid_tearDown();

    pid_setUp();
    RUN_TEST(test_PID_compute_firstRun);
    pid_tearDown();

    pid_setUp();
    RUN_TEST(test_PID_compute_proportionalOnly);
    pid_tearDown();

    pid_setUp();
    RUN_TEST(test_PID_compute_integralTerm);
    pid_tearDown();

    pid_setUp();
    RUN_TEST(test_PID_compute_derivativeTerm);
    pid_tearDown();

    pid_setUp();
    RUN_TEST(test_PID_compute_coolingLogic);
    pid_tearDown();

    pid_setUp();
    RUN_TEST(test_PID_outputLimits);
    pid_tearDown();

    pid_setUp();
    RUN_TEST(test_PID_reset);
    pid_tearDown();

    pid_setUp();
    RUN_TEST(test_PID_setGains);
    pid_tearDown();

    pid_setUp();
    RUN_TEST(test_PID_setOutputLimits);
    pid_tearDown();
}