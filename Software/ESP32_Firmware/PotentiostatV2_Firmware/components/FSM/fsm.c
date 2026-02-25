/*******************************************************************************
 * @file        timer.c
 * @brief       C Header Library for timer control
 * @details     This file contains the functions that provide general but needed
 *              funcionlities like UART control and timer control
 * @version     1.0
 * @author      Ing. Danilo Coletto Gallego
 * @date        08.12.2025
 * @copyright   (c) 2025  Universidad Nacional del Sur - CONICET
 * @see         
******************************************************************************** 

MIT License
*/

#include "fsm.h"


/********************************************************************************/
/*                               Global Variables                               */
/********************************************************************************/

static potentiostat_fsm_t       fsm;
TaskHandle_t                    xTaskExpControlHandle = NULL;
DEPO_PRECOND_config             precond_config;
exp_config                      experiment_config; 

/********************************************************************************/
/*                 Private FSM Auxiliary Functions Prototypes                   */
/********************************************************************************/

static void       process_line_FSM     (potentiostat_fsm_t *fsm) ;


/********************************************************************************/
/*                   Auxiliary Complementary Functions for FSM                  */
/********************************************************************************/


/**
 * @brief Internal command dispatcher for the Finite State Machine (FSM).
 * * Parses the accumulated command string in the line buffer. Handles priority commands 
 * like ABORT and manages transitions between WAITING, PREPARING, and EXECUTING states 
 * based on received ASCII instructions from the host.
 * * @param fsm Pointer to the FSM instance structure.
 */
static void process_line_FSM (potentiostat_fsm_t *fsm) 
{
    char *cmd = fsm->line_buffer;

    // COMANDO PRIORITARIO: ABORT (Funciona siempre)
    if (strcmp(cmd, "ABORT") == 0) {
        transition_state_FSM(fsm, STATE_WAITING);
        // Función externa para detener hardware
        ABORT_FLAG = 1;
        // stop_all_hardware(); 
        uart_write_bytes(UART_NUM_0, "STOP_OK\n", 8);
        return;
    }

    switch (fsm->current_state) {
        case STATE_WAITING:
            if (strcmp(cmd, "READY_UP") == 0) {
                transition_state_FSM(fsm, STATE_PREPARING);
                uart_write_bytes(UART_NUM_0, "READY_ACK\n", 10);
                uart_write_bytes(UART_NUM_0, "PREPARING\n", 10);
            } else if (strcmp(cmd, "?ID") == 0) {
                const char* id = "Potenciostato V2.0\n";
                uart_write_bytes(UART_NUM_0, id, strlen(id));
            }
            break;

        case STATE_PREPARING:
            if (strncmp(cmd, "CONF_SWV:", 9) == 0) 
            {
                fsm->current_experiment = EXP_SWV; // La FSM ahora "sabe" qué ejecutará
                // Parsing manual con sscanf (C estándar)
                // Usamos punteros a tus variables globales de experimento
                int res = 11;

                sscanf(cmd + 9, "%hd, %hd, %hu, %hu, %hu, %hu, %hhd, %hhd, %hd, %hu, %hu",                                                      
                                 &experiment_config.SWV.initial_pot_mv, &experiment_config.SWV.final_pot_mv, 
                                 &experiment_config.SWV.freq_hz, &experiment_config.SWV.pulse_amplitude_mv, 
                                 &experiment_config.SWV.step_pot_mv, &experiment_config.SWV.quiet_time_s,
                                 &precond_config.precond_on_off,
                                 &precond_config.stir_on_off, &precond_config.deposition_pot_mv, &precond_config.deposition_time_s, &precond_config.quiet_time_s);

                if (res == 11) 
                {
                    uart_write_bytes(UART_NUM_0, "CONF_OK\n", 8);
                } 
                else 
                    uart_write_bytes(UART_NUM_0, "CONF_ERR\n", 9);
            }
            else if (strncmp(cmd, "CONF_LSV:", 8) == 0) 
            {
                fsm->current_experiment = EXP_LSV;
                // Parsing manual con sscanf (C estándar)
                // Usamos punteros a tus variables globales de experimento
                int res = 6;
                sscanf(cmd + 9, "%hd, %hd, %hd, %hd, %hu, %hu, %hu",                                                      
                                 &experiment_config.LSV.initial_pot_mv, &experiment_config.LSV.switching_pot1_mv,                                       // FALTA VER EL PREACONDICIONAMIENTO QUE
                                 &experiment_config.LSV.switching_pot2_mv, &experiment_config.LSV.final_pot_mv,                                         // QUE SE DEBE TENER EN CUENTA ACA
                                 &experiment_config.LSV.segments, &experiment_config.LSV.scan_rate_mv_s, &experiment_config.LSV.quiet_time_s);
                                 
                if (res == 6) 
                {
                    uart_write_bytes(UART_NUM_0, "CONF_OK\n", 8);
                } 
                else 
                    uart_write_bytes(UART_NUM_0, "CONF_ERR\n", 9);
            }
            //else if (strncmp(cmd, "CONF_CPE:", 8) == 0) 
            else if (strcmp(cmd, "START_EXP") == 0) 
            {
                uart_write_bytes(UART_NUM_0, "EXECUTING\n", 10);
                transition_state_FSM(fsm, STATE_EXECUTING);
                // Iniciar tarea de muestreo en Core 1
                xTaskNotifyGive(xTaskExpControlHandle);
            }
            break;

        case STATE_EXECUTING:
            // En ejecución ignoramos todo excepto ABORT (ya manejado arriba)
            break;
    }
}

