# MQTT Handler for ESP32

Non-blocking MQTT client with automatic WiFi/MQTT reconnection.

## Features
- Automatic WiFi reconnection
- Automatic MQTT reconnection with exponential backoff
- Non-blocking operation (call update() periodically)
- Telemetry publishing (SensorData_t -> JSON)
- Command subscription (JSON -> ManualCommand_t)
- ArduinoJson integration
- Status monitoring

## Usage
```cpp
#include "MQTTHandler.h"

MQTTHandler mqtt;
mqtt.begin();

// In main loop or task:
mqtt.update();

// Publish telemetry:
if (mqtt.isConnected()) {
    SensorData_t data = getSensorData();
  mqtt.publishTelemetry(data, fanDutyPct, fanRpm, targetTemperature,
              mode, alarm, shutdown);
}

// Check for commands:
if (mqtt.hasCommand()) {
    ManualCommand_t cmd = mqtt.getCommand();
    // Process cmd.duty_pct (0-100)
}
```

## Topics
- Telemetry: `env/telemetry` (publish)
- Commands: `env/command` (subscribe)  
- Alerts: `env/alert` (publish)

## Message Formats
**Telemetry (published):**
```json
{
  "device_id": "esp32-fan-controller",
  "timestamp": 1234567,
  "temperature": 25.5,
  "humidity": 60.2,
  "fan_duty": 75,
  "fan_rpm": 1800,
  "mode": "MANUAL",
  "alarm": false,
  "shutdown": false,
  "target_temperature": 30.0,
  "sensor_valid": true
}
```

**Commands (subscribed):**
```json
{
  "duty": 75
}
```