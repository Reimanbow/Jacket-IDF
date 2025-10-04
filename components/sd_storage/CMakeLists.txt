idf_component_register(SRCS "sd_storage.c"
					INCLUDE_DIRS "."
					PRIV_REQUIRES fatfs sdmmc espressif__log_router)