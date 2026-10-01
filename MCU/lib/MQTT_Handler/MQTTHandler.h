

#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "../../include/config.h"
#include "../../include/state_machine.h"

// -----------------------------------------------------------------------------
// Class MQTTHandler đóng gói toàn bộ logic kết nối mạng và giao tiếp MQTT.
// Nó quản lý WiFi, kết nối broker, subscribe topic lệnh, publish telemetry và xử lý cảnh báo.
// -----------------------------------------------------------------------------
class MQTTHandler {
public:
    // Constructor khởi tạo client WiFi/MQTT và các giá trị trạng thái ban đầu.
    MQTTHandler();
    ~MQTTHandler();

    // Khởi tạo WiFi và cấu hình callback nhận MQTT message.
    bool begin();
    // Gọi định kỳ để kiểm tra kết nối và xử lý dữ liệu từ broker.
    void update();

    // Publish dữ liệu telemetry dưới dạng JSON lên topic telemetry.
    bool publishTelemetry(const SensorData_t &data,
                          uint8_t fanDutyPct,
                          uint16_t fanRpm,
                          float targetTemp,
                          SystemMode_t mode,
                          bool alarm,
                          bool shutdown);

    // Các hàm kiểm tra và lấy lệnh điều khiển manual từ MQTT.
    bool hasCommand() const;
    ManualCommand_t getCommand();

    // Trạng thái kết nối của WiFi và MQTT, dùng cho việc decide system mode.
    bool isConnected() const { return _wifiConnected && _mqttConnected; }
    bool isWiFiConnected() const { return _wifiConnected; }
    bool isMQTTConnected() const { return _mqttConnected; }

private:
    WiFiClient _wifiClient;      // Socket client dùng cho kết nối WiFi cơ bản.
    PubSubClient _mqttClient;    // Client MQTT dùng cho subscribe/publish.

    bool _wifiConnected;         // Cờ báo WiFi đã nối thành công.
    bool _mqttConnected;         // Cờ báo MQTT đã kết nối tới broker.

    uint32_t _lastReconnectAttempt; // Thời điểm thử reconnect MQTT gần nhất.
    uint32_t _lastTelemetryPublish; // Thời điểm publish telemetry lần trước.
    uint32_t _lastMQTTLoop;         // Thời điểm vòng lặp MQTT gần nhất, dùng để throttle loop.

    ManualCommand_t _pendingCommand; // Lệnh điều khiển mới nhất đang chờ xử lý.
    bool _commandAvailable;          // Cờ báo có lệnh chưa đọc từ queue.

    const char *_ssid;        // Tên WiFi SSID.
    const char *_password;    // Mật khẩu WiFi.
    const char *_mqttServer;  // Địa chỉ broker MQTT.
    uint16_t _mqttPort;       // Cổng broker MQTT.
    const char *_mqttClientId; // ID client trên MQTT.
    const char *_telemetryTopic; // Topic gửi telemetry.
    const char *_commandTopic;   // Topic nhận lệnh từ xa.
    const char *_alertTopic;     // Topic báo cảnh báo hệ thống.

    // Hàm hỗ trợ kết nối và xử lý callback.
    bool connectWiFi();
    bool connectMQTT();
    void mqttCallback(char* topic, byte* payload, unsigned int length);
    void handleTelemetry();
    void handleCommands();
    void publishAlert(const char *message);

    // Hàm chuyển đổi dữ liệu sensor thành JSON và parse lệnh từ JSON.
    String createTelemetryJson(const SensorData_t &data,
                              uint8_t fanDutyPct,
                              uint16_t fanRpm,
                              float targetTemp,
                              SystemMode_t mode,
                              bool alarm,
                              bool shutdown);
    bool parseCommandJson(const String &jsonString, ManualCommand_t &cmd);
};

#endif // MQTT_HANDLER_H