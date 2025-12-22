idf_component_register(SRCS "experiments.c" "ads1255.c"
                      INCLUDE_DIRS "include"
		              REQUIRES esp_rom driver GPIO)
