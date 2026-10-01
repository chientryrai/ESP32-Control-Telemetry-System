

#include "AHT20.h"

AHT20::AHT20(TwoWire &wirePort, uint8_t addr)
        : _wireAdapter(wirePort), _wire(&_wireAdapter), _addr(addr), _lastData{0.0f, 0.0f, false, 0},
            _lastTriggerTime(0), _measurementPending(false) {}

AHT20::AHT20(AHT20I2CTransport &wirePort, uint8_t addr)
        : _wireAdapter(Wire), _wire(&wirePort), _addr(addr), _lastData{0.0f, 0.0f, false, 0},
      _lastTriggerTime(0), _measurementPending(false) {}

AHT20::~AHT20() {}

bool AHT20::begin() {
    if (!writeCommand(AHT20_CMD_SOFTRESET)) {
        return false;
    }
    delay(20);

    if (!writeCommandWithParams(AHT20_CMD_INIT, 0x08, 0x00)) {
        return false;
    }
    delay(10);

    return checkCalibration();
}

void AHT20::triggerMeasurement() {
    if (_measurementPending) return;

    _wire->beginTransmission(_addr);
    _wire->write(AHT20_CMD_TRIGGER);
    _wire->write(0x33);
    _wire->write(0x00);
    if (_wire->endTransmission() != 0) {
        return;
    }

    _measurementPending = true;
    _lastTriggerTime = millis();
}

bool AHT20::isDataReady() {
    return _measurementPending && (millis() - _lastTriggerTime >= 80UL);
}

bool AHT20::readData() {
    if (!isDataReady()) {
        return false;
    }

    uint8_t data[6];
    if (_wire->requestFrom(_addr, (uint8_t)6) != 6 || _wire->available() != 6) {
        _measurementPending = false;
        return false;
    }

    for (size_t i = 0; i < 6; ++i) {
        data[i] = _wire->read();
    }

    _measurementPending = false;
    if (data[0] & 0x80) {
        return false;
    }

    uint32_t rawHumidity = ((uint32_t)data[1] << 12) |
                          ((uint32_t)data[2] << 4) |
                          ((uint32_t)data[3] >> 4);
    uint32_t rawTemperature = (((uint32_t)data[3] & 0x0F) << 16) |
                             ((uint32_t)data[4] << 8) |
                             data[5];

    _lastData.humidity = (rawHumidity * 100.0f) / 1048576.0f;
    _lastData.temperature = ((rawTemperature * 200.0f) / 1048576.0f) - 50.0f;
    if (_lastData.temperature < -50.0f || _lastData.temperature > 100.0f) {
        return false;
    }

    _lastData.valid = true;
    _lastData.timestamp = millis();

    return true;
}

AHT20_Data_t AHT20::getLastResult() {
    return _lastData;
}

bool AHT20::update() {
    if (!_measurementPending &&
        (millis() - _lastData.timestamp >= 1000 || !_lastData.valid)) {
        triggerMeasurement();
        return false;
    }

    if (_measurementPending && isDataReady()) {
        return readData();
    }

    return false;
}

bool AHT20::checkCalibration() {
    uint8_t status = getStatus();
    if (status == 0xFF) {
        return false;
    }

    return (status & 0x08) != 0;
}

uint8_t AHT20::getStatus() {
    _wire->beginTransmission(_addr);
    _wire->write(AHT20_CMD_STATUS);
    if (_wire->endTransmission() != 0) {
        return 0xFF;
    }

    _wire->requestFrom(_addr, (uint8_t)1);
    if (_wire->available() != 1) {
        return 0xFF;
    }

    return _wire->read();
}

bool AHT20::writeCommand(uint8_t cmd) {
    _wire->beginTransmission(_addr);
    _wire->write(cmd);
    return (_wire->endTransmission() == 0);
}

bool AHT20::writeCommandWithParams(uint8_t cmd, uint8_t param1, uint8_t param2) {
    _wire->beginTransmission(_addr);
    _wire->write(cmd);
    _wire->write(param1);
    _wire->write(param2);
    return (_wire->endTransmission() == 0);
}

bool AHT20::readBytes(uint8_t *buffer, size_t length) {
    _wire->requestFrom(_addr, (uint8_t)length);
    if (_wire->available() != (int)length) {
        return false;
    }

    for (size_t i = 0; i < length; ++i) {
        buffer[i] = _wire->read();
    }
    return true;
}