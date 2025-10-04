/**
 * @file sd_storage.h
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdio.h>

esp_err_t sd_init(void);

esp_err_t sd_write(const char *filename, const uint8_t *data, size_t len);

esp_err_t sd_read(const char *filename, uint8_t *buf, size_t buf_len, size_t *read_len);