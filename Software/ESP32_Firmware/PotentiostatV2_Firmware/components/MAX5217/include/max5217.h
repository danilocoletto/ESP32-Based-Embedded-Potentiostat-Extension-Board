/*******************************************************************************
 * @file        max5217.h
 * @brief       C Header Library for MAX5217 Digital to Analog Converter
 * @details     This file implements the functionalities of the ADC.
 * @version     1.0
 * @author      Ing. Danilo Coletto Gallego
 * @date        04.12.2025
 * @copyright   (c) 2025  Universidad Nacional del Sur - CONICET
 * @see         
******************************************************************************** 
 */

#ifndef MAX5217_H
#define	MAX5217_H

#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "gpio_config.h"
#include "freertos/FreeRTOS.h"


// Definiciones de pines (Ajústalo a tu placa)
#define MAX_5217_NAUX             GPIO_NUM_32
#define MAX_5217_SCL              GPIO_NUM_22
#define MAX_5217_SDA              GPIO_NUM_21
#define I2C_MASTER_FREQ_HZ          10000    // 100kHz o 400kHz

// DATOS DEL DAC
#define DAC1_DEVICE_ID 	            0x1C
#define DAC_REF_5V 		            5000
#define DAC_REF_2_5V 	            2500
#define MAX_VALUE_DAC	            65500


// COMANDOS DEL DAC
#define MAX5217_CMD_NO_OP 			0x00
#define MAX5217_CMD_CODE_LOAD 		0x01
#define MAX5217_CMD_CODE			0x02
#define MAX5217_CMD_LOAD			0x03
#define MAX5217_CMD_USER_CONFIG 	0x08
#define MAX5217_CMD_SW_RESET 		0x09
#define MAX5217_CMD_SW_CLEAR		0x10

void i2c_scanner(i2c_master_bus_handle_t bus_handle);
i2c_master_dev_handle_t setup_DAC(uint8_t devID);
void write_DAC(i2c_master_dev_handle_t dac_handle, double mvolt_value);
void clear_DAC(i2c_master_dev_handle_t dac_handle);


#endif	/* MAX5217_H */