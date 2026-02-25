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

#include <math.h>
// Own Libraries
#include "gpio_config.h"
#include "ads1255.h"
#include "max5217.h"
#include "muxes.h"
#include "general.h"


#define     ON          1
#define     OFF         0

#define ESP_CPU_FREQ_HZ  ((double)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000.0)



// Struct "Object"
typedef struct {
    uint8_t precond_on_off;
    uint8_t stir_on_off;
    int16_t deposition_pot_mv;
    uint16_t deposition_time_s;
    uint16_t quiet_time_s;
} DEPO_PRECOND_config;

typedef struct {
    int16_t initial_pot_mv;
    int16_t final_pot_mv;
    uint16_t freq_hz;
    uint16_t step_pot_mv;
    uint16_t pulse_amplitude_mv;
    uint16_t quiet_time_s;
} SWV_config;

typedef struct {
    int16_t initial_pot_mv;
    int16_t switching_pot1_mv;
    int16_t switching_pot2_mv;
    int16_t final_pot_mv;
    uint16_t segments;
    uint16_t scan_rate_mv_s;
    uint16_t quiet_time_s;
} LSV_CV_config;


typedef struct {
    int16_t applied_pot_mv;
    uint16_t sample_int_s;
    uint16_t time_limit_s;
} PA_config;

typedef struct {
    int16_t applied_pot_mv;
    uint16_t sample_int_s;
    uint16_t time_limit_s;
} CPE_config;

typedef union {
    SWV_config SWV;
    LSV_CV_config LSV;
    PA_config PA;
    CPE_config CPE;
} exp_config;


typedef struct __attribute__((packed)) {
    int16_t lastindex;
    float forward;
    float reverse;
} swv_packet_t;

// Nueva estructura específica para LSV/CV con resolución decimal
typedef struct __attribute__((packed)) {
    float voltage_index_mv;
    float voltage_meas;
} lsv_cv_packet_t;

bool        get_Electrodes_State        (void);
bool        get_Agitator_State          (void);
uint8_t     execute_DEPO_PRECOND        (DEPO_PRECOND_config *config);
uint8_t     execute_SWV_experiment      (SWV_config *config);
uint8_t     execute_LSV_CV_experiment   (LSV_CV_config *config);
uint8_t     execute_PA_experiment       (PA_config *config);
uint8_t     execute_CPE_experiment      (CPE_config *config);