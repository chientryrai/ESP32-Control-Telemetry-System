# LCD Display

PCF8574 and HD44780 4-bit LCD driver used by the firmware display task. It uses
the shared I2C bus and address configured by `LCD_I2C_ADDR` in `config.h`.

The driver is adapted from `ESP32-Peripheral-Test-Rig/include/lcd_i2c.h`.
Callers must serialize LCD and sensor transactions on the shared I2C bus.