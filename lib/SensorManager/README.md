# Sensor Manager for ESP32

Unified interface for automatic detection and handling of AHT20/BME280 sensors.

## Features
- Automatic sensor detection (AHT20 preferred, fallback to BME280)
- Unified SensorData_t interface
- Non-blocking updates
- Error handling and retry logic
- Transparent driver switching

## Usage
```cpp
#include "SensorManager.h"

SensorManager sensor(Wire);
sensor.begin();

// In your loop or task:
if (sensor.update()) {
    SensorData_t data = sensor.getLastReading();
    if (data.valid) {
        float temp = data.temperature_c;
        float hum = data.humidity_pct;
        uint32_t timestamp = data.timestamp_ms;
    }
}

// Check sensor type:
if (sensor.getSensorType() == SENSOR_TYPE_AHT20) {
    // AHT20 specific handling
} else if (sensor.getSensorType() == SENSOR_TYPE_BME280) {
    // BME280 specific handling
}
```

## Implementation Notes
- Tries AHT20 first (0x38), then BME280 (0x76/0x77)
- Returns standardized SensorData_t regardless of underlying sensor
- Handles initialization failures gracefully with retry mechanism