/*******************************************************************************
 * @file        gpio_config.c
 * @brief       C Library for General Purpose GPIOs Functions for ESP32
 * @details     This file implements the functionalities of the ADC.
 * @version     1.0
 * @author      Ing. Danilo Coletto Gallego
 * @date        04.12.2025
 * @copyright   (c) 2025  Universidad Nacional del Sur - CONICET
 * @see         
******************************************************************************** 
 */

#include "gpio_config.h"

/*****************************************************************************************/
/*                                     GPIO FUNCTIONS                                    */
/*                                                                                       */
/*****************************************************************************************/


/**
 * @brief Configures a general-purpose GPIO pin with custom mode and interrupt settings.
 *
 * This function wraps the ESP-IDF `gpio_config()` API to configure a specified GPIO pin
 * with desired direction, pull-up/pull-down settings, and interrupt trigger type.
 *
 * @param pin        GPIO number to configure (e.g., GPIO_NUM_5).
 * @param pin_mode   Direction mode for the pin (input, output, or both).
 * @param pull_up    Whether to enable internal pull-up resistor (`GPIO_PULLUP_ENABLE` or `DISABLE`).
 * @param pull_down  Whether to enable internal pull-down resistor (`GPIO_PULLDOWN_ENABLE` or `DISABLE`).
 * @param intr_type  Interrupt type for the pin (e.g., `GPIO_INTR_POSEDGE`, `GPIO_INTR_DISABLE`).
 *
 * @note This function does not check if the GPIO number is valid. You may add validation in the future.
 */
void config_pin(gpio_num_t pin, gpio_mode_t pin_mode, gpio_pullup_t pull_up, gpio_pulldown_t pull_down, gpio_int_type_t intr_type)
{
  
    //Future Feature - GPIO VALID CHECK
    /*
    GPIO_CHECK(GPIO_IS_VALID_GPIO(pin), "GPIO number error", ESP_ERR_INVALID_ARG);
  
    if ((GPIO_IS_VALID_OUTPUT_GPIO(pin) != true) && (pin_mode & GPIO_MODE_DEF_OUTPUT)) {
        ESP_LOGE("CONFIG_PIN", "GPIO_num=%d can only be input", pin);
        ESP_ERROR_CHECK(ESP_ERR_INVALID_ARG);
    }*/
  
    gpio_config_t gpio_pin_setup = {
      .pin_bit_mask = (1ULL << pin),      // Bitmask of pins
      .mode = pin_mode,                   // Both input and output
      .pull_up_en = pull_up,              // Enable pull-up
      .pull_down_en = pull_down,          // Disable pull-down
      .intr_type = intr_type              // Disable interrupts
    };
  
    // Apply configuration
    ESP_ERROR_CHECK(gpio_config(&gpio_pin_setup));
}