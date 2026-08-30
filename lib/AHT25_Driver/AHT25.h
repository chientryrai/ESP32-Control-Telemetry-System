

#ifndef AHT25_H
#define AHT25_H

#include <Arduino.h>
#include <Wire.h>

#define AHT25_I2C_ADDR    0x38
#define AHT25_CMD_INIT    0xE1
#define AHT25_CMD_TRIGGER 0xAC
#define AHT25_CMD_SOFTRESET 0xBA
#define AHT25_CMD_STATUS  0x71

typedef struct {
    float temperature;    // Celsius
    float humidity;       // Percent RH
    bool valid;           // True if last read successful
    uint32_t timestamp;   // millis() of successful read
} AHT25_Data_t;

class AHT25 {
public:
    AHT25(TwoWire &wirePort = Wire, uint8_t addr = AHT25_I2C_ADDR);
    ~AHT25();

    bool begin();
    void triggerMeasurement();  // Non-blocking trigger
    bool isDataReady();         // Check if conversion complete
    bool readData();            // Read results (call after isDataReady)
    AHT25_Data_t getLastResult(); // Get cached data

    bool update();              // Call periodically, returns true on new data

    bool checkCalibration();    // Verify calibration bits
    uint8_t getStatus();        // Read status register

private:
    TwoWire &_wire;
    uint8_t _addr;
    AHT25_Data_t _lastData;
    uint32_t _lastTriggerTime;
    bool _measurementPending;

    bool writeCommand(uint8_t cmd);
    bool writeCommandWithParams(uint8_t cmd, uint8_t param1, uint8_t param2);
    bool readBytes(uint8_t *buffer, size_t length);
};

#endif // AHT25_H