#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <Arduino.h>

void lcdInit(uint8_t addr);
void lcdBacklight(bool on);
void lcdClear();
void lcdSetCursor(uint8_t col, uint8_t row);
void lcdPrint(const char *str);

#endif