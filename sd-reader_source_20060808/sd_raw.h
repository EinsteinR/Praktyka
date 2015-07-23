
#ifndef SD_RAW_H
#define SD_RAW_H

#include <stdint.h>
#include "sd_raw_config.h"

/**
 * \addtogroup sd_raw
 *
 * @{
 */
/**
 * \file
 * MMC/SD raw access header.
 *
 * \author Roland Riegel
 */

typedef uint8_t (*sd_raw_interval_handler)(uint8_t* buffer, uint32_t offset, void* p);

uint8_t sd_raw_init();
uint8_t sd_raw_available();
uint8_t sd_raw_locked();

uint8_t sd_raw_read(uint32_t offset, uint8_t* buffer, uint16_t length);
uint8_t sd_raw_read_interval(uint32_t offset, uint8_t* buffer, uint16_t interval, uint16_t length, sd_raw_interval_handler callback, void* p);
uint8_t sd_raw_write(uint32_t offset, const uint8_t* buffer, uint16_t length);
uint8_t sd_raw_sync();

/**
 * @}
 */

#endif

