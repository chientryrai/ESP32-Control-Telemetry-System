

#include "MQTTHandler.h"

MQTTHandler::MQTTHandler()
    : _wifiClient(), 
      _mqttClient(_wifiClient),
      _wifiConnected(false),
      _mqttConnected(false),
      _lastReconnectAttempt(0),
      _lastTelemetryPublish(0),
      _lastMQTTLoop(0),
      _commandAvailable(false) {

    _ssid = WIFI_SSID;
    _password = WIFI_PASSWORD;
    _mqttServer = MQTT_BROKER;
    _mqttPort = MQTT_PORT;
    _mqttClientId = MQTT_CLIENT_ID;
    _telemetryTopic = MQTT_TOPIC_TELEM;
    _commandTopic = MQTT_TOPIC_CMD;
    _alertTopic = MQTT_TOPIC_ALERT;
}

MQTTHandler::~MQTTHandler() {
    if (_mqttClient.connected()) {
        _mqttClient.disconnect();
    }
    WiFi.disconnect();
}

bool MQTTHandler::begin() {
    Serial.println("Starting MQTT Handler...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid, _password);
    Serial.print("Connecting to WiFi...");

    _mqttClient.setServer(_mqttServer, _mqttPort);
    _mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->mqttCallback(topic, payload, length);
    });
    
    return true;
}

void MQTTHandler::update() {

    if (!_wifiConnected && WiFi.status() == WL_CONNECTED) {
        _wifiConnected = true;
        Serial.println("\nWiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else if (_wifiConnected && WiFi.status() != WL_CONNECTED) {
        _wifiConnected = false;
        _mqttConnected = false;
        Serial.println("\nWiFi disconnected");
    }

    if (_wifiConnected && !_mqttConnected) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt > 5000) {  // Try every 5 seconds
            _lastReconnectAttempt = now;
            if (connectMQTT()) {
                Serial.println("MQTT reconnected");
            }
        }
    }

    if (_mqttConnected && (millis() - _lastMQTTLoop > 100)) {
        _mqttClient.loop();
        _lastMQTTLoop = millis();
    }

    if (_mqttConnected && (millis() - _lastTelemetryPublish > MQTT_PUBLISH_PERIOD_MS)) {


        _lastTelemetryPublish = millis();
    }
}

bool MQTTHandler::publishTelemetry(const SensorData_t &data) {
    if (!_mqttConnected) {
        return false;
    }
    
    String jsonPayload = createTelemetryJson(data);
    bool result = _mqttClient.publish(_telemetryTopic, jsonPayload.c_str());
    
    if (result) {
        Serial.printf("Published telemetry: %s\n", jsonPayload.c_str());
        _lastTelemetryPublish = millis();
    } else {
        Serial.println("Failed to publish telemetry");
    }
    
    return result;
}

bool MQTTHandler::hasCommand() const {
    return _commandAvailable;
}

ManualCommand_t MQTTHandler::getCommand() {
    _commandAvailable = false;
    return _pendingCommand;
}

bool MQTTHandler::connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        _wifiConnected = true;
        return true;
    }
    
    Serial.print("Connecting to WiFi...");
    WiFi.begin(_ssid, _password);
    
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        _wifiConnected = true;
        Serial.println("\nWiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        Serial.println("\nWiFi connection failed");
        _wifiConnected = false;
        return false;
    }
}

bool MQTTHandler::connectMQTT() {
    if (!_wifiConnected) {
        return false;
    }
    
    Serial.print("Connecting to MQTT...");
    if (_mqttClient.connect(_mqttClientId)) {
        _mqttConnected = true;
        Serial.println(" connected");

        if (_mqttClient.subscribe(_commandTopic)) {
            Serial.printf("Subscribed to %s\n", _commandTopic);
        } else {
            Serial.printf("Failed to subscribe to %s\n", _commandTopic);
        }
        
        return true;
    } else {
        Serial.print(" failed, rc=");
        Serial.print(_mqttClient.state());
        Serial.println(" try again in 5 seconds");
        return false;
    }
}

void MQTTHandler::mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    String payloadString;
    for (unsigned int i = 0; i < length; i++) {
        payloadString += (char)payload[i];
    }
    Serial.println(payloadString);

    if (String(topic) == _commandTopic) {
        ManualCommand_t cmd;
        if (parseCommandJson(payloadString, cmd)) {
            _pendingCommand = cmd;
            _commandAvailable = true;
            Serial.println("Command queued for processing");
        } else {
            Serial.println("Failed to parse command JSON");
        }
    }
}

void MQTTHandler::handleTelemetry() {


}

void MQTTHandler::handleCommands() {

}

void MQTTHandler::publishAlert(const char *message) {
    if (_mqttConnected) {
        _mqttClient.publish(_alertTopic, message);
        Serial.printf("Published alert: %s\n", message);
    }
}

String MQTTHandler::createTelemetryJson(const SensorData_t &data) {
    JsonDocument doc;

    doc["temperature"] = data.temperature_c;
    doc["humidity"] = data.humidity_pct;
    doc["timestamp"] = data.timestamp_ms;
    doc["valid"] = data.valid;
    doc["device_id"] = _mqttClientId;

    char jsonBuffer[256];
    size_t n = serializeJson(doc, jsonBuffer);
    return String(jsonBuffer, n);
}

bool MQTTHandler::parseCommandJson(const String &jsonString, ManualCommand_t &cmd) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error) {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        return false;
    }

    if (!doc["duty"].is<int>()) {
        Serial.println("JSON missing 'duty' field");
        return false;
    }

    int dutyRaw = doc["duty"].as<int>();
    cmd.duty_pct = static_cast<uint8_t>(constrain(dutyRaw, 0, 100));
    cmd.timestamp_ms = millis();

    return true;
}