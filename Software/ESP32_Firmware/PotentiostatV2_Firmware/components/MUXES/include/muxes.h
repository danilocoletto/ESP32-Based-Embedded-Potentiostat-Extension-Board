/*******************************************************************************
 * @file        muxes.h
 * @brief       C Library to control  Digital Muxes MAX4737EUD and MAX4617CSE+
 * @details     This file implements the functionalities of the Muxes
 * @version     1.0
 * @author      Ing. Danilo Coletto Gallego
 * @date        11.12.2025
 * @copyright   (c) 2025  Universidad Nacional del Sur - CONICET
 * @see         
******************************************************************************** 

MIT License
*/

#include "gpio_config.h"

#define     CONNECTED           1
#define     DISCONNECTED        0

// --- PINOUT MAX4617 FOR ESP32 ---

#define MAX4617_SEL_A       GPIO_NUM_27
#define MAX4617_SEL_B       GPIO_NUM_26
#define MAX4617_SEL_C       GPIO_NUM_25

// --- PINOUT MAX4737 FOR ESP32 ---

#define MAX4737_CR_SHORT_EN     GPIO_NUM_14
#define MAX4737_RE_FB_EN        GPIO_NUM_33
#define MAX4737_WE_EN           GPIO_NUM_13
#define MAX4737_CE_EN           GPIO_NUM_15

// --- GAIN VALUES ---

#define NO_GAIN				    0
#define GAIN1_100			    1
#define GAIN2_3K			    2
#define GAIN3_30K			    3
#define GAIN4_300K			    4
#define GAIN5_3M			    5
#define GAIN6_30M			    6
#define GAIN7_100M			    7


void MAX4617_Config_Pins    (void);
void MAX4617_Set_Gain       (int gain);
int MAX4617_get_gain        (void);


void MAX4737_Config_Pins    (void);