

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>


#define PIN_I2C_SDA         21      // I2C data (AHT20/BME280 + LCD)
#define PIN_I2C_SCL         22      // I2C clock
#define PIN_FAN_PWM         25      // LEDC PWM output -> 4-wire fan PWM line
#define PIN_BUZZER          26      // Active buzzer (EMERGENCY alert)
#define PIN_LED_GREEN       27      // Normal operation indicator
#define PIN_LED_RED         14      // Emergency / fault indicator
#define PIN_FAN_TACH        34      // Fan tachometer input (optional, input-only)


#define LEDC_FAN_CHANNEL    0
#define LEDC_FAN_TIMER      0
#define LEDC_FAN_RESOLUTION 13   // 13-bit resolution => 0..8191
#define LEDC_FAN_FREQ_HZ    25000 // 25 kHz (4-wire fan spec)


#define SENSOR_I2C_ADDR_AHT20   0x38
#define SENSOR_I2C_ADDR_BME280  0x76
#define SENSOR_I2C_FREQ_HZ      100000UL   // 100 kHz standard mode


#define TEMP_MIN_C          20.0f   // Below -> fan off (0%)
#define TEMP_MAX_C          40.0f   // Above -> fan full (100%)
#define TEMP_CRITICAL_C     45.0f   // EMERGENCY_MODE trigger
#define HUM_CRITICAL_HIGH   85.0f   // EMERGENCY_MODE trigger
#define HUM_CRITICAL_LOW    15.0f   // EMERGENCY_MODE trigger

#define FAN_SAFE_DUTY_PCT   50      // SAFE_MODE default duty
#define FAN_MIN_DUTY_PCT    0
#define FAN_MAX_DUTY_PCT    100


#define SENSOR_READ_PERIOD_MS   1000UL   // Task_SensorRead period
#define FAN_UPDATE_PERIOD_MS    500UL    // Task_FanControl period
#define NETWORK_PERIOD_MS       100UL    // Task_Network tick
#define MQTT_KEEPALIVE_S        30
#define MQTT_PUBLISH_PERIOD_MS  5000UL   // Telemetry publish interval
#define MANUAL_TIMEOUT_MS       30000UL  // Revert MANUAL -> AUTO if no cmd
#define I2C_TIMEOUT_MS          2000UL   // Sensor read timeout -> SAFE_MODE
#define EMERGENCY_REACT_MS      1000UL   // Max reaction budget (<= 1s)
#define WDT_TIMEOUT_S           10       // Hardware WDT feed window


#define WIFI_SSID           "YOUR_SSID"
#define WIFI_PASSWORD       "YOUR_PASSWORD"
#define MQTT_BROKER         "192.168.1.100"
#define MQTT_PORT           1883
#define MQTT_CLIENT_ID      "esp32-fan-controller"
#define MQTT_TOPIC_TELEM    "env/telemetry"
#define MQTT_TOPIC_CMD      "env/command"
#define MQTT_TOPIC_ALERT    "env/alert"


#define TASK_STACK_SENSOR       4096    // Task_SensorRead
#define TASK_STACK_FAN          3072    // Task_FanControl
#define TASK_STACK_NETWORK      6144    // Task_Network (JSON/MQTT heavy)
#define TASK_STACK_WATCHDOG     2048    // Task_Emergency_Watchdog

#define TASK_PRIO_WATCHDOG      (configMAX_PRIORITIES - 1)  // Highest
#define TASK_PRIO_FAN           (configMAX_PRIORITIES - 2)  // High
#define TASK_PRIO_SENSOR        (configMAX_PRIORITIES - 3)  // Medium
#define TASK_PRIO_NETWORK       (configMAX_PRIORITIES - 4)  // Low

#define SENSOR_QUEUE_LEN        8       // SensorData queue depth
#define CMD_QUEUE_LEN           8       // Manual command queue depth


#define EVT_NETWORK_READY       (1UL << 0)
#define EVT_SENSOR_OK           (1UL << 1)
#define EVT_SENSOR_TIMEOUT      (1UL << 2)
#define EVT_EMERGENCY           (1UL << 3)
#define EVT_MANUAL_ACTIVE       (1UL << 4)
#define EVT_MANUAL_TIMEOUT      (1UL << 5)
#define EVT_SAFE_MODE           (1UL << 6)
#define EVT_WDT_FAULT           (1UL << 7)

#endif // CONFIG_H