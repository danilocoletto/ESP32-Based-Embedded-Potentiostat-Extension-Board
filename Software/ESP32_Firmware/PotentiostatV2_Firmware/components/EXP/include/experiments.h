/*******************************************************************************
 * @file        experiments.h
 * @brief       C Header file of Experiments Library
 * @details     This file contains the functions prototypes, definitions and
 *              structs that executes the different types of experiments.
 * @version     1.0
 * @author      Ing. Danilo Coletto Gallego
 * @date        08.12.2025
 * @copyright   (c) 2025  Universidad Nacional del Sur - CONICET
 * @see         
******************************************************************************** 

MIT License
*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_rom_sys.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "sdkconfig.h" 

// Own Libraries
#include "gpio_config.h"
#include "ads1255.h"
#include "max5217.h"
#include "muxes.h"


#define ESP_CPU_FREQ_HZ  ((double)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000.0)


// Struct "Object"
typedef struct {
    int16_t deposition_pot_mv;
    uint16_t deposition_time_s;
} PRECOND_config;

typedef struct {
    int16_t initial_pot_mv;
    int16_t final_pot_mv;
    uint16_t freq_hz;
    int16_t step_pot_mv;
    int16_t pulse_amplitude_mv;
    uint16_t quiet_time_s;
} SWV_config;

typedef struct {
    int16_t initial_pot_mv;
    int16_t switching_pot1;
    int16_t switching_pot2;
    int16_t final_pot_mv;
    uint16_t segments;
    uint16_t scan_rate;
    uint16_t quiet_time_s;
} CV_config;


typedef struct {
    int16_t applied_pot_mv;
    uint16_t sample_int_s;
    uint16_t time_limit_s;
} PA_config;


typedef struct __attribute__((packed)) {
    int16_t lastindex;
    float forward;
    float reverse;
} swv_packet_t;


void execute_PRECOND_Treatment(PRECOND_config *config);
uint8_t execute_SWV_experiment(SWV_config *config);
void execute_CV_experiment(CV_config *config);
void execute_PA_experiment(PA_config *config);