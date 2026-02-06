/*******************************************************************************
 * @file        experiments.c
 * @brief       C file of Experiments Library
 * @details     This file contains the functions that executes the different 
 *              types of experiments.
 * @version     1.0
 * @author      Ing. Danilo Coletto Gallego
 * @date        08.12.2025
 * @copyright   (c) 2025  Universidad Nacional del Sur - CONICET
 * @see         
******************************************************************************** 

MIT License
*/

#include "experiments.h"


/********************************************************************************/
/*              Auxiliary Complementary Functions for Experiments               */
/********************************************************************************/

/**
 * @brief Connects measurement cell to rest of circuit before the start of an experiment.
 *  
 */
void VOLT_EXP_START (void)
{
    gpio_set_level(MAX4737_RE_FB_EN, HIGH);
    gpio_set_level(MAX4737_WE_EN, HIGH);

    esp_rom_delay_us(100000); // Make sure WE circuit is connected before control voltage applied
    gpio_set_level(MAX4737_CE_EN, HIGH);
}

/**
* @brief Disconnects measurement cell.
*/
void VOLT_EXP_STOP (void)
{
    gpio_set_level(MAX4737_RE_FB_EN, LOW);
    gpio_set_level(MAX4737_CE_EN, LOW);
    
    esp_rom_delay_us(100000);
    // Make sure WE is last to disconnect
	gpio_set_level(MAX4737_WE_EN, LOW);
}

/********************************************************************************/
/*                       Experiment Execution Functions                         */
/********************************************************************************/

void execute_PRECOND_Treatment(PRECOND_config *config)
{

}

/**
 * @brief Executes a Square Wave Voltammetry (SWV) experiment.
 *
 * This function controls the DAC and ADC to generate a staircase waveform with 
 * superimposed square wave pulses. It synchronizes ADC readings to sample current 
 * exactly at the end of each forward and reverse pulse (One-Shot method).
 *
 * @param[in] config Pointer to the configuration structure containing experiment parameters:
 * - \b initial_pot_mv: Start potential in millivolts.
 * - \b final_pot_mv:   Stop potential in millivolts.
 * - \b freq_hz:        Frequency of the square wave (Hz). Determines pulse width.
 * - \b step_pot_mv:    Height of the staircase step (mV).
 * - \b pulse_amplitude_mv:   Amplitude of the square pulse (mV).
 * - \b quiet_time_s:   Equilibration time before the scan starts (seconds).
 *
 * @return void
 */
