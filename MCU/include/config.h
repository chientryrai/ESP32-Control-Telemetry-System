

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// -----------------------------------------------------------------------------
// Bảng pin trung tâm cho firmware chính.
// Giữ pin theo chuẩn tham chiếu của Peripheral Test Rig và tránh trùng lẫn giữa
// tín hiệu đầu vào (switch / tach) và đầu ra điều khiển (PWM).
// -----------------------------------------------------------------------------
#define PIN_I2C_SDA         21      // SDA cho bus I2C AHT20/LCD
#define PIN_I2C_SCL         22      // SCL cho bus I2C AHT20/LCD
#define PIN_FAN_PWM         25      // PWM quạt 4 dây, LEDC channel 0
#define PIN_FAN_TACH        26      // Tín hiệu xung tachometer quạt
#define PIN_SWITCH_LEVEL_0   14      // Switch mức 0% (INPUT_PULLUP)
#define PIN_SWITCH_LEVEL_33  27      // Switch mức 33% (INPUT_PULLUP)
#define PIN_SWITCH_LEVEL_66  32      // Switch mức 66% (INPUT_PULLUP)
#define PIN_SWITCH_LEVEL_100 33      // Switch mức 100% (INPUT_PULLUP)

// -----------------------------------------------------------------------------
// Cấu hình PWM quạt.
// LEDC là bộ điều chế xung dùng trên ESP32 để tạo tín hiệu PWM ổn định cho quạt.
// -----------------------------------------------------------------------------
#define LEDC_FAN_CHANNEL    0
#define LEDC_FAN_TIMER      0
#define LEDC_FAN_RESOLUTION 10   // 10-bit => 1024 mức, phù hợp với PWM 25 kHz.
#define LEDC_FAN_FREQ_HZ    25000 // Tần số 25 kHz phù hợp với tiêu chuẩn quạt 4 dây, giảm tiếng ồn và tăng độ ổn định.
#define FAN_TACH_PULSES_PER_REV 2
#define MAX_PHYSICAL_RPM    20000
#define EMA_ALPHA           0.1f

// -----------------------------------------------------------------------------
// Cấu hình bus I2C và địa chỉ sensor.
// AHT20 dùng địa chỉ 0x38; các định nghĩa stale khác được giữ nhằm dễ tương thích với phiên bản cũ.
// -----------------------------------------------------------------------------
#define SENSOR_I2C_ADDR_AHT20   0x38
#define SENSOR_I2C_FREQ_HZ      100000UL   // Bus I2C chạy ở tốc độ 100 kHz theo chuẩn I2C thông dụng.
#define LCD_I2C_ADDR            0x27
#define LCD_COLS                16
#define LCD_ROWS                2


// -----------------------------------------------------------------------------
// Giới hạn nhiệt độ và độ ẩm dùng để kích hoạt trạng thái an toàn và khẩn cấp.
// Những ngưỡng này quyết định khi nào quạt phải tăng tốc hoặc hệ thống chuyển sang EMERGENCY_MODE.
// -----------------------------------------------------------------------------
#define TEMP_MIN_C          20.0f   // Ngưỡng dưới này được xem như nhiệt độ quá thấp, quạt có thể tắt.
#define TEMP_MAX_C          40.0f   // Nhiệt độ tối đa bình thường trong phạm vi hoạt động thông thường.
#define TEMP_CRITICAL_C     45.0f   // Nếu vượt ngưỡng này => chuyển sang EMERGENCY_MODE.
#define HUM_CRITICAL_HIGH   85.0f   // Độ ẩm quá cao => cảnh báo và ngắt an toàn nếu cần.
#define HUM_CRITICAL_LOW    15.0f   // Độ ẩm quá thấp => cảnh báo bất thường về môi trường.

// -----------------------------------------------------------------------------
// Cấu hình mức duty quạt.
// FAN_SAFE_DUTY_PCT là mức chạy an toàn dự phòng để hệ thống duy trì hoạt động ổn định.
// FAN_STALL_DUTY_PCT là mức tối thiểu bắt buộc để quạt không bị stall trong AUTO_MODE.
// -----------------------------------------------------------------------------

#define TARGET_TEMP_DEFAULT_C   27.0f
#define FAN_DUTY_PER_DEGREE_PCT 10.0f
#define FAN_STALL_DUTY_PCT      20    // Mức duty tối thiểu bắt buộc để quạt quay chắc chắn trong AUTO_MODE.
#define DISPLAY_UPDATE_PERIOD_MS 200UL

