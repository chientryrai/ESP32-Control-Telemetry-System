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
    mqtt.publishTelemetry(data);
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
  "temperature": 25.5,
  "humidity": 60.2,
  "timestamp": 1234567,
  "valid": true,
  "device_id": "esp32-fan-controller"
}
```

**Commands (subscribed):**
```json
{
  "duty": 75
}
```