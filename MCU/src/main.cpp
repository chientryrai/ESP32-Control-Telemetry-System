#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "state_machine.h"
#include "../lib/AHT20_Driver/AHT20.h"
#include "../lib/MQTT_Handler/MQTTHandler.h"
#include "../lib/LCD_Display/LCD_I2C.h"

// Khai báo prototype cho các hàm quản lý hệ thống.
// Các hàm này được định nghĩa bên dưới để thực hiện setup/trạng thái của firmware.
static void initHardware(void);
static void manualTimeoutCallback(TimerHandle_t xTimer);
static void setSystemMode(SystemMode_t newMode);
static void applyModeAction(SystemMode_t mode);

static void Task_SensorRead(void *pvParameters);
static void Task_FanControl(void *pvParameters);
static void Task_Network(void *pvParameters);
static void Task_Emergency_Watchdog(void *pvParameters);
static void Task_Display(void *pvParameters);

// Queue dùng để truyền sample mới nhất từ SensorRead sang FanControl.
// Telemetry queue tách riêng để tránh một consumer ăn mất dữ liệu của consumer khác.
static QueueHandle_t sensorQueue = nullptr;
static QueueHandle_t telemetryQueue = nullptr;
static QueueHandle_t cmdQueue = nullptr;
static EventGroupHandle_t sysEvents = nullptr;
static TimerHandle_t manualTimer = nullptr;
static SemaphoreHandle_t i2cMutex = nullptr;

// Trạng thái hệ thống toàn cục: mode đang hoạt động, mức duty hiện tại và trạng thái network.
static SystemContext_t g_sys = {
    .current_mode = INIT_MODE,
    .fan_duty_pct = 0,
    .network_ready = false,
    .last_temperature_c = 0.0f,
    .last_humidity_pct = 0.0f,
    .last_sensor_valid = false,
    .fan_rpm = 0,
    .target_temp_c = TARGET_TEMP_DEFAULT_C,
    .manual_via_switch = false,
};

static volatile uint32_t g_tachPulseCount = 0;
static volatile uint32_t g_lastTachPulseMs = 0;

// Đối tượng MQTT dùng để kết nối WiFi/MQTT và nhận lệnh điều khiển từ xa.
static MQTTHandler mqttHandler;

extern "C" {
// ISR của tín hiệu tachometer quạt: chỉ đếm xung, không báo khẩn cấp trực tiếp.
static void IRAM_ATTR gpio_isr_handler() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    g_tachPulseCount++;
    g_lastTachPulseMs = millis();
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
}

// setup(): chạy một lần khi boot hệ thống.
// Chức năng: khởi tạo GPIO, queue, timer, event group và tạo tất cả các task RTOS.
void setup() {
    Serial.begin(115200);  // Mở cổng serial để debug gỡ lỗi và xem log runtime.

    initHardware();  // Khởi tạo I2C, PWM quạt và interrupt tachometer.

    // Tạo queue gửi dữ liệu cảm biến mới nhất và lệnh điều khiển manual từ MQTT.
    sensorQueue = xQueueCreate(1, sizeof(SensorData_t));
    telemetryQueue = xQueueCreate(1, sizeof(SensorData_t));
    cmdQueue = xQueueCreate(CMD_QUEUE_LEN, sizeof(ManualCommand_t));
    sysEvents = xEventGroupCreate();
    i2cMutex = xSemaphoreCreateMutex();

    configASSERT(sensorQueue != nullptr);
    configASSERT(telemetryQueue != nullptr);
    configASSERT(cmdQueue != nullptr);
    configASSERT(sysEvents != nullptr);
    configASSERT(i2cMutex != nullptr);

    // Timer dùng để tự động quay lại AUTO_MODE nếu không có lệnh manual mới trong thời gian quy định.
    manualTimer = xTimerCreate(
        "manualTimeout",
        pdMS_TO_TICKS(MANUAL_TIMEOUT_MS),
        pdFALSE,
        (void *)0,
        manualTimeoutCallback);
    configASSERT(manualTimer != nullptr);

    // Tạo các task FreeRTOS với ưu tiên khác nhau để tối ưu hóa thời gian và độ nhạy của firmware.
    xTaskCreatePinnedToCore(Task_Emergency_Watchdog, "Watchdog",
                            TASK_STACK_WATCHDOG, nullptr,
                            TASK_PRIO_WATCHDOG, nullptr, 1);
    xTaskCreatePinnedToCore(Task_FanControl, "FanControl",
                            TASK_STACK_FAN, nullptr,
                            TASK_PRIO_FAN, nullptr, 1);
    xTaskCreatePinnedToCore(Task_SensorRead, "SensorRead",
                            TASK_STACK_SENSOR, nullptr,
                            TASK_PRIO_SENSOR, nullptr, 0);
    xTaskCreatePinnedToCore(Task_Network, "Network",
                            TASK_STACK_NETWORK, nullptr,
                            TASK_PRIO_NETWORK, nullptr, 0);
    xTaskCreatePinnedToCore(Task_Display, "Display",
                            TASK_STACK_DISPLAY, nullptr,
                            TASK_PRIO_DISPLAY, nullptr, 0);

    // Scheduler của Arduino-ESP32 đã được khởi động sẵn; không gọi hand-start lại.
}

