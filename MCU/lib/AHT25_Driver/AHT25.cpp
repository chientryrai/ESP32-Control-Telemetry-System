

#include "AHT25.h"

AHT25::AHT25(TwoWire &wirePort, uint8_t addr)
    : _wire(wirePort), _addr(addr), _lastData{0, 0, false, 0},
      _lastTriggerTime(0), _measurementPending(false) {}

AHT25::~AHT25() {}

bool AHT25::begin() {
    _wire.begin();

    if (!writeCommand(AHT25_CMD_SOFTRESET)) {
        return false;
    }
    delay(20);  // Wait for reset

    if (!writeCommandWithParams(AHT25_CMD_INIT, 0x08, 0x00)) {
        return false;
    }
    delay(10);  // Wait for init

    return checkCalibration();
}

void AHT25::triggerMeasurement() {
    if (_measurementPending) return;

    _wire.beginTransmission(_addr);
    _wire.write(AHT25_CMD_TRIGGER);
    _wire.write(0x33);
    _wire.write(0x00);
    if (_wire.endTransmission() != 0) {
        return;
    }

    _measurementPending = true;
    _lastTriggerTime = millis();
}

bool AHT25::isDataReady() {
    if (!_measurementPending) return false;

    if (millis() - _lastTriggerTime < 75) {
        return false;
    }

    uint8_t status = getStatus();
    if (status == 0xFF) {
        return false;  // Read failed
    }

    return (status & 0x80) == 0;  // Busy bit = 0 means ready
}

bool AHT25::readData() {
    if (!isDataReady()) {
        return false;
    }

    uint8_t data[6];
    _wire.beginTransmission(_addr);
    _wire.write(AHT25_CMD_STATUS);
    if (_wire.endTransmission() != 0) {
        _measurementPending = false;
        return false;
    }

    _wire.requestFrom(_addr, (uint8_t)6);
    if (_wire.available() != 6) {
        _measurementPending = false;
        return false;
    }

    for (size_t i = 0; i < 6; i++) {
        data[i] = _wire.read();
    }

    _measurementPending = false;

    uint32_t rawHumidity = ((uint32_t)data[1] << 12) |
                          ((uint32_t)data[2] << 4) |
                          (data[3] >> 4);
    uint32_t rawTemperature = (((uint32_t)data[3] & 0x0F) << 16) |
                             ((uint32_t)data[4] << 8) |
                             data[5];

    _lastData.humidity = (rawHumidity * 100.0f) / 0xFFFFF;
    _lastData.temperature = ((rawTemperature * 200.0f) / 0xFFFFF) - 50.0f;
    _lastData.valid = true;
    _lastData.timestamp = millis();

    return true;
}

AHT25_Data_t AHT25::getLastResult() {
    return _lastData;
}

bool AHT25::update() {
    if (!_measurementPending &&
        (millis() - _lastData.timestamp >= 1000 || !_lastData.valid)) {
        triggerMeasurement();
        return false;  // Measurement triggered, not ready yet
    }

    if (_measurementPending && isDataReady()) {
        return readData();  // Returns true if new data available
    }

    return false;  // No new data
}

bool AHT25::checkCalibration() {
    uint8_t status = getStatus();
    if (status == 0xFF) {
        return false;
    }

    return (status & 0x08) != 0;
}

uint8_t AHT25::getStatus() {
    _wire.beginTransmission(_addr);
    _wire.write(AHT25_CMD_STATUS);
    if (_wire.endTransmission() != 0) {
        return 0xFF;
    }

    _wire.requestFrom(_addr, (uint8_t)1);
    if (_wire.available() != 1) {
        return 0xFF;
    }

    return _wire.read();
}

bool AHT25::writeCommand(uint8_t cmd) {
    _wire.beginTransmission(_addr);
    _wire.write(cmd);
    return (_wire.endTransmission() == 0);
}

bool AHT25::writeCommandWithParams(uint8_t cmd, uint8_t param1, uint8_t param2) {
    _wire.beginTransmission(_addr);
    _wire.write(cmd);
    _wire.write(param1);
    _wire.write(param2);
    return (_wire.endTransmission() == 0);
}

bool AHT25::readBytes(uint8_t *buffer, size_t length) {
    _wire.requestFrom(_addr, (uint8_t)length);
    size_t available = _wire.available();
    if (available != length) {
        return false;
    }

    for (size_t i = 0; i < length; i++) {
        buffer[i] = _wire.read();
    }
    return true;
}