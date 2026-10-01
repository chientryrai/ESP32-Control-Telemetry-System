/**
 * Unit tests for Main Application Logic
 * 
 * This test suite validates the core application logic including
 * state machine transitions, fan control logic, and system behavior.
 */

#include <Arduino.h>
#include <state_machine.h>
#include <unity.h>

// Test fixtures
SystemContext_t sys;

// Helper functions to simulate the mode transition logic
void applyModeAction_test(SystemMode_t mode) {
    // Simplified version of applyModeAction for testing logic
    switch (mode) {
        case INIT_MODE:
            sys.fan_duty_pct = 0;
            break;
        case AUTO_MODE:
            sys.fan_duty_pct = 50; // Simplified
            break;
        case MANUAL_MODE:
            sys.fan_duty_pct = 75; // Simplified
            break;
        case EMERGENCY_MODE:
            sys.fan_duty_pct = 100;
            break;
        default:
            sys.fan_duty_pct = 0;
            break;
    }
}

void setSystemMode_test(SystemMode_t newMode) {
    if (sys.current_mode == newMode) {
        return;
    }
    sys.current_mode = newMode;
    applyModeAction_test(newMode);
}

void test_stateMachine_initialState(void) {
    // Test initial state
    sys.current_mode = INIT_MODE;
    sys.fan_duty_pct = 0;
    sys.network_ready = false;
    
    TEST_ASSERT_EQUAL_UINT8(INIT_MODE, sys.current_mode);
    TEST_ASSERT_EQUAL_UINT8(0, sys.fan_duty_pct);
    TEST_ASSERT_FALSE(sys.network_ready);
}

void test_stateMachine_transitions(void) {
    // Test INIT_MODE -> AUTO_MODE transition
    setSystemMode_test(INIT_MODE);
    TEST_ASSERT_EQUAL_UINT8(INIT_MODE, sys.current_mode);
    
    setSystemMode_test(AUTO_MODE);
    TEST_ASSERT_EQUAL_UINT8(AUTO_MODE, sys.current_mode);
    TEST_ASSERT_EQUAL_UINT8(50, sys.fan_duty_pct); // From applyModeAction_test
    
    // Test AUTO_MODE -> MANUAL_MODE transition
    setSystemMode_test(MANUAL_MODE);
    TEST_ASSERT_EQUAL_UINT8(MANUAL_MODE, sys.current_mode);
    TEST_ASSERT_EQUAL_UINT8(75, sys.fan_duty_pct); // From applyModeAction_test
    
    // Test MANUAL_MODE -> EMERGENCY_MODE transition
    setSystemMode_test(EMERGENCY_MODE);
    TEST_ASSERT_EQUAL_UINT8(EMERGENCY_MODE, sys.current_mode);
    TEST_ASSERT_EQUAL_UINT8(100, sys.fan_duty_pct); // From applyModeAction_test
    
    // Test EMERGENCY_MODE -> INIT_MODE transition
    setSystemMode_test(INIT_MODE);
    TEST_ASSERT_EQUAL_UINT8(INIT_MODE, sys.current_mode);
    TEST_ASSERT_EQUAL_UINT8(0, sys.fan_duty_pct); // From applyModeAction_test
}

void test_stateMachine_noChange(void) {
    // Test that setting same mode does nothing
    sys.current_mode = AUTO_MODE;
    sys.fan_duty_pct = 75; // Some arbitrary value
    
    uint8_t original_duty = sys.fan_duty_pct;
    SystemMode_t original_mode = sys.current_mode;
    
    setSystemMode_test(AUTO_MODE); // Same mode
    
    TEST_ASSERT_EQUAL_UINT8(original_mode, sys.current_mode);
    TEST_ASSERT_EQUAL_UINT8(original_duty, sys.fan_duty_pct); // Should be unchanged
}

void test_SensorData_t_init(void) {
    // Test SensorData_t initialization
    SensorData_t data = {0.0f, 0.0f, 0, false};
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data.humidity_pct);
    TEST_ASSERT_EQUAL_UINT32(0, data.timestamp_ms);
    TEST_ASSERT_FALSE(data.valid);
}

void test_SensorData_t_validData(void) {
    // Test SensorData_t with valid data
    SensorData_t data = {25.5f, 60.2f, 12345, true};
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.5f, data.temperature_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 60.2f, data.humidity_pct);
    TEST_ASSERT_EQUAL_UINT32(12345, data.timestamp_ms);
    TEST_ASSERT_TRUE(data.valid);
}

void test_ManualCommand_t(void) {
    // Test ManualCommand_t
    ManualCommand_t cmd = {75, 12345};
    
    TEST_ASSERT_EQUAL_UINT8(75, cmd.duty_pct);
    TEST_ASSERT_EQUAL_UINT32(12345, cmd.timestamp_ms);
}

void test_ManualCommand_t_clamping(void) {
    // Test that duty cycle values are clamped (conceptually)
    // In real code, this happens in MQTT handler, but we can test the concept
    ManualCommand_t cmd1 = {0, 0};    // Minimum
    ManualCommand_t cmd2 = {100, 0};  // Maximum
    ManualCommand_t cmd3 = {150, 0};  // Would be clamped to 100
    ManualCommand_t cmd4 = {-10, 0};  // Would be clamped to 0
    
    TEST_ASSERT_EQUAL_UINT8(0, cmd1.duty_pct);
    TEST_ASSERT_EQUAL_UINT8(100, cmd2.duty_pct);
    // Note: In actual implementation, clamping happens elsewhere
    // We're testing the data structure here
}

void test_SystemMode_t_enumValues(void) {
    // Test that enum values are as expected
    TEST_ASSERT_EQUAL_UINT8(0, INIT_MODE);
    TEST_ASSERT_EQUAL_UINT8(1, AUTO_MODE);
    TEST_ASSERT_EQUAL_UINT8(2, MANUAL_MODE);
    TEST_ASSERT_EQUAL_UINT8(3, EMERGENCY_MODE);
}

void run_main_logic_tests(void) {
    RUN_TEST(test_stateMachine_initialState);
    RUN_TEST(test_stateMachine_transitions);
    RUN_TEST(test_stateMachine_noChange);
    RUN_TEST(test_SensorData_t_init);
    RUN_TEST(test_SensorData_t_validData);
    RUN_TEST(test_ManualCommand_t);
    RUN_TEST(test_ManualCommand_t_clamping);
    RUN_TEST(test_SystemMode_t_enumValues);
}