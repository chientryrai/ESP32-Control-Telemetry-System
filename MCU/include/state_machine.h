

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>


typedef enum {
    INIT_MODE = 0,      // Setup I2C/LEDC/GPIO/Queues + non-blocking network
    AUTO_MODE,          // Default: sensor-driven fan + telemetry
    MANUAL_MODE,        // User PWM override via MQTT/ISR (with timeout)
    EMERGENCY_MODE,     // Highest priority: critical thresholds exceeded
    SAFE_MODE           // Fault recovery: WDT / I2C timeout, fan @ safe duty
} SystemMode_t;


typedef struct {
    float temperature_c;    // Temperature in degrees Celsius
    float humidity_pct;     // Relative humidity in percent
    uint32_t timestamp_ms;  // millis() at read time
    bool valid;             // false if I2C read failed
} SensorData_t;


typedef struct {
    uint8_t duty_pct;       // 0..100 explicit PWM duty
    uint32_t timestamp_ms;  // millis() at receipt (for timeout tracking)
} ManualCommand_t;


typedef struct {
    volatile SystemMode_t current_mode;  // Current active mode
    volatile uint8_t     fan_duty_pct;   // Current fan duty (0..100)
    volatile bool        network_ready;  // MQTT connected flag
} SystemContext_t;

#endif // STATE_MACHINE_H