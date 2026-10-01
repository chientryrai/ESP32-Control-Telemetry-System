

#ifndef AHT20_H
#define AHT20_H

#include <Arduino.h>
#include <Wire.h>

#define AHT20_I2C_ADDR    0x38
#define AHT20_CMD_INIT    0xBE
#define AHT20_CMD_TRIGGER 0xAC
#define AHT20_CMD_SOFTRESET 0xBA
#define AHT20_CMD_STATUS  0x71

class AHT20I2CTransport {
public:
    virtual ~AHT20I2CTransport() = default;
    virtual void beginTransmission(uint8_t address) = 0;
    virtual size_t write(uint8_t data) = 0;
    virtual uint8_t endTransmission() = 0;
    virtual uint8_t requestFrom(uint8_t address, uint8_t quantity) = 0;
    virtual int available() = 0;
    virtual int read() = 0;
};

typedef struct {
    float temperature;    // Celsius
    float humidity;       // Percent RH
    bool valid;           // True if last read successful
    uint32_t timestamp;   // millis() of successful read
} AHT20_Data_t;

class AHT20 {
public:
    AHT20(TwoWire &wirePort = Wire, uint8_t addr = AHT20_I2C_ADDR);
    AHT20(AHT20I2CTransport &wirePort, uint8_t addr = AHT20_I2C_ADDR);
    ~AHT20();

    bool begin();
    void triggerMeasurement();  // Non-blocking trigger
    bool isDataReady();         // Check if conversion complete
    bool readData();            // Read results (call after isDataReady)
    AHT20_Data_t getLastResult(); // Get cached data

    bool update();              // Call periodically, returns true on new data

    bool checkCalibration();    // Verify calibration bits
    uint8_t getStatus();        // Read status register

private:
    class TwoWireAdapter : public AHT20I2CTransport {
    public:
        explicit TwoWireAdapter(TwoWire &wirePort) : _wire(wirePort) {}
        void beginTransmission(uint8_t address) override { _wire.beginTransmission(address); }
        size_t write(uint8_t data) override { return _wire.write(data); }
        uint8_t endTransmission() override { return _wire.endTransmission(); }
        uint8_t requestFrom(uint8_t address, uint8_t quantity) override {
            return _wire.requestFrom(address, quantity);
        }
        int available() override { return _wire.available(); }
        int read() override { return _wire.read(); }

    private:
        TwoWire &_wire;
    };

    TwoWireAdapter _wireAdapter;
    AHT20I2CTransport *_wire;
    uint8_t _addr;
    AHT20_Data_t _lastData;
    uint32_t _lastTriggerTime;
    bool _measurementPending;

    bool writeCommand(uint8_t cmd);
    bool writeCommandWithParams(uint8_t cmd, uint8_t param1, uint8_t param2);
    bool readBytes(uint8_t *buffer, size_t length);
};


#endif 
