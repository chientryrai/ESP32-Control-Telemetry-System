# AHT20 / AHT25 Driver for ESP32

Non-blocking I2C driver (address 0x38) for AHT20 and AHT25 temperature/humidity
sensors (same protocol). This is the only sensor library; `main.cpp` uses it
directly and converts `AHT20_Data_t` to `SensorData_t`.

```cpp
AHT20 sensor(Wire);   // Wire.begin(SDA, SCL, freq) must already be called
sensor.begin();
if (sensor.update()) {
    AHT20_Data_t d = sensor.getLastResult();
}
```
