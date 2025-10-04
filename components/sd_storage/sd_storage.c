/**
 * @file sd_storage.c
 */
#include "sd_storage.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "log_router.h"

static sdmmc_card_t *card;

static const char *sd_tag = "SD";
static const char *mount_dir = "/sd";
static int log_index = 0;
static char log_dir[32];

#define PIN_MISO 	GPIO_NUM_20
#define PIN_MOSI 	GPIO_NUM_18
#define PIN_CLK		GPIO_NUM_19
#define PIN_CS		GPIO_NUM_17

esp_err_t create_log_directory_index(void) {
	DIR *dir;
	struct dirent *ent;
	int max_index = -1;

	dir = opendir(mount_dir);
	if (dir != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			if (strncasecmp(ent->d_name, "LOG", 3) == 0) {
				int idx = atoi(ent->d_name + 3);
				if (idx > max_index) {
					max_index = idx;
				}
			}
		}
		closedir(dir);
	} else {
		ESP_LOGE(sd_tag, "Failed to open %s", mount_dir);
		return ESP_FAIL;
	}

	log_index = max_index + 1;
	ESP_LOGI(sd_tag, "log_index: %d", log_index);
	snprintf(log_dir, sizeof(log_dir), "%s/LOG%d", mount_dir, log_index);

	int error = mkdir(log_dir, 0777);
	if (error == 0) {
		ESP_LOGI(sd_tag, "Created directory: %s", log_dir);
	} else {
		ESP_LOGE(sd_tag, "Failed to create directory: %s, %d", log_dir, error);
		return ESP_FAIL;
	}

	return ESP_OK;
}

esp_err_t sd_init(void) {
	esp_err_t ret;

	sdmmc_host_t host = SDSPI_HOST_DEFAULT();

	spi_bus_config_t bus_cfg = {
		.mosi_io_num = PIN_MOSI,
		.miso_io_num = PIN_MISO,
		.sclk_io_num = PIN_CLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 4096,
	};

	ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
	if (ret != ESP_OK) {
		ESP_LOGE(sd_tag, "Initializing SD card");
		return ret;
	}

	sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
	slot_config.gpio_cs = PIN_CS;
	slot_config.host_id = host.slot;

	esp_vfs_fat_mount_config_t mount_config = {
		.format_if_mount_failed = true,
		.max_files = 5,
		.allocation_unit_size = 16 * 1024
	};

	ret = esp_vfs_fat_sdspi_mount(mount_dir, &host, &slot_config, &mount_config, &card);
	if (ret != ESP_OK) {
		ESP_LOGE(sd_tag, "Failed to mount filesystem");
		return ret;
	}

	ret = create_log_directory_index();
	if (ret != ESP_OK) {
		ESP_LOGE(sd_tag, "Failed to create directory");
		return ret;
	}

	char log_path[64];
	snprintf(log_path, sizeof(log_path), "%s/log.txt", log_dir);

	ret = esp_log_router_to_file(log_path, NULL, ESP_LOG_INFO);
	if (ret != ESP_OK) {
		ESP_LOGE(sd_tag, "Failed to init log_router with file: %s", log_path);
		return ret;
	}

	ESP_LOGI(sd_tag, "Logging redirected to %s", log_path);

	return ret;
}

esp_err_t sd_write(const char *filename, const uint8_t *data, size_t len) {
	fflush(NULL);
	char path[64];
	snprintf(path, sizeof(path), "%s/%s", log_dir, filename);
	FILE *f = fopen(path, "wb");
	if (!f) {
		ESP_LOGE(sd_tag, "Failed to open file for writing: %s", path);
		return ESP_FAIL;
	}
	size_t written = fwrite(data, 1, len, f);
	fclose(f);

	if (written != len) {
		ESP_LOGE(sd_tag, "Write incomplete: %s", path);
		return ESP_FAIL;
	}
	return ESP_OK;
}

esp_err_t sd_read(const char *filename, uint8_t *buf, size_t buf_len, size_t *read_len) {
	char path[64];
	snprintf(path, sizeof(path), "%s/%s", log_dir, filename);
	FILE *f = fopen(path, "rb");
	if (!f) {
		ESP_LOGE(sd_tag, "Failed to open file for reading: %s", path);
		return ESP_FAIL;
	}
	*read_len = fread(buf, 1, buf_len, f);
	fclose(f);
	return ESP_OK;
}