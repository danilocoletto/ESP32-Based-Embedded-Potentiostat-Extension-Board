/*******************************************************************************
 * @file        muxes.c
 * @brief       C Library to control  Digital Muxes MAX4737EUD and MAX4617CSE+
 * @details     This file implements the functionalities of the DAC.
 * @version     1.0
 * @author      Ing. Danilo Coletto Gallego
 * @date        11.12.2025
 * @copyright   (c) 2025  Universidad Nacional del Sur - CONICET
 * @see         
******************************************************************************** 

MIT License
*/

#include "muxes.h"


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
    gpio_set_level(MAX4617_SEL_B , HIGH);
    config_pin(MAX4617_SEL_C , GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    gpio_set_level(MAX4617_SEL_C , HIGH);

}

void MAX4617_Set_Gain(int gain)
{
    
	switch (gain)
	{
		case 0:
			gpio_set_level(MAX4617_SEL_A, LOW);
			gpio_set_level(MAX4617_SEL_B, LOW);
			gpio_set_level(MAX4617_SEL_C, LOW);
			printf("NO GAIN SET (GAIN 0) \n");
			break;
		case 1:
			gpio_set_level(MAX4617_SEL_A, HIGH);
			gpio_set_level(MAX4617_SEL_B, LOW);
			gpio_set_level(MAX4617_SEL_C, LOW);
			printf("GAIN SET TO 100 \n");
			break;
		case 2:
			gpio_set_level(MAX4617_SEL_A, LOW);
			gpio_set_level(MAX4617_SEL_B, HIGH);
			gpio_set_level(MAX4617_SEL_C, LOW);
			printf("GAIN SET TO 3K \n");
			break;
		case 3:
			gpio_set_level(MAX4617_SEL_A, HIGH);
			gpio_set_level(MAX4617_SEL_B, HIGH);
			gpio_set_level(MAX4617_SEL_C, LOW);
			printf("GAIN SET TO 30K \n");
			break;
		case 4:
			gpio_set_level(MAX4617_SEL_A, LOW);
			gpio_set_level(MAX4617_SEL_B, LOW);
			gpio_set_level(MAX4617_SEL_C, HIGH);
			printf("GAIN SET TO 300K \n");
			break;
		case 5:
			gpio_set_level(MAX4617_SEL_A, HIGH);
			gpio_set_level(MAX4617_SEL_B, LOW);
			gpio_set_level(MAX4617_SEL_C, HIGH);
			printf("GAIN SET TO 3M \n");
			break;
		case 6:
			gpio_set_level(MAX4617_SEL_A, LOW);
			gpio_set_level(MAX4617_SEL_B, HIGH);
			gpio_set_level(MAX4617_SEL_C, HIGH);
			printf("GAIN SET TO 30M \n");
			break;
		case 7:
			gpio_set_level(MAX4617_SEL_A, HIGH);
			gpio_set_level(MAX4617_SEL_B, HIGH);
			gpio_set_level(MAX4617_SEL_C, HIGH);
			printf("GAIN SET TO 100M \n");
			break;
	}
}