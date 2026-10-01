/**
 * Unit tests for AHT20 Driver
 */

#include <Arduino.h>
#include <AHT20.h>
#include <unity.h>

class MockTwoWire : public AHT20I2CTransport {
public:
    MockTwoWire()
                : tx_buffer_pos(0),
          rx_buffer_len(0),
                    rx_buffer_pos(0),
          status_value(0x00),
          status_ready(false),
          measurement_ready(false),
          fail_next_transmission(false),
          saw_valid_init_command(false) {
        memset(tx_buffer, 0, sizeof(tx_buffer));
        memset(rx_buffer, 0, sizeof(rx_buffer));
    }

    void beginTransmission(uint8_t address) override {
        tx_address = address;
        tx_buffer_pos = 0;
    }

    size_t write(uint8_t data) override {
        if (tx_buffer_pos < sizeof(tx_buffer)) {
            tx_buffer[tx_buffer_pos++] = data;
            return 1;
        }
        return 0;
    }

    uint8_t endTransmission() override {
        if (fail_next_transmission) {
            fail_next_transmission = false;
            tx_buffer_pos = 0;
            return 2;
        }
        if (tx_buffer_pos == 3 && tx_buffer[0] == 0xBE &&
            tx_buffer[1] == 0x08 && tx_buffer[2] == 0x00) {
            saw_valid_init_command = true;
        }
        tx_buffer_pos = 0;
        return 0;
    }

    uint8_t requestFrom(uint8_t address, uint8_t quantity) override {
        rx_address = address;
        rx_buffer_pos = 0;

        if (quantity == 1 && status_ready) {
            rx_buffer[0] = status_value;
            rx_buffer_len = 1;
            status_ready = false;
            return 1;
        }

        if (quantity == 6 && measurement_ready) {
            rx_buffer_len = 6;
            for (size_t i = 0; i < 6; ++i) {
                rx_buffer[i] = measurement_buffer[i];
            }
            measurement_ready = false;
            return 6;
        }

        rx_buffer_len = 0;
        return 0;
    }

    int available() override {
        return static_cast<int>(rx_buffer_len - rx_buffer_pos);
    }

    int read() override {
        if (rx_buffer_pos < rx_buffer_len) {
            return rx_buffer[rx_buffer_pos++];
        }
        return -1;
    }

    void simulateResetResponse() {}

    void simulateTransmissionFailure() {
        fail_next_transmission = true;
    }

    bool sawValidInitCommand() const {
        return saw_valid_init_command;
    }

    void simulateStatusResponse(uint8_t status) {
        status_value = status;
        status_ready = true;
    }

    void simulateMeasurementData(float temperature, float humidity) {
        uint32_t rawHumidity = (humidity * 0xFFFFF) / 100.0f;
        uint32_t rawTemperature = ((temperature + 50.0f) * 0xFFFFF) / 200.0f;

        measurement_buffer[0] = 0x18;
        measurement_buffer[1] = (rawHumidity >> 12) & 0xFF;
        measurement_buffer[2] = (rawHumidity >> 4) & 0xFF;
        measurement_buffer[3] = ((rawHumidity & 0x0F) << 4) | ((rawTemperature >> 16) & 0x0F);
        measurement_buffer[4] = (rawTemperature >> 8) & 0xFF;
        measurement_buffer[5] = rawTemperature & 0xFF;

        measurement_ready = true;
    }

private:
    uint8_t tx_address;
    uint8_t tx_buffer[32];
    size_t tx_buffer_pos;

    uint8_t rx_address;
    uint8_t rx_buffer[32];
    size_t rx_buffer_len;
    size_t rx_buffer_pos;

    uint8_t status_value;
    bool status_ready;

    uint8_t measurement_buffer[6];
    bool measurement_ready;
    bool fail_next_transmission;
    bool saw_valid_init_command;
};

MockTwoWire aht20_mockWire;
AHT20* sensor = nullptr;

void aht20_setUp(void) {
    sensor = new AHT20(static_cast<AHT20I2CTransport&>(aht20_mockWire), 0x38);
}

void aht20_tearDown(void) {
    delete sensor;
    sensor = nullptr;
}

void test_AHT20_constructor(void) {
    TEST_ASSERT_NOT_NULL(sensor);
}

void test_AHT20_begin_success(void) {
    aht20_mockWire.simulateResetResponse();
    aht20_mockWire.simulateStatusResponse(0x08);

    bool result = sensor->begin();
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(aht20_mockWire.sawValidInitCommand());
}

void test_AHT20_begin_failure(void) {
    aht20_mockWire.simulateResetResponse();
    aht20_mockWire.simulateTransmissionFailure();
    bool result = sensor->begin();
    TEST_ASSERT_FALSE(result);
}

void test_AHT20_triggerMeasurement(void) {
    sensor->triggerMeasurement();
    TEST_ASSERT_TRUE(true);
}

void test_AHT20_isDataReady_notPending(void) {
    TEST_ASSERT_FALSE(sensor->isDataReady());
}

void test_AHT20_isDataReady_tooSoon(void) {
    sensor->triggerMeasurement();
    TEST_ASSERT_FALSE(sensor->isDataReady());
}

void test_AHT20_readData_success(void) {
    aht20_mockWire.simulateMeasurementData(25.0f, 50.0f);
    aht20_mockWire.simulateStatusResponse(0x18);

    sensor->triggerMeasurement();
    delay(80);

    bool result = sensor->readData();
    TEST_ASSERT_TRUE(result);

    AHT20_Data_t data = sensor->getLastResult();
    TEST_ASSERT_TRUE(data.valid);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 25.0f, data.temperature);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 50.0f, data.humidity);
}

void test_AHT20_getLastResult(void) {
    AHT20_Data_t data = sensor->getLastResult();
    TEST_ASSERT_FALSE(data.valid);
}

void test_AHT20_update_noMeasurement(void) {
    TEST_ASSERT_FALSE(sensor->update());
}

void test_AHT20_checkCalibration(void) {
    aht20_mockWire.simulateStatusResponse(0x00);
    TEST_ASSERT_FALSE(sensor->checkCalibration());

    aht20_mockWire.simulateStatusResponse(0x08);
    TEST_ASSERT_TRUE(sensor->checkCalibration());
}

void run_AHT20_tests(void) {
    aht20_setUp();
    RUN_TEST(test_AHT20_constructor);
    aht20_tearDown();

    aht20_setUp();
    RUN_TEST(test_AHT20_begin_success);
    aht20_tearDown();

    aht20_setUp();
    RUN_TEST(test_AHT20_begin_failure);
    aht20_tearDown();

    aht20_setUp();
    RUN_TEST(test_AHT20_triggerMeasurement);
    aht20_tearDown();

    aht20_setUp();
    RUN_TEST(test_AHT20_isDataReady_notPending);
    aht20_tearDown();

    aht20_setUp();
    RUN_TEST(test_AHT20_isDataReady_tooSoon);
    aht20_tearDown();

    aht20_setUp();
    RUN_TEST(test_AHT20_readData_success);
    aht20_tearDown();

    aht20_setUp();
    RUN_TEST(test_AHT20_getLastResult);
    aht20_tearDown();

    aht20_setUp();
    RUN_TEST(test_AHT20_update_noMeasurement);
    aht20_tearDown();

    aht20_setUp();
    RUN_TEST(test_AHT20_checkCalibration);
    aht20_tearDown();
}