/********************************************************************************/
/*                           FSM Execution Functions                            */
/********************************************************************************/

/**
 * @brief Initializes the FSM structure to its default state.
 * * Sets the initial state to WAITING and clears the internal UART line buffer.
 * * @param fsm Pointer to the FSM instance structure to initialize.
 */
void init_FSM (potentiostat_fsm_t *fsm) 
{
    fsm->current_state = STATE_WAITING;
    fsm->line_idx = 0;
    memset(fsm->line_buffer, 0, sizeof(fsm->line_buffer));
}


/**
 * @brief Updates the current state of the FSM.
 * * Manages the transition of the system state and provides a hook for 
 * notifying external interfaces (like a Python host) about the change.
 * * @param fsm Pointer to the FSM instance structure.
 * @param new_state The new state to transition into.
 */
void transition_state_FSM (potentiostat_fsm_t *fsm, system_state_t new_state) 
{
    fsm->current_state = new_state;
    // Opcional: Notificar a Python el cambio de estado
    // uart_write_bytes(UART_NUM_0, "EVENT:STATE_CHANGE\n", 19);
}

/**
 * @brief Processes individual characters received via UART for the FSM.
 * * Accumulates characters into a line buffer. Upon receiving a termination 
 * character ('\n' or '\r'), it triggers the line processing logic.
 * * @param fsm Pointer to the FSM instance structure.
 * @param c The character received from the UART.
 */
void process_char_FSM (potentiostat_fsm_t *fsm, char c) 
{
    if (c == '\n' || c == '\r') {
        if (fsm->line_idx > 0) {
            fsm->line_buffer[fsm->line_idx] = '\0';
            process_line_FSM(fsm);
            fsm->line_idx = 0;
        }
    } else if (fsm->line_idx < sizeof(fsm->line_buffer) - 1) {
        fsm->line_buffer[fsm->line_idx++] = c;
    }
}


/********************************************************************************/
/*                         Functions to Acces Variables                         */
/********************************************************************************/

/**
 * @brief Provides access to the global FSM instance.
 * * @return potentiostat_fsm_t* Pointer to the static FSM structure.
 */
potentiostat_fsm_t* get_fsm_pointer (void)
{
    return &fsm;
}

/**
 * @brief Provides access to the experiment control task handle.
 * * Useful for task notifications and synchronization between cores.
 * * @return TaskHandle_t* Pointer to the experiment control task handle.
 */
TaskHandle_t* get_ExpControlTask_pointer (void)
{
    return &xTaskExpControlHandle;
}


exp_config* get_exp_config_pointer(void)
{
    return &experiment_config;
}