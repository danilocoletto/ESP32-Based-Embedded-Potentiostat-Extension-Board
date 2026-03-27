

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
#include "esp_attr.h"
#include "driver/uart.h"
#include "driver/gpio.h"




/* --- COMANDOS RECIBIDOS (Entrada) --- */
#define CMD_ABORT               "ABORT"
#define CMD_READY_UP            "READY_UP"
#define CMD_GET_ID              "?ID"
#define CMD_GET_STATE           "?STATE"
#define CMD_RESET               "RST"
#define CMD_E_STATUS            "E_STATUS"
#define CMD_CONF_SWV            "CONF_SWV:"
#define CMD_CONF_LSV            "CONF_LSV:"
#define CMD_CONF_DPV            "CONF_DPV:"
#define CMD_CONF_CPE            "CONF_CPE:"
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
#define MSG_FIRMWARE            "FIRM_V1.1\n"


// --- CONFIGURACIÓN UART ---
#define UART_PORT_NUM           UART_NUM_0
#define UART_BAUD_RATE          921600
#define BUF_SIZE                2048
#define UART_RX_BUFFER_SIZE     32 // Tamaño máximo de la línea de comando (ej: "10\n")

//  --- GPIO DEFINITIONS ---
#define HIGH                    1
#define LOW                     0

extern volatile bool            ABORT_FLAG;

/*
typedef struct {
    gpio_num_t pin;
    gpio_mode_t pin_mode;
    gpio_pullup_t pull_up;
    gpio_pulldown_t pull_down;
    gpio_int_type_t intr_type;
} GPIO_TypeDef;*/

void        init_UART       (void);
void        config_pin      (gpio_num_t pin, gpio_mode_t pin_mode, gpio_pullup_t pull_up, gpio_pulldown_t pull_down, gpio_int_type_t intr_type);
//void config_pin(GPIO_TypeDef* pin);

#endif