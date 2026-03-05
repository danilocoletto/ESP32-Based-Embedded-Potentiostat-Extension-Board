/*******************************************************************************
 * @file        general.h
 * @brief       C Header Library for general purpose funcionalities
 * @details     This file contains the functions prototypes that provide general but needed
 *              funcionlities like UART control and timer control
 * @version     1.0
 * @author      Ing. Danilo Coletto Gallego
 * @date        08.12.2025
 * @copyright   (c) 2025  Universidad Nacional del Sur - CONICET
 * @see         
******************************************************************************** 

MIT License
*/

#ifndef POTENTIOSTAT_FSM_H
#define POTENTIOSTAT_FSM_H

#include "general.h"
#include "experiments.h"
#include "muxes.h"


typedef enum {
    STATE_WAITING,    // Estado inicial por defecto
    STATE_PREPARING,  // Esperando configuración
    STATE_EXECUTING   // Ejecutando experimento
} system_state_t;

typedef enum {
    EXP_NONE = 0,
    EXP_SWV,
    EXP_LSV,
    EXP_CPE
} experiment_type_t;

typedef struct {
    system_state_t current_state;
    volatile experiment_type_t current_experiment; // <--- El "Contexto"
    char line_buffer[512];
    int line_idx;
} potentiostat_fsm_t;


// Functions Prototypes
void                    init_FSM                    (potentiostat_fsm_t *fsm);
void                    process_char_FSM            (potentiostat_fsm_t *fsm, char c);
void                    transition_state_FSM        (potentiostat_fsm_t *fsm, system_state_t new_state);
potentiostat_fsm_t*     get_fsm_pointer             (void);
TaskHandle_t*           get_ExpControlTask_pointer  (void);
exp_config*             get_exp_config_pointer      (void);
DEPO_PRECOND_config*    get_precond_config_pointer  (void);



#endif /*POTENTIOSTAT_FSM_H*/