// Toàn bộ logic và watchdog được quản lý bởi các task FreeRTOS riêng.
void loop() {
}

// Khởi tạo phần cứng: I2C, PWM quạt, interrupt tachometer và watchdog.
static void initHardware(void) {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, SENSOR_I2C_FREQ_HZ);

    // Thiết lập kênh PWM quạt 25kHz, 10-bit resolution cho điều khiển độ rộng xung.
    ledcSetup(LEDC_FAN_CHANNEL, LEDC_FAN_FREQ_HZ, LEDC_FAN_RESOLUTION);
    ledcAttachPin(PIN_FAN_PWM, LEDC_FAN_CHANNEL);
    ledcWrite(LEDC_FAN_CHANNEL, 0);

    // Tachometer quạt và các switch mức dùng pull-up nội để đọc tín hiệu đầu vào.
    pinMode(PIN_FAN_TACH, INPUT_PULLUP);
    pinMode(PIN_SWITCH_LEVEL_0, INPUT_PULLUP);
    pinMode(PIN_SWITCH_LEVEL_33, INPUT_PULLUP);
    pinMode(PIN_SWITCH_LEVEL_66, INPUT_PULLUP);
    pinMode(PIN_SWITCH_LEVEL_100, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACH), gpio_isr_handler, FALLING);

    // WDT ESP32 được khởi tạo để phát hiện task treo hoặc nhầm lẫn trong vòng lặp.
    esp_task_wdt_init(WDT_TIMEOUT_S, true);

    g_tachPulseCount = 0;
    g_lastTachPulseMs = millis();
}

// Cập nhật mode hệ thống và gọi applyModeAction để đổi trạng thái hardware tương ứng.
static void setSystemMode(SystemMode_t newMode) {
    if (g_sys.current_mode == newMode) {
        return;  // Nếu đang ở cùng mode thì bỏ qua để tránh lặp lại hành động không cần thiết.
    }

    g_sys.current_mode = newMode;
    applyModeAction(newMode);
}

// Chuyển đổi trạng thái mode thành PWM quạt tương ứng.
static void applyModeAction(SystemMode_t mode) {
    switch (mode) {
        case INIT_MODE:
            ledcWrite(LEDC_FAN_CHANNEL, 0);
            break;

        case AUTO_MODE:
            break;

        case MANUAL_MODE:
            break;

        case EMERGENCY_MODE:
            ledcWrite(LEDC_FAN_CHANNEL, (1UL << LEDC_FAN_RESOLUTION) - 1UL);
            break;

        default:
            break;
    }
}

// Timer callback: khi lệnh manual quá hạn thì báo event để trở về AUTO_MODE.
static void manualTimeoutCallback(TimerHandle_t xTimer) {
    (void)xTimer;
    if (sysEvents != nullptr) {
        xEventGroupSetBits(sysEvents, EVT_MANUAL_TIMEOUT);
    }
}

