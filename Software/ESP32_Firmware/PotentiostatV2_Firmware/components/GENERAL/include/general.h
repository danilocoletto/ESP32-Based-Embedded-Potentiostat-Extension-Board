#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//#include "esp_attr.h"
//#include "esp_rom_sys.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "driver/uart.h"


// --- CONFIGURACIÓN UART ---
#define UART_PORT_NUM      UART_NUM_0
#define UART_BAUD_RATE     921600
#define BUF_SIZE           2048
#define UART_RX_BUFFER_SIZE 32 // Tamaño máximo de la línea de comando (ej: "10\n")

extern volatile bool ABORT_FLAG;

void        init_UART       (void);