idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES MAX5217 ADS1255 GPIO MUXES)