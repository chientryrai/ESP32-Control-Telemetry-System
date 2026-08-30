# MCU Project Summary

## 1. Tổng quan
Dự án này là firmware cho ESP32 dùng để:
- đọc nhiệt độ và độ ẩm từ cảm biến AHT25
- điều khiển quạt theo chế độ tự động/manual
- giám sát trạng thái hệ thống và an toàn
- giao tiếp WiFi/MQTT
- xử lý trạng thái hệ thống theo state machine

## 2. File quan trọng

### Cấu hình hệ thống
- [platformio.ini](platformio.ini) — cấu hình PlatformIO, board ESP32, thư viện, compile flags
- [include/config.h](include/config.h) — pin mapping, ngưỡng, thời gian, topic MQTT, cấu hình hệ thống
- [include/state_machine.h](include/state_machine.h) — định nghĩa trạng thái, dữ liệu hệ thống, command

### Firmware chính
- [src/main.cpp](src/main.cpp) — setup, task scheduler, vòng lặp chính, logic điều khiển quạt và MQTT

### Sensor
- [lib/AHT25_Driver/AHT25.h](lib/AHT25_Driver/AHT25.h)
- [lib/AHT25_Driver/AHT25.cpp](lib/AHT25_Driver/AHT25.cpp) — driver đọc cảm biến AHT25 qua I2C
- [lib/SensorManager/SensorManager.h](lib/SensorManager/SensorManager.h)
- [lib/SensorManager/SensorManager.cpp](lib/SensorManager/SensorManager.cpp) — lớp quản lý cảm biến, tổng hợp dữ liệu cho hệ thống

### Điều khiển quạt
- [lib/FanController/PIDController.h](lib/FanController/PIDController.h)
- [lib/FanController/PIDController.cpp](lib/FanController/PIDController.cpp) — bộ điều khiển PID cho quạt

### Network và MQTT
- [lib/MQTT_Handler/MQTTHandler.h](lib/MQTT_Handler/MQTTHandler.h)
- [lib/MQTT_Handler/MQTTHandler.cpp](lib/MQTT_Handler/MQTTHandler.cpp) — kết nối WiFi, MQTT, nhận lệnh và publish telemetry

## 3. Luồng hoạt động
1. Khởi tạo GPIO, I2C, WiFi, MQTT.
2. Đọc sensor AHT25 định kỳ.
3. Cập nhật dữ liệu thay đổi trong state machine.
4. Chạy PID để tính đầu ra quạt.
5. Dựa trên mode hệ thống:
   - INIT_MODE
   - AUTO_MODE
   - MANUAL_MODE
   - EMERGENCY_MODE
   - SAFE_MODE
6. Publish dữ liệu và nhận lệnh từ MQTT.

## 4. Ghi chú kỹ thuật
- Cảm biến đang dùng: AHT25
- Hệ thống có hỗ trợ trạng thái tự động và manual
- MQTT dùng để điều khiển từ xa và telemetry
- Tính an toàn có watchdog/emergency logic

## 5. Mục tiêu triển khai tiếp theo
- Test trên board ESP32 thật
- Kiểm tra tín hiệu cảm biến thực tế
- Đánh giá MQTT trên mạng nội bộ/Internet
- Dùng thêm log và cảnh báo khi nhiệt độ quá ngưỡng

## 6. Tóm tắt nhanh
Project chính tập trung vào 3 phần:
- Sensor Layer
- Control Layer
- Communication Layer

Nếu cần, file này có thể dùng làm bản tóm tắt để mở nhanh khi làm việc với MCU.