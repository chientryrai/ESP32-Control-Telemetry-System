

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "../../include/state_machine.h"
#include "../AHT25_Driver/AHT25.h"

typedef enum {
    SENSOR_TYPE_NONE = 0,
    SENSOR_TYPE_AHT25
} SensorType_t;

class SensorManager {
public:
    SensorManager(TwoWire &wirePort = Wire);
    ~SensorManager();

    bool begin();
    bool update();
    SensorData_t getLastReading();

    SensorType_t getSensorType() const { return _sensorType; }
    bool isInitialized() const { return _initialized; }
    bool isDataValid() const { return _lastData.valid; }

private:
    TwoWire &_wire;
    AHT25 _aht25;
    SensorType_t _sensorType;
    bool _initialized;
    SensorData_t _lastData;

    void convertAHT25Data(const AHT25_Data_t &src, SensorData_t &dst);
};

#endif // SENSOR_MANAGER_H