// -----------------------------------------------------------------------------
// Chu kỳ lặp của từng task.
// Mỗi task có khoảng thời gian nhắc lại khác nhau để cân bằng giữa độ nhạy và chi phí CPU.
// -----------------------------------------------------------------------------
#define SENSOR_READ_PERIOD_MS   1000UL   // Chu kỳ đọc sensor AHT25, khoảng 1 giây.
#define FAN_UPDATE_PERIOD_MS    500UL    // Chu kỳ cập nhật quạt, 0.5 giây.
#define NETWORK_PERIOD_MS       100UL    // Chu kỳ duy trì kết nối MQTT và xử lý mạng.
#define MQTT_KEEPALIVE_S        30
#define MQTT_PUBLISH_PERIOD_MS  5000UL   // Khoảng thời gian publish telemetry lên broker.
#define MANUAL_TIMEOUT_MS       30000UL  // Nếu hết thời gian mà không có lệnh manual mới, quay về AUTO_MODE.
#define I2C_TIMEOUT_MS          2000UL   // Timeout I2C khi sensor không phản hồi, hệ thống quay về AUTO_MODE.
#define EMERGENCY_REACT_MS      1000UL   // Thời gian tối đa hệ thống phản ứng với sự kiện khẩn cấp.
#define WDT_TIMEOUT_S           10       // Cửa sổ watchdog phần cứng, nếu task treo sẽ reset.

// -----------------------------------------------------------------------------
// Thông tin đăng nhập và topic MQTT.
// Khi triển khai thật, các giá trị này phải được thay bằng SSID, password và broker thực tế.
// -----------------------------------------------------------------------------
#define WIFI_SSID           "YOUR_SSID"
#define WIFI_PASSWORD       "YOUR_PASSWORD"
#define MQTT_BROKER         "192.168.1.100"
#define MQTT_PORT           1883
#define MQTT_CLIENT_ID      "esp32-fan-controller"
#define MQTT_TOPIC_TELEM    "env/telemetry"
#define MQTT_TOPIC_CMD      "env/command"
#define MQTT_TOPIC_ALERT    "env/alert"

// -----------------------------------------------------------------------------
// Cấu hình stack và độ ưu tiên task RTOS.
// Stack size ảnh hưởng tới bộ nhớ, ưu tiên ảnh hưởng tới mức phản ứng của task.
// -----------------------------------------------------------------------------
#define TASK_STACK_SENSOR       4096    // Bộ nhớ stack cho task đọc sensor.
#define TASK_STACK_FAN          3072    // Bộ nhớ stack cho task điều khiển quạt.
#define TASK_STACK_NETWORK      6144    // Bộ nhớ stack cho task MQTT/network lớn hơn vì xử lý JSON.
#define TASK_STACK_WATCHDOG     2048    // Bộ nhớ stack cho task giám sát sự kiện nguy hiểm.
#define TASK_STACK_DISPLAY      2048

#define TASK_PRIO_WATCHDOG      (configMAX_PRIORITIES - 1)  // Task watchdog ưu tiên cao nhất.
#define TASK_PRIO_FAN           (configMAX_PRIORITIES - 2)  // Task fan control ưu tiên cao.
#define TASK_PRIO_SENSOR        (configMAX_PRIORITIES - 3)  // Task sensor có mức ưu tiên vừa.
#define TASK_PRIO_NETWORK       (configMAX_PRIORITIES - 4)  // Task network ưu tiên thấp hơn do phụ thuộc giao tiếp mạng.
#define TASK_PRIO_DISPLAY       (configMAX_PRIORITIES - 5)

// -----------------------------------------------------------------------------
// Độ sâu queue và bit event.
// Queue dùng để truyền dữ liệu cảm biến và lệnh người dùng; event dùng để báo trạng thái hệ thống.
// -----------------------------------------------------------------------------
#define SENSOR_QUEUE_LEN        8       // Độ sâu queue chứa mẫu dữ liệu sensor.
#define CMD_QUEUE_LEN           8       // Độ sâu queue chứa lệnh điều khiển thủ công.

#define EVT_NETWORK_READY       (1UL << 0)
#define EVT_SENSOR_OK           (1UL << 1)
#define EVT_SENSOR_TIMEOUT      (1UL << 2)
#define EVT_EMERGENCY           (1UL << 3)
#define EVT_MANUAL_ACTIVE       (1UL << 4)
#define EVT_MANUAL_TIMEOUT      (1UL << 5)
#define EVT_WDT_FAULT           (1UL << 6)

#endif // CONFIG_H