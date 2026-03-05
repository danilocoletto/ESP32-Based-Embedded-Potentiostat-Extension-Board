idf_component_register(SRCS "experiments.c"
                      INCLUDE_DIRS "include"
		              REQUIRES esp_rom esp_timer driver GPIO ADS1255 MAX5217 MUXES GENERAL)
