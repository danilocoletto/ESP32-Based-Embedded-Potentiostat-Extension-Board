/*******************************************************************************
 * @file        gpio_config.h
 * @brief       C Header Library for General Purpose GPIOs Functions for ESP32
 * @details     This file implements the functionalities of the ADC.
 * @version     1.0
 * @author      Ing. Danilo Coletto Gallego
 * @date        04.12.2025
 * @copyright   (c) 2025  Universidad Nacional del Sur - CONICET
 * @see         
******************************************************************************** 
 */
#ifndef GPIO_CONFIG_H
#define GPIO_CONFIG_H

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_attr.h"

#define HIGH 1
#define LOW  0


/*
typedef struct {
    gpio_num_t pin;
    gpio_mode_t pin_mode;
    gpio_pullup_t pull_up;
    gpio_pulldown_t pull_down;
    gpio_int_type_t intr_type;
} GPIO_TypeDef;*/


void config_pin(gpio_num_t pin, gpio_mode_t pin_mode, gpio_pullup_t pull_up, gpio_pulldown_t pull_down, gpio_int_type_t intr_type);
//void config_pin(GPIO_TypeDef* pin);

#endif // GPIO_CONFIG_H