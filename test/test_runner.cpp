#include <Arduino.h>
#include <unity.h>

void run_main_logic_tests(void);
void run_AHT20_tests(void);
void run_PID_tests(void);

void setup() {
    delay(2000);
    UNITY_BEGIN();

    run_main_logic_tests();
    run_AHT20_tests();
    run_PID_tests();

    UNITY_END();
}

void loop() {
    // Do nothing here - tests run in setup()
}