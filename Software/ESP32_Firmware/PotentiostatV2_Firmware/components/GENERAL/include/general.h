

#ifndef GENERAL_H
#define GENERAL_H

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



/* --- COMANDOS RECIBIDOS (Entrada) --- */
#define CMD_ABORT               "ABORT"
#define CMD_READY_UP            "READY_UP"
#define CMD_GET_ID              "?ID"
#define CMD_RESET               "RST"
#define CMD_E_STATUS            "E_STATUS"
#define CMD_CONF_SWV            "CONF_SWV:"
#define CMD_CONF_LSV            "CONF_LSV:"
#define CMD_START_EXP           "START_EXP"
#define CMD_CONF_GAIN           "SET_GAIN:"

/* --- RESPUESTAS ENVIADAS (Salida) --- */
#define MSG_STOP_OK             "STOP_OK\n"
#define MSG_READY_ACK           "READY_ACK\n"
#define MSG_PREPARING           "PREPARING\n"
#define MSG_EXECUTING           "EXECUTING\n"
#define MSG_CONF_OK             "CONF_OK\n"
#define MSG_CONF_GAIN_OK        "CONF_GAIN_OK\n"
#define MSG_CONF_ERR            "CONF_ERR\n"
#define MSG_RESETTING           "RESETTING\n"
#define MSG_WAITING             "WAITING\n"
#define MSG_FINISHED            "FINISHED\n"
#define MSG_CELL_CONNECTED      "CELL1\n"
#define MSG_CELL_DISCONNECTED   "CELL0\n"
#define MSG_ID_STR              "POTENTIOSTAT_V2.0\n"
#define MSG_FIRMWARE            "FIRM_V1.0\n"


// --- CONFIGURACIÓN UART ---
#define UART_PORT_NUM      UART_NUM_0
#define UART_BAUD_RATE     921600
#define BUF_SIZE           2048
#define UART_RX_BUFFER_SIZE 32 // Tamaño máximo de la línea de comando (ej: "10\n")

extern volatile bool ABORT_FLAG;

void        init_UART       (void);

#endif