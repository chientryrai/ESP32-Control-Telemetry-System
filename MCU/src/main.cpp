#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <freertos/timers.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "state_machine.h"
#include "../lib/SensorManager/SensorManager.h"
#include "../lib/FanController/PIDController.h"
#include "../lib/MQTT_Handler/MQTTHandler.h"

static void initHardware(void);
static void manualTimeoutCallback(TimerHandle_t xTimer);
static void setSystemMode(SystemMode_t newMode);
static void applyModeAction(SystemMode_t mode);

static void Task_SensorRead(void *pvParameters);
static void Task_FanControl(void *pvParameters);
static void Task_Network(void *pvParameters);
static void Task_Emergency_Watchdog(void *pvParameters);

static QueueHandle_t sensorQueue = nullptr;
static QueueHandle_t cmdQueue = nullptr;
static EventGroupHandle_t sysEvents = nullptr;
static TimerHandle_t manualTimer = nullptr;

static SystemContext_t g_sys = {
    .current_mode = INIT_MODE,
    .fan_duty_pct = 0,
    .network_ready = false,
};

static MQTTHandler mqttHandler;

extern "C" {
static void IRAM_ATTR gpio_isr_handler() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (sysEvents != nullptr) {
        xEventGroupSetBitsFromISR(sysEvents, EVT_EMERGENCY, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
}

void setup() {
    Serial.begin(115200);

    initHardware();

    sensorQueue = xQueueCreate(SENSOR_QUEUE_LEN, sizeof(SensorData_t));
    cmdQueue = xQueueCreate(CMD_QUEUE_LEN, sizeof(ManualCommand_t));
    sysEvents = xEventGroupCreate();

    configASSERT(sensorQueue != nullptr);
    configASSERT(cmdQueue != nullptr);
    configASSERT(sysEvents != nullptr);

    manualTimer = xTimerCreate(
        "manualTimeout",
        pdMS_TO_TICKS(MANUAL_TIMEOUT_MS),
        pdFALSE,
        (void *)0,
        manualTimeoutCallback);
    configASSERT(manualTimer != nullptr);

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

    vTaskStartScheduler();

    for (;;) {
    }
}

void loop() {
    vTaskDelete(nullptr);
}

static void initHardware(void) {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, SENSOR_I2C_FREQ_HZ);

    ledcSetup(LEDC_FAN_CHANNEL, LEDC_FAN_FREQ_HZ, LEDC_FAN_RESOLUTION);
    ledcAttachPin(PIN_FAN_PWM, LEDC_FAN_CHANNEL);
    ledcWrite(LEDC_FAN_CHANNEL, 0);

    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);

    pinMode(PIN_FAN_TACH, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACH), gpio_isr_handler, FALLING);

    esp_task_wdt_init(WDT_TIMEOUT_S, true);
    esp_task_wdt_add(nullptr);
}

static void setSystemMode(SystemMode_t newMode) {
    if (g_sys.current_mode == newMode) {
        return;
    }

    g_sys.current_mode = newMode;
    applyModeAction(newMode);
}

static void applyModeAction(SystemMode_t mode) {
    switch (mode) {
        case INIT_MODE:
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_RED, LOW);
            digitalWrite(PIN_BUZZER, LOW);
            ledcWrite(LEDC_FAN_CHANNEL, 0);
            break;

        case AUTO_MODE:
            digitalWrite(PIN_LED_GREEN, HIGH);
            digitalWrite(PIN_LED_RED, LOW);
            digitalWrite(PIN_BUZZER, LOW);
            break;

        case MANUAL_MODE:
            digitalWrite(PIN_LED_GREEN, HIGH);
            digitalWrite(PIN_LED_RED, LOW);
            break;

        case EMERGENCY_MODE:
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_RED, HIGH);
            digitalWrite(PIN_BUZZER, HIGH);
            ledcWrite(LEDC_FAN_CHANNEL, 8191);
            break;

        case SAFE_MODE:
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_RED, HIGH);
            digitalWrite(PIN_BUZZER, LOW);
            ledcWrite(LEDC_FAN_CHANNEL, 4095);
            break;

        default:
            break;
    }
}

static void manualTimeoutCallback(TimerHandle_t xTimer) {
    (void)xTimer;
    if (sysEvents != nullptr) {
        xEventGroupSetBits(sysEvents, EVT_MANUAL_TIMEOUT);
    }
}

