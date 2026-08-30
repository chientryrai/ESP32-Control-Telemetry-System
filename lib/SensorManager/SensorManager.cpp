

#include "SensorManager.h"

SensorManager::SensorManager(TwoWire &wirePort)
    : _wire(wirePort), _aht25(wirePort),
      _sensorType(SENSOR_TYPE_NONE), _initialized(false) {}

SensorManager::~SensorManager() {}

bool SensorManager::begin() {
    _wire.begin();

    _wire.beginTransmission(AHT25_I2C_ADDR);
    if (_wire.endTransmission() == 0) {
        if (_aht25.begin()) {
            _sensorType = SENSOR_TYPE_AHT25;
            _initialized = true;
            return true;
        }
    }

    return false;
}

bool SensorManager::update() {
    if (!_initialized) {
        return false;
    }

    if (_sensorType == SENSOR_TYPE_AHT25) {
        if (_aht25.update()) {
            AHT25_Data_t ahtData = _aht25.getLastResult();
            convertAHT25Data(ahtData, _lastData);
            return true;
        }
    }

    return false;
}

SensorData_t SensorManager::getLastReading() {
    return _lastData;
}

void SensorManager::convertAHT25Data(const AHT25_Data_t &src, SensorData_t &dst) {
    dst.temperature_c = src.temperature;
    dst.humidity_pct = src.humidity;
    dst.timestamp_ms = src.timestamp;
    dst.valid = src.valid;
}