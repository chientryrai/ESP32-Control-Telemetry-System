

#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "../../include/config.h"
#include "../../include/state_machine.h"

class MQTTHandler {
public:
    MQTTHandler();
    ~MQTTHandler();

    bool begin();
    void update();  // Call periodically to handle MQTT tasks

    bool publishTelemetry(const SensorData_t &data);

    bool hasCommand() const;
    ManualCommand_t getCommand();  // Call only if hasCommand() returns true

    bool isConnected() const { return _wifiConnected && _mqttConnected; }
    bool isWiFiConnected() const { return _wifiConnected; }
    bool isMQTTConnected() const { return _mqttConnected; }

private:
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;

    bool _wifiConnected;
    bool _mqttConnected;

    uint32_t _lastReconnectAttempt;
    uint32_t _lastTelemetryPublish;
    uint32_t _lastMQTTLoop;

    ManualCommand_t _pendingCommand;
    bool _commandAvailable;

    const char *_ssid;
    const char *_password;
    const char *_mqttServer;
    uint16_t _mqttPort;
    const char *_mqttClientId;
    const char *_telemetryTopic;
    const char *_commandTopic;
    const char *_alertTopic;

    bool connectWiFi();
    bool connectMQTT();
    void mqttCallback(char* topic, byte* payload, unsigned int length);
    void handleTelemetry();
    void handleCommands();
    void publishAlert(const char *message);

    String createTelemetryJson(const SensorData_t &data);
    bool parseCommandJson(const String &jsonString, ManualCommand_t &cmd);
};

#endif // MQTT_HANDLER_H