static void Task_SensorRead(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    static SensorManager sensorMgr(Wire);
    static bool sensorInitialized = false;

    if (!sensorInitialized) {
        sensorInitialized = sensorMgr.begin();
        if (!sensorInitialized) {
            xEventGroupSetBits(sysEvents, EVT_SENSOR_TIMEOUT);
        }
    }

    for (;;) {
        if (sensorInitialized && sensorMgr.update()) {
            SensorData_t sample = sensorMgr.getLastReading();

            if (xQueueSend(sensorQueue, &sample, 0) != pdTRUE) {
                xEventGroupSetBits(sysEvents, EVT_SENSOR_TIMEOUT);
            } else {
                xEventGroupSetBits(sysEvents, EVT_SENSOR_OK);

                if (sample.temperature_c > TEMP_CRITICAL_C ||
                    sample.humidity_pct > HUM_CRITICAL_HIGH ||
                    sample.humidity_pct < HUM_CRITICAL_LOW) {
                    xEventGroupSetBits(sysEvents, EVT_EMERGENCY);
                }
            }
        } else if (!sensorInitialized) {
            if (xTaskGetTickCount() % 5000 == 0) {
                sensorInitialized = sensorMgr.begin();
                if (sensorInitialized) {
                    xEventGroupClearBits(sysEvents, EVT_SENSOR_TIMEOUT);
                }
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(SENSOR_READ_PERIOD_MS));
    }
}

static void Task_FanControl(void *pvParameters) {
    (void)pvParameters;
    SensorData_t data;
    ManualCommand_t manualCmd;

    static PIDController fanPID(2.0f, 0.5f, 1.0f, 0.0f, 100.0f);
    static uint32_t lastPIDTime = 0;
    static float targetTemp = TARGET_TEMP_DEFAULT_C;

    for (;;) {
        EventBits_t bits = xEventGroupGetBits(sysEvents);

        if (bits & EVT_EMERGENCY) {
            setSystemMode(EMERGENCY_MODE);
        } else if (bits & EVT_SENSOR_TIMEOUT) {
            setSystemMode(SAFE_MODE);
        } else if (bits & EVT_MANUAL_TIMEOUT) {
            setSystemMode(AUTO_MODE);
        } else if (g_sys.current_mode == INIT_MODE) {
            setSystemMode(AUTO_MODE);
        }

        if (xQueueReceive(sensorQueue, &data, 0) == pdTRUE && data.valid) {
            if (millis() - lastPIDTime >= 200) {
                lastPIDTime = millis();

                float dutyCycle = 0.0f;

                switch (g_sys.current_mode) {
                    case AUTO_MODE: {
                float raw = fanPID.compute(targetTemp, data.temperature_c);

                uint8_t duty;
                if (raw <= 0.5f) {
                    duty = 0;                       // không cần làm mát -> tắt hẳn
                } else if (raw < FAN_STALL_DUTY_PCT) {
                    duty = FAN_STALL_DUTY_PCT;      // ép lên sàn để quạt chắc chắn quay, tránh "stall"
                } else {
                    duty = (uint8_t)constrain(raw, 0.0f, 100.0f);
                }

                g_sys.fan_duty_pct = duty;
                break;
            }

                    case MANUAL_MODE:
                        if (xQueueReceive(cmdQueue, &manualCmd, 0) == pdTRUE) {
                            g_sys.fan_duty_pct = manualCmd.duty_pct;
                            xTimerReset(manualTimer, portMAX_DELAY);
                            xTimerStart(manualTimer, portMAX_DELAY);
                        }
                        break;

                    case SAFE_MODE:
                        g_sys.fan_duty_pct = FAN_SAFE_DUTY_PCT;
                        break;

                    case EMERGENCY_MODE:
                        g_sys.fan_duty_pct = FAN_MAX_DUTY_PCT;
                        break;

                    case INIT_MODE:
                    default:
                        g_sys.fan_duty_pct = FAN_MIN_DUTY_PCT;
                        break;
                }
            }
        }

        const uint32_t pwmMax = (1UL << LEDC_FAN_RESOLUTION) - 1UL;
        const uint32_t pwmValue = (uint32_t)map(g_sys.fan_duty_pct, 0, 100, 0, (int)pwmMax);
        ledcWrite(LEDC_FAN_CHANNEL, pwmValue);

        vTaskDelay(pdMS_TO_TICKS(FAN_UPDATE_PERIOD_MS));
    }
}

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

        if (mqttHandler.hasCommand()) {
            ManualCommand_t cmd = mqttHandler.getCommand();
            if (xQueueSend(cmdQueue, &cmd, 0) == pdTRUE) {
                setSystemMode(MANUAL_MODE);
                xEventGroupSetBits(sysEvents, EVT_MANUAL_ACTIVE);
                xTimerReset(manualTimer, portMAX_DELAY);
                xTimerStart(manualTimer, portMAX_DELAY);
                Serial.printf("Manual command received: duty=%u%%\n", cmd.duty_pct);
            }
        }

        if (mqttHandler.isConnected() &&
            xQueueReceive(sensorQueue, &lastTelemetryData, 0) == pdTRUE &&
            lastTelemetryData.valid &&
            millis() - lastTelemetryTime >= MQTT_PUBLISH_PERIOD_MS) {

            if (mqttHandler.publishTelemetry(lastTelemetryData)) {
                Serial.printf("Telemetry published: T=%.1fC H=%.1f%%\n",
                              lastTelemetryData.temperature_c,
                              lastTelemetryData.humidity_pct);
            } else {
                Serial.println("Failed to publish telemetry");
            }
            lastTelemetryTime = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(NETWORK_PERIOD_MS));
    }
}

static void Task_Emergency_Watchdog(void *pvParameters) {
    (void)pvParameters;

    for (;;) {
        EventBits_t bits = xEventGroupWaitBits(
            sysEvents,
            EVT_EMERGENCY | EVT_WDT_FAULT | EVT_SENSOR_TIMEOUT | EVT_MANUAL_TIMEOUT,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY);

        if (bits & EVT_EMERGENCY) {
            setSystemMode(EMERGENCY_MODE);
            xEventGroupClearBits(sysEvents, EVT_MANUAL_ACTIVE);
        } else if (bits & EVT_SENSOR_TIMEOUT) {
            setSystemMode(SAFE_MODE);
        } else if (bits & EVT_MANUAL_TIMEOUT) {
            setSystemMode(AUTO_MODE);
        }

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(EMERGENCY_REACT_MS));
    }
}