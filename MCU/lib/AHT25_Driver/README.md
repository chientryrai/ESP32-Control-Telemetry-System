# AHT25 Driver for ESP32

Non-blocking I2C driver for AHT25 temperature/humidity sensor.

## AHT25 vs AHT20
AHT25 is the upgraded version of AHT20 with:
- **Better accuracy**: ±0.2°C temp, ±2% RH (vs ±0.3°C, ±3% RH for AHT20)
- **Wider measurement range**: -40 to 85°C
- **Faster response time**: ~75ms typical
- **Improved long-term stability**
- **Same I2C address**: 0x38
- **Same command protocol**: Compatible with AHT20

## Features
- Non-blocking I2C communication
- Automatic measurement triggering
- Data ready polling
- Calibration verification
- Temperature compensation
- Humidity calculation

## Usage
```cpp
#include "AHT25.h"

AHT25 sensor(Wire);
sensor.begin();

// In your loop or task:
if (sensor.update()) {
    AHT25_Data_t data = sensor.getLastResult();
    float temp = data.temperature;
    float hum = data.humidity;
}
```

## Implementation Notes
Based on AHT25 datasheet:
- I2C address: 0x38
- Measurement time: ~75ms (faster than AHT20)
- Resolution: 0.01°C for temp, 0.024% RH for humidity
- Supply voltage: 1.8V - 3.6V
- Operating temp: -40°C to +85°C