// Task đọc sensor: khởi tạo AHT20, đọc dữ liệu định kỳ, publish lên queue cho FanControl.
static void Task_SensorRead(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    static AHT20 sensor(Wire);
    static bool sensorInitialized = false;

    if (!sensorInitialized) {
        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        sensorInitialized = sensor.begin();
        xSemaphoreGive(i2cMutex);
        if (!sensorInitialized) {
            xEventGroupSetBits(sysEvents, EVT_SENSOR_TIMEOUT);
        }
    }

    for (;;) {
        bool sensorUpdated = false;
        if (sensorInitialized) {
            xSemaphoreTake(i2cMutex, portMAX_DELAY);
            sensorUpdated = sensor.update();
            xSemaphoreGive(i2cMutex);
        }

        if (sensorUpdated) {
            const AHT20_Data_t raw = sensor.getLastResult();
            SensorData_t sample;
            sample.temperature_c = raw.temperature;
            sample.humidity_pct  = raw.humidity;
            sample.timestamp_ms  = raw.timestamp;
            sample.valid         = raw.valid;
            g_sys.last_temperature_c = sample.temperature_c;
            g_sys.last_humidity_pct = sample.humidity_pct;
            g_sys.last_sensor_valid = sample.valid;

            xQueueOverwrite(sensorQueue, &sample);
            if (sample.valid) {
                xQueueOverwrite(telemetryQueue, &sample);
                xEventGroupSetBits(sysEvents, EVT_SENSOR_OK);
                xEventGroupClearBits(sysEvents, EVT_SENSOR_TIMEOUT);

                if (sample.temperature_c > TEMP_CRITICAL_C ||
                    sample.humidity_pct > HUM_CRITICAL_HIGH ||
                    sample.humidity_pct < HUM_CRITICAL_LOW) {
                    xEventGroupSetBits(sysEvents, EVT_EMERGENCY);
                }
            } else {
                xEventGroupSetBits(sysEvents, EVT_SENSOR_TIMEOUT);
            }
        } else if (!sensorInitialized) {
            if (xTaskGetTickCount() % 5000 == 0) {
                xSemaphoreTake(i2cMutex, portMAX_DELAY);
                sensorInitialized = sensor.begin();
                xSemaphoreGive(i2cMutex);
                if (sensorInitialized) {
                    xEventGroupClearBits(sysEvents, EVT_SENSOR_TIMEOUT);
                }
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(SENSOR_READ_PERIOD_MS));
    }
}

// Task điều khiển quạt dựa trên mode hiện tại và dữ liệu nhiệt độ từ sensor.
static void Task_FanControl(void *pvParameters) {
    (void)pvParameters;
    SensorData_t data;
    ManualCommand_t manualCmd;

    static uint32_t lastPIDTime = 0;
    static float targetTemp = TARGET_TEMP_DEFAULT_C;
    static float emaRpm = 0.0f;
    g_sys.target_temp_c = targetTemp;

    for (;;) {
        EventBits_t bits = xEventGroupGetBits(sysEvents);
        int manualSwPct = -1;
        if (digitalRead(PIN_SWITCH_LEVEL_0) == LOW) {
            manualSwPct = 0;
        } else if (digitalRead(PIN_SWITCH_LEVEL_33) == LOW) {
            manualSwPct = 33;
        } else if (digitalRead(PIN_SWITCH_LEVEL_66) == LOW) {
            manualSwPct = 66;
        } else if (digitalRead(PIN_SWITCH_LEVEL_100) == LOW) {
            manualSwPct = 100;
        }

        // Emergency has priority; physical switches override MQTT manual commands.
        if (bits & EVT_EMERGENCY) {
            setSystemMode(EMERGENCY_MODE);
            g_sys.fan_duty_pct = 50;
        } else if (manualSwPct >= 0) {
            setSystemMode(MANUAL_MODE);
            g_sys.manual_via_switch = true;
            g_sys.fan_duty_pct = (uint8_t)manualSwPct;
            xTimerStop(manualTimer, 0);
            xEventGroupClearBits(sysEvents, EVT_MANUAL_TIMEOUT);
        } else if (g_sys.current_mode == MANUAL_MODE && g_sys.manual_via_switch) {
            g_sys.manual_via_switch = false;
            setSystemMode(AUTO_MODE);
        } else if (bits & EVT_SENSOR_TIMEOUT) {
            g_sys.manual_via_switch = false;
            setSystemMode(AUTO_MODE);
            xEventGroupClearBits(sysEvents, EVT_SENSOR_TIMEOUT);
        } else if (bits & EVT_MANUAL_TIMEOUT) {
            g_sys.manual_via_switch = false;
            setSystemMode(AUTO_MODE);
            xEventGroupClearBits(sysEvents, EVT_MANUAL_TIMEOUT);
        } else if (g_sys.current_mode == INIT_MODE) {
            setSystemMode(AUTO_MODE);
        }

        if (manualSwPct >= 0) {
            while (xQueueReceive(cmdQueue, &manualCmd, 0) == pdTRUE) {
            }
        } else if (g_sys.current_mode != EMERGENCY_MODE &&
                   xQueueReceive(cmdQueue, &manualCmd, 0) == pdTRUE) {
            setSystemMode(MANUAL_MODE);
            g_sys.manual_via_switch = false;
            g_sys.fan_duty_pct = (uint8_t)constrain(manualCmd.duty_pct, 0, 100);
            xTimerReset(manualTimer, portMAX_DELAY);
            xTimerStart(manualTimer, portMAX_DELAY);
        }

        const uint32_t nowMs = millis();
        if (g_sys.current_mode != INIT_MODE &&
            g_sys.fan_duty_pct > FAN_STALL_DUTY_PCT &&
            (nowMs - g_lastTachPulseMs) > 2000UL) {
            setSystemMode(EMERGENCY_MODE);
            g_sys.fan_duty_pct = 50;
            xEventGroupSetBits(sysEvents, EVT_EMERGENCY);
        }

        if (g_sys.current_mode == EMERGENCY_MODE) {
            g_sys.fan_duty_pct = 50;
        }

        noInterrupts();
        const uint32_t pulses = g_tachPulseCount;
        g_tachPulseCount = 0;
        interrupts();
        const float rpm = (pulses * 60000.0f) /
                          (FAN_TACH_PULSES_PER_REV * FAN_UPDATE_PERIOD_MS);
        const float boundedRpm = rpm > MAX_PHYSICAL_RPM ? 0.0f : rpm;
        emaRpm = EMA_ALPHA * boundedRpm + (1.0f - EMA_ALPHA) * emaRpm;
        g_sys.fan_rpm = (uint16_t)emaRpm;

        // Nếu có dữ liệu cảm biến mới, tiến hành tính toán duty cycle theo mode hiện tại.
        if (xQueueReceive(sensorQueue, &data, 0) == pdTRUE && data.valid) {
            if (millis() - lastPIDTime >= 200) {
                lastPIDTime = millis();

                switch (g_sys.current_mode) {
                    case AUTO_MODE: {
                        const float temperatureDelta = data.temperature_c - targetTemp;
                        const float requestedDuty = temperatureDelta * FAN_DUTY_PER_DEGREE_PCT;

                        uint8_t duty;
                        if (requestedDuty <= 0.5f) {
                            duty = 0;  // Không cần làm mát -> tắt quạt.
                        } else if (requestedDuty < FAN_STALL_DUTY_PCT) {
                            duty = FAN_STALL_DUTY_PCT;  // Ép mức tối thiểu để quạt không dừng do stall.
                        } else {
                            duty = (uint8_t)constrain(requestedDuty, 0.0f, 100.0f);
                        }

                        g_sys.fan_duty_pct = duty;
                        break;
                    }

                    case MANUAL_MODE:
                        break;

                    case EMERGENCY_MODE:
                        g_sys.fan_duty_pct = 50;
                        break;

                    case INIT_MODE:
                    default:
                        g_sys.fan_duty_pct = 0;
                        break;
                }
            }
        }

        // Chuyển duty percentage sang giá trị PWM chuẩn của ESP32 để điều khiển quạt.
        const uint8_t duty = g_sys.fan_duty_pct;
        const uint32_t pwmMax = (1UL << LEDC_FAN_RESOLUTION) - 1UL;
        const uint32_t pwmValue = duty == 0
                          ? 0
                          : (uint32_t)map(duty, 0, 100, 0, (int)pwmMax);
        ledcWrite(LEDC_FAN_CHANNEL, pwmValue);

        vTaskDelay(pdMS_TO_TICKS(FAN_UPDATE_PERIOD_MS));
    }
}

// Task network: duy trì kết nối WiFi/MQTT, nhận lệnh manual và publish telemetry.
static void Task_Network(void *pvParameters) {
    (void)pvParameters;
    static bool mqttInitialized = false;
    static SensorData_t lastTelemetryData;
    static uint32_t lastTelemetryTime = 0;

    if (!mqttInitialized) {
        mqttInitialized = mqttHandler.begin();
        if (!mqttInitialized) {
            Serial.println("Failed to initialize MQTT handler");
        }
    }

    for (;;) {
        mqttHandler.update();

        bool networkReady = mqttHandler.isConnected();
        if (networkReady != g_sys.network_ready) {
            g_sys.network_ready = networkReady;
            if (networkReady) {
                xEventGroupSetBits(sysEvents, EVT_NETWORK_READY);
                Serial.println("Network ready - MQTT connected");
            } else {
                xEventGroupClearBits(sysEvents, EVT_NETWORK_READY);
                Serial.println("Network not ready - MQTT disconnected");
            }
        }

        // Queue MQTT commands; FanControl applies them after checking physical switches.
        if (mqttHandler.hasCommand()) {
            ManualCommand_t cmd = mqttHandler.getCommand();
            if (xQueueSend(cmdQueue, &cmd, 0) == pdTRUE) {
                xEventGroupSetBits(sysEvents, EVT_MANUAL_ACTIVE);
                Serial.printf("Manual command received: duty=%u%%\n", cmd.duty_pct);
            }
        }

        // Publish telemetry theo chu kỳ nếu mạng đã kết nối.
        // Queue telemetry được tách khỏi queue điều khiển quạt để mỗi consumer nhận dữ liệu riêng.
        if (mqttHandler.isConnected() &&
            xQueueReceive(telemetryQueue, &lastTelemetryData, 0) == pdTRUE &&
            lastTelemetryData.valid &&
            millis() - lastTelemetryTime >= MQTT_PUBLISH_PERIOD_MS) {

            const bool alarm = (g_sys.current_mode == EMERGENCY_MODE) ||
                               ((xEventGroupGetBits(sysEvents) & EVT_EMERGENCY) != 0U);
            const bool shutdown = false;

            if (mqttHandler.publishTelemetry(lastTelemetryData,
                                            g_sys.fan_duty_pct,
                                            g_sys.fan_rpm,
                                            g_sys.target_temp_c,
                                            g_sys.current_mode,
                                            alarm,
                                            shutdown)) {
                Serial.printf("Telemetry published: T=%.1fC H=%.1f%% duty=%u mode=%d\n",
                              lastTelemetryData.temperature_c,
                              lastTelemetryData.humidity_pct,
                              g_sys.fan_duty_pct,
                              (int)g_sys.current_mode);
            } else {
                Serial.println("Failed to publish telemetry");
            }
            lastTelemetryTime = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(NETWORK_PERIOD_MS));
    }
}

// Task watchdog: theo dõi các sự kiện cảnh báo an toàn và chuyển hệ thống về trạng thái thích hợp.
static void Task_Emergency_Watchdog(void *pvParameters) {
    (void)pvParameters;
    esp_task_wdt_add(nullptr);

    for (;;) {
        EventBits_t bits = xEventGroupWaitBits(
            sysEvents,
            EVT_EMERGENCY | EVT_WDT_FAULT,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(2000));

        if (bits & EVT_EMERGENCY) {
            setSystemMode(EMERGENCY_MODE);
            xEventGroupClearBits(sysEvents, EVT_MANUAL_ACTIVE);
        }

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(EMERGENCY_REACT_MS));
    }
}

