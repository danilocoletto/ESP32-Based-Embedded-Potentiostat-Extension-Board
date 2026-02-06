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

#include "esp_timer.h"
#include "esp_log.h"
#include "driver/uart.h"


// --- CONFIGURACIÓN UART ---
#define UART_PORT_NUM      UART_NUM_0
#define UART_BAUD_RATE     115200
#define BUF_SIZE           1024
#define UART_RX_BUFFER_SIZE 32 // Tamaño máximo de la línea de comando (ej: "10\n")




// Prototype for the callback function
static void  ADC_DAC_Sync_Timer_callback  (void* arg);
void         Sync_Timer_init              (void);
void         Sync_Timer_configure         (uint64_t period_us);
void         Sync_Timer_stop              (void);


void init_uart(void);
