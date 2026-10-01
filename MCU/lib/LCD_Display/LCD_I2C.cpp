#include "LCD_I2C.h"

#include <Wire.h>
#include "../../include/config.h"

namespace {
uint8_t lcdAddress = LCD_I2C_ADDR;

void lcdWrite(uint8_t value) {
    Wire.beginTransmission(lcdAddress);
    Wire.write(value);
    Wire.endTransmission();
}

void lcdPulseEnable(uint8_t data) {
    lcdWrite(data | 0x04);
    delayMicroseconds(1);
    lcdWrite(data & (uint8_t)~0x04);
    delayMicroseconds(100);
}

void lcdSendNibble(uint8_t nibble, bool rs) {
    const uint8_t data = (nibble << 4) | (rs ? 0x01 : 0x00) | 0x08;
    lcdWrite(data);
    lcdPulseEnable(data);
}

void lcdSendByte(uint8_t byteVal, bool rs) {
    lcdSendNibble(byteVal >> 4, rs);
    lcdSendNibble(byteVal & 0x0F, rs);
}
}

void lcdInit(uint8_t addr) {
    lcdAddress = addr;
    delay(50);
    lcdWrite(0x38);
    delayMicroseconds(4500);
    lcdPulseEnable(0x38);
    lcdWrite(0x38);
    delayMicroseconds(4500);
    lcdPulseEnable(0x38);
    lcdWrite(0x38);
    delayMicroseconds(150);
    lcdPulseEnable(0x38);
    lcdWrite(0x28);
    delayMicroseconds(150);
    lcdPulseEnable(0x28);
    lcdSendByte(0x28, false);
    lcdSendByte(0x0C, false);
    lcdSendByte(0x06, false);
    lcdClear();
}

void lcdBacklight(bool on) {
    Wire.beginTransmission(lcdAddress);
    Wire.write(on ? 0x08 : 0x00);
    Wire.endTransmission();
}

void lcdClear() {
    lcdSendByte(0x01, false);
    delayMicroseconds(2000);
}

void lcdSetCursor(uint8_t col, uint8_t row) {
    static const uint8_t rowOffset[2] = {0x00, 0x40};
    if (row > 1) row = 1;
    lcdSendByte(0x80 | (col + rowOffset[row]), false);
}

void lcdPrint(const char *str) {
    while (*str) {
        lcdSendByte((uint8_t)*str++, true);
    }
}