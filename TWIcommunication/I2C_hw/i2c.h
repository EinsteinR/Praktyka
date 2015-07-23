#ifndef I2C_H
#define I2C_H

void i2c_init(void);

void i2c_start(void);

void i2c_stop(void);

void i2c_write(uint8_t);

uint8_t i2c_read(uint8_t);

#endif /* I2C_H*/