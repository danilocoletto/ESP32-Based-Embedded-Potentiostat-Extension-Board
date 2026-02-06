/*******************************************************************************
 * @file        muxes.c
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

#include "muxes.h"


int global_gain = 1;

void MAX4737_Config_Pins(void)
{
    config_pin(MAX4737_CR_SHORT_EN, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    gpio_set_level(MAX4737_CR_SHORT_EN , LOW);

    config_pin(MAX4737_RE_FB_EN, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    gpio_set_level(MAX4737_RE_FB_EN , LOW);

    config_pin(MAX4737_WE_EN, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    gpio_set_level(MAX4737_WE_EN , LOW);

    config_pin(MAX4737_CE_EN, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    gpio_set_level(MAX4737_CE_EN, LOW);

}

void MAX4617_Config_Pins(void)
{
    config_pin(MAX4617_SEL_A , GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    gpio_set_level(MAX4617_SEL_A , LOW);
    config_pin(MAX4617_SEL_B , GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    gpio_set_level(MAX4617_SEL_B , LOW);
    config_pin(MAX4617_SEL_C , GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    gpio_set_level(MAX4617_SEL_C , LOW);
	global_gain = 1;

}

void MAX4617_Set_Gain(int gain)
{
    
	switch (gain)
	{
		case 0:
			gpio_set_level(MAX4617_SEL_A, LOW);
			gpio_set_level(MAX4617_SEL_B, LOW);
			gpio_set_level(MAX4617_SEL_C, LOW);
			printf("NO GAIN SET (UNITY GAIN) \n");
			global_gain = 1;
			break;
		case 1:
			gpio_set_level(MAX4617_SEL_A, HIGH);
			gpio_set_level(MAX4617_SEL_B, LOW);
			gpio_set_level(MAX4617_SEL_C, LOW);
			printf("GAIN SET TO 100 \n");
			global_gain = 100;
			break;
		case 2:
			gpio_set_level(MAX4617_SEL_A, LOW);
			gpio_set_level(MAX4617_SEL_B, HIGH);
			gpio_set_level(MAX4617_SEL_C, LOW);
			printf("GAIN SET TO 3K \n");
			global_gain = 3000;
			break;
		case 3:
			gpio_set_level(MAX4617_SEL_A, HIGH);
			gpio_set_level(MAX4617_SEL_B, HIGH);
			gpio_set_level(MAX4617_SEL_C, LOW);
			printf("GAIN SET TO 30K \n");
			global_gain = 30000;
			break;
		case 4:
			gpio_set_level(MAX4617_SEL_A, LOW);
			gpio_set_level(MAX4617_SEL_B, LOW);
			gpio_set_level(MAX4617_SEL_C, HIGH);
			printf("GAIN SET TO 300K \n");
			global_gain = 300000;
			break;
		case 5:
			gpio_set_level(MAX4617_SEL_A, HIGH);
			gpio_set_level(MAX4617_SEL_B, LOW);
			gpio_set_level(MAX4617_SEL_C, HIGH);
			printf("GAIN SET TO 3M \n");
			global_gain = 3000000;
			break;
		case 6:
			gpio_set_level(MAX4617_SEL_A, LOW);
			gpio_set_level(MAX4617_SEL_B, HIGH);
			gpio_set_level(MAX4617_SEL_C, HIGH);
			printf("GAIN SET TO 30M \n");
			global_gain = 30000000;
			break;
		case 7:
			gpio_set_level(MAX4617_SEL_A, HIGH);
			gpio_set_level(MAX4617_SEL_B, HIGH);
			gpio_set_level(MAX4617_SEL_C, HIGH);
			printf("GAIN SET TO 100M \n");
			global_gain = 100000000;
			break;
	}
}


int MAX4617_get_gain(void)
{
	return global_gain;
}