static void Task_Display(void *pvParameters) {
    (void)pvParameters;
    lcdInit(LCD_I2C_ADDR);
    lcdBacklight(true);

    uint32_t lastPageChange = millis();
    bool showMode = false;
    char line0[LCD_COLS + 1];
    char line1[LCD_COLS + 1];

    for (;;) {
        if (millis() - lastPageChange >= 2500UL) {
            showMode = !showMode;
            lastPageChange = millis();
        }

        if (g_sys.last_sensor_valid) {
            snprintf(line0, sizeof(line0), "T:%.1fC H:%.0f%%",
                     (double)g_sys.last_temperature_c,
                     (double)g_sys.last_humidity_pct);
        } else {
            snprintf(line0, sizeof(line0), "AHT20 ERROR");
        }

        if (showMode) {
            const char *modeName = "INIT";
            switch (g_sys.current_mode) {
                case AUTO_MODE: modeName = "AUTO"; break;
                case MANUAL_MODE: modeName = "MANU"; break;
                case EMERGENCY_MODE: modeName = "EMRG"; break;
                case INIT_MODE:
                default: modeName = "INIT"; break;
            }
            snprintf(line1, sizeof(line1), "F:%03u%% %s",
                     (unsigned)g_sys.fan_duty_pct, modeName);
        } else {
            snprintf(line1, sizeof(line1), "F:%03u%% R:%4u",
                     (unsigned)g_sys.fan_duty_pct, (unsigned)g_sys.fan_rpm);
        }

        for (size_t i = strlen(line0); i < LCD_COLS; ++i) {
            line0[i] = ' ';
        }
        line0[LCD_COLS] = '\0';
        for (size_t i = strlen(line1); i < LCD_COLS; ++i) {
            line1[i] = ' ';
        }
        line1[LCD_COLS] = '\0';

        if (xSemaphoreTake(i2cMutex, 0) == pdTRUE) {
            lcdSetCursor(0, 0);
            lcdPrint(line0);
            lcdSetCursor(0, 1);
            lcdPrint(line1);
            xSemaphoreGive(i2cMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_PERIOD_MS));
    }
}