uint8_t execute_SWV_experiment(SWV_config *config)
{
    uint8_t direction = 1;
    uint32_t prop_delay_us = 0;
	int16_t dacindex = config->initial_pot_mv; 
    // Definimos una cabecera de 2 bytes
    const uint8_t header[2] = {0xAA, 0xBB};


	float forward = 0;
    float reverse = 0;
    int16_t lastindex = 0;

	swv_packet_t data;

	
	if (config->initial_pot_mv < config->final_pot_mv)
		direction = 1;
	else
		direction = 0;
	
	uint32_t half_cycle_period_us = 1000000UL / ((uint32_t)(config->freq_hz) * 2); //compensate for half-period triggers
	uint32_t adc_trigger_wait_us = 0;

    prop_delay_us = ADS125X_Get_Prop_Delay_Us();

	if (half_cycle_period_us > prop_delay_us) 
	{
        adc_trigger_wait_us = half_cycle_period_us - prop_delay_us;
    } 
	else 
	{
        // ERROR: La frecuencia es demasiado alta para el tiempo de asentamiento del ADC.
        // Opción A: Clavar a 0 (despertar inmediatamente y rezar)
        adc_trigger_wait_us = 0; 
        // Opción B: Retornar error (Recomendado para robustez). Aunque este chequeo deberia realizarse quiza antes
        // printf("Error: Frecuencia demasiado alta\n"); return -1;
    }

	ADS125X_WAKEUP_HAL();
    ADS125X_STANDBY_HAL();

	VOLT_EXP_START();


	// --- INICIO DE BUCLE DE BARRIDO ---

    // Configuración inicial del DAC
    if (direction == 1)
        MAX5217_DAC_WRITE_HAL_MV(dacindex + config->pulse_amplitude_mv);
    else
        MAX5217_DAC_WRITE_HAL_MV(dacindex - config->pulse_amplitude_mv);

    // Condición de parada del barrido
    while ((direction == 1 && dacindex <= config->final_pot_mv) || (direction == 0 && dacindex >= config->final_pot_mv)) 
	{
    	// ============================
        // FASE 1: PULSO FORWARD
        // ============================
        
        int64_t t_start = esp_timer_get_time(); // Marca de tiempo (µs)

        // Espera activa hasta el momento del trigger
        while ((esp_timer_get_time() - t_start) < adc_trigger_wait_us) {
            // Chequeo de abortar (USB)
           /* if (udi_cdc_is_rx_ready()) { 
                if (getchar() == 'a') return 1;
            }*/
        }

        // Trigger ADC
        ADS125X_WAKEUP_HAL();
        
        // Esperar a que DRDY baje (Dato listo)
        // NOTA: gpio_get_level devuelve 1 si es HIGH. Esperamos mientras sea HIGH.
        //while (gpio_get_level(ADS_DRDY_PIN) == 1);

		// NOTA: ADS125X_WAIT_DYDR_HAL internamente hace lo mismo que este bucle while
		ADS125X_WAIT_DYDR_HAL();
        // Leer dato
        forward = ADS125X_READVOLT_HAL(); 
        ADS125X_STANDBY_HAL();

        // Esperar a que termine el semiciclo completo
        while ((esp_timer_get_time() - t_start) < half_cycle_period_us);


        // ============================
        // CAMBIO DE VOLTAJE (MITAD CICLO)
        // ============================
        if (direction == 1)
            MAX5217_DAC_WRITE_HAL_MV(dacindex - config->pulse_amplitude_mv);
        else
            MAX5217_DAC_WRITE_HAL_MV(dacindex + config->pulse_amplitude_mv);

        t_start = esp_timer_get_time(); // Reiniciar cronómetro

        // ============================
        // FASE 2: PULSO REVERSE
        // ============================
        
        while ((esp_timer_get_time() - t_start) < adc_trigger_wait_us) {
           /* if (udi_cdc_is_rx_ready()) { 
                if (getchar() == 'a') return 1;
            }*/
        }

        ADS125X_WAKEUP_HAL();
        //while (gpio_get_level(ADS_DRDY_PIN) == 1);

		// NOTA: ADS125X_WAIT_DYDR_HAL internamente hace lo mismo que este bucle while
		ADS125X_WAIT_DYDR_HAL();

        reverse = ADS125X_READVOLT_HAL(); 
        ADS125X_STANDBY_HAL();

        while ((esp_timer_get_time() - t_start) < half_cycle_period_us);

        // ============================
        // AVANCE DE ESCALÓN
        // ============================
        lastindex = dacindex;

        if (direction == 1) {
            dacindex += config->step_pot_mv;
            MAX5217_DAC_WRITE_HAL_MV(dacindex + config->pulse_amplitude_mv);
        } else {
            dacindex -= config->step_pot_mv;
            MAX5217_DAC_WRITE_HAL_MV(dacindex - config->pulse_amplitude_mv);
        }


		data.lastindex = lastindex;
		data.forward = forward;
		data.reverse = reverse;
		
		//printf("B\n");		// Definir si es necesario este comando indicando algo para la comunicacion
            // Enviamos la cabecera antes de la estructura
        uart_write_bytes(UART_NUM_0, header, 2);
		uart_write_bytes(UART_NUM_0, &data, 10);
        //printf("index: %d -----  forward: %f ---- reverse: %f ------ difference: %f \n", data.lastindex, data.forward, data.reverse, (data.forward - data.reverse));
        //printf("%f \n",(data.forward - data.reverse));
		printf("\n");

        // WATCHDOG RESET (IMPORTANTE) ?? Para que?
        // esp_task_wdt_reset(); 
    }

	VOLT_EXP_STOP();
    printf("B");
    return 0;
}

void execute_CV_experiment(CV_config *config)
{
    
}

void execute_PA_experiment(PA_config *config)
{
    
}