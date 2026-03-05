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
/*                      Global Variables for Experiments                        */
/********************************************************************************/

bool ELECTRODES_STATE = DISCONNECTED;
bool AGITATOR_STATE = OFF;


/********************************************************************************/
/*              Auxiliary Complementary Functions for Experiments               */
/********************************************************************************/

/**
 * @brief Just returns the current value of variable ELECTRODE_STATE
 *  
 */
bool get_Electrodes_State (void)
{
    return ELECTRODES_STATE;
}

/**
 * @brief Just returns the current value of variable AGITATOR_STATE
 *  
 */
bool get_Agitator_State (void)
{
    return AGITATOR_STATE;
}


/**
 * @brief Connects measurement cell to rest of circuit before the start of an experiment.
 *  
 */
void VOLT_EXP_START (void)
{

    if (ELECTRODES_STATE == DISCONNECTED)
    {
        gpio_set_level(MAX4737_RE_FB_EN, HIGH);
        gpio_set_level(MAX4737_WE_EN, HIGH);

        esp_rom_delay_us(10000); // Make sure WE circuit is connected before control voltage applied
        gpio_set_level(MAX4737_CE_EN, HIGH);
        ELECTRODES_STATE = CONNECTED;
        uart_write_bytes(UART_NUM_0, MSG_CELL_CONNECTED, strlen(MSG_CELL_CONNECTED));
    }
}

/**
* @brief Disconnects measurement cell.
*/
void VOLT_EXP_STOP (void)
{
    if (ELECTRODES_STATE == CONNECTED)
    {
        gpio_set_level(MAX4737_RE_FB_EN, LOW);
        gpio_set_level(MAX4737_CE_EN, LOW);
    
        esp_rom_delay_us(10000);
        // Make sure WE is last to disconnect
	    gpio_set_level(MAX4737_WE_EN, LOW);
        ELECTRODES_STATE = DISCONNECTED;
        uart_write_bytes(UART_NUM_0, MSG_CELL_DISCONNECTED, strlen(MSG_CELL_DISCONNECTED));
    }
}

/********************************************************************************/
/*                       Experiment Execution Functions                         */
/********************************************************************************/

/**
 * @brief Executes the preconditioning stage for Stripping Voltammetry experiments.
 * * This function handles the initial stages of a stripping experiment as described in the 
 * technical manual:
 * 1. Deposition: Applies a specific potential (Deposition E) while stirring the solution 
 * to preconcentrate the analyte in the mercury electrode[cite: 54, 105].
 * 2. Quiet Time: Stops the stirring and allows the system to reach equilibrium 
 * before the stripping scan begins[cite: 55, 105].
 * * @param[in] config Pointer to the configuration structure containing:
 * - \b deposition_pot_mv: The potential applied during the accumulation step.
 * - \b deposition_time_s: Duration of the deposition/preconcentration step (seconds).
 * - \b quiet_time_s: Stabilization time without stirring, typically 10-15s[cite: 55, 105].
 * * @return uint8_t Returns 0 upon successful completion of the treatment.
 */
uint8_t execute_DEPO_PRECOND (DEPO_PRECOND_config *config)
{
    int64_t quiet_until;

    VOLT_EXP_START();

    printf("depo_pot_mv: %d \n", (int) (config->deposition_pot_mv));
    printf("depo_time_mv: %d \n", (int) (config->deposition_time_s));
    printf("depo_pot_quiet_time: %d \n", (int) (config->quiet_time_s));
    printf("depo_stir_on_off: %d \n", (int) (config->stir_on_off));
    // ==========================================
    // STAGE 1: DEPOSITION (PRECONCENTRATION)
    // ==========================================
    // Apply the deposition potential 
    MAX5217_DAC_WRITE_HAL_MV(config->deposition_pot_mv);

    // Turn on the agitator/stirrer to improve efficiency 
    //turn_on_agitator(); 

    // Maintain potential and stirring for the defined duration 
    if (config->deposition_time_s > 0)
    {
        quiet_until = esp_timer_get_time() + (config->deposition_time_s * 1000000LL);
        while (esp_timer_get_time() < quiet_until)
        {
            if (ABORT_FLAG) goto abort_precond;
            vTaskDelay(pdMS_TO_TICKS(10)); // Mantiene el sistema responsivo
        }
    }
    // ==========================================
    // STAGE 2: QUIET TIME (EQUILIBRIUM)
    // ==========================================
    // Stop the stirrer to allow the solution to rest 
    //turn_off_agitator();

    // The system remains at the deposition potential but at rest 
    // Generally, Quiet Time ranges from 10 to 15 seconds 
    if (config->quiet_time_s > 0)
    {
        quiet_until = esp_timer_get_time() + (config->quiet_time_s * 1000000LL);
        while (esp_timer_get_time() < quiet_until)
        {
            if (ABORT_FLAG) goto abort_precond;
            vTaskDelay(pdMS_TO_TICKS(10)); // Mantiene el sistema responsivo
        }
    }
    

    return 0; // Preconditioning finished successfully

abort_precond:
    VOLT_EXP_STOP();
    ADS125X_STANDBY_HAL();
    uart_write_bytes(UART_NUM_0, MSG_FINISHED, strlen(MSG_FINISHED));
    ABORT_FLAG = 0;
    return 1;
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
 * @return 0 if the functions manages to get to the end properly
 */
uint8_t execute_SWV_experiment(SWV_config *config)
{
    uint8_t direction = 1;
    uint32_t prop_delay_us = 0;
    int16_t dacindex = config->initial_pot_mv; 

    swv_packet_t pkt;
    float forward = 0;
    float reverse = 0;

    if (config->initial_pot_mv < config->final_pot_mv)
        direction = 1;
    else
        direction = 0;
    
    uint32_t half_cycle_period_us = 1000000UL / ((uint32_t)(config->freq_hz) * 2);
    prop_delay_us = ADS125X_Get_Prop_Delay_Us();

    ADS125X_WAKEUP_HAL();
    ADS125X_STANDBY_HAL();

    VOLT_EXP_START();

    if (config->quiet_time_s > 0)
    {
        int64_t quiet_until = esp_timer_get_time() + (config->quiet_time_s * 1000000LL);
        while (esp_timer_get_time() < quiet_until)
        {
            if (ABORT_FLAG) goto abort_swv;
            vTaskDelay(pdMS_TO_TICKS(10)); // Mantiene el sistema responsivo
        }
    }

    // Sincronización absoluta inicial
    int64_t t_target = esp_timer_get_time(); 

    // --- INICIO DE BUCLE DE BARRIDO ---
    while ((direction == 1 && dacindex <= config->final_pot_mv) || (direction == 0 && dacindex >= config->final_pot_mv)) 
    {

        // --- CHEQUEO DE ABORTO (Ultra veloz) ---
        if (ABORT_FLAG) goto abort_swv;
        
        // ============================
        // FASE 1: PULSO FORWARD
        // ============================
        // Calculamos el próximo objetivo
        t_target += half_cycle_period_us;

        if (direction == 1)
            MAX5217_DAC_WRITE_HAL_MV(dacindex + config->pulse_amplitude_mv);
        else
            MAX5217_DAC_WRITE_HAL_MV(dacindex - config->pulse_amplitude_mv);

        // Espera al trigger del ADC
        while (esp_timer_get_time() < (t_target - prop_delay_us));
        
        ADS125X_WAKEUP_HAL();
        ADS125X_WAIT_DYDR_HAL();
        forward = ADS125X_READVOLT_HAL(); 
        ADS125X_STANDBY_HAL();
        
        // Espera estricta al final del semiciclo
        while (esp_timer_get_time() < t_target);

        // ============================
        // FASE 2: PULSO REVERSE
        // ============================
        t_target += half_cycle_period_us;

        if (direction == 1)
            MAX5217_DAC_WRITE_HAL_MV(dacindex - config->pulse_amplitude_mv);
        else
            MAX5217_DAC_WRITE_HAL_MV(dacindex + config->pulse_amplitude_mv);

        // Espera al trigger del ADC
        while (esp_timer_get_time() < (t_target - prop_delay_us));

        ADS125X_WAKEUP_HAL();
        ADS125X_WAIT_DYDR_HAL();
        reverse = ADS125X_READVOLT_HAL(); 
        ADS125X_STANDBY_HAL();
        
        // Espera estricta al final del semiciclo
        while (esp_timer_get_time() < t_target);

        // ============================
        // PROCESAMIENTO (FUERA DEL TIMING CRÍTICO)
        // ============================
        pkt.lastindex = dacindex;
        pkt.forward = forward;
        pkt.reverse = reverse;
        
        // El tiempo que tarden estas funciones es absorbido por el inicio 
        // de la próxima iteración porque t_target no se resetea.
        uart_write_bytes(UART_NUM_0, HEADER_SWV, HEADER_SWV_SIZE);
        uart_write_bytes(UART_NUM_0, &pkt, sizeof(pkt));
        uart_write_bytes(UART_NUM_0, PACKET_TAIL, TAIL_SIZE);

        if (direction == 1) 
            dacindex += config->step_pot_mv;
        else 
            dacindex -= config->step_pot_mv;

        // Si la UART tardó mucho, t_target += half_cycle_period_us al inicio 
        // del bucle compensará ese retraso inmediatamente.
    }

// Finalización normal
    VOLT_EXP_STOP();
    ADS125X_STANDBY_HAL();
    uart_write_bytes(UART_NUM_0, MSG_FINISHED, strlen(MSG_FINISHED));
    return 0;

abort_swv:
    VOLT_EXP_STOP();
    ADS125X_STANDBY_HAL();
    uart_write_bytes(UART_NUM_0, MSG_FINISHED, strlen(MSG_FINISHED));
    ABORT_FLAG = 0;
    return 1;
}

/*uint8_t execute_SWV_experiment(SWV_config *config)
{
    uint8_t direction = 1;
    uint32_t prop_delay_us = 0;
    int16_t dacindex = config->initial_pot_mv; 
    const uint8_t header[2] = {0xAA, 0xBB};
    const uint8_t end_byte[1] = {0x0A};

    float forward = 0;
    float reverse = 0;
    swv_packet_t data;

    if (config->initial_pot_mv < config->final_pot_mv)
        direction = 1;
    else
        direction = 0;
    
    uint32_t half_cycle_period_us = 1000000UL / ((uint32_t)(config->freq_hz) * 2);
    prop_delay_us = ADS125X_Get_Prop_Delay_Us();

    ADS125X_WAKEUP_HAL();
    ADS125X_STANDBY_HAL();

    VOLT_EXP_START();

    // Sincronización absoluta: Capturamos el tiempo base una sola vez
    int64_t t_next_edge = esp_timer_get_time(); 

    // --- INICIO DE BUCLE DE BARRIDO ---
    while ((direction == 1 && dacindex <= config->final_pot_mv) || (direction == 0 && dacindex >= config->final_pot_mv)) 
    {
        // ============================
        // FASE 1: PULSO FORWARD
        // ============================
        
        // Calculamos el momento exacto en que DEBE terminar este semiciclo
        t_next_edge += half_cycle_period_us;

        if (direction == 1)
            MAX5217_DAC_WRITE_HAL_MV(dacindex + config->pulse_amplitude_mv);
        else
            MAX5217_DAC_WRITE_HAL_MV(dacindex - config->pulse_amplitude_mv);

        // Esperamos hasta el momento del trigger (final del pulso - delay del ADC)
        while (esp_timer_get_time() < (t_next_edge - prop_delay_us));
        
        ADS125X_WAKEUP_HAL();
        ADS125X_WAIT_DYDR_HAL();
        forward = ADS125X_READVOLT_HAL(); 
        ADS125X_STANDBY_HAL();
        
        // Espera absoluta hasta el final del semiciclo
        while (esp_timer_get_time() < t_next_edge);

        // ============================
        // FASE 2: PULSO REVERSE
        // ============================
        
        t_next_edge += half_cycle_period_us;

        if (direction == 1)
            MAX5217_DAC_WRITE_HAL_MV(dacindex - config->pulse_amplitude_mv);
        else
            MAX5217_DAC_WRITE_HAL_MV(dacindex + config->pulse_amplitude_mv);

        while (esp_timer_get_time() < (t_next_edge - prop_delay_us));

        ADS125X_WAKEUP_HAL();
        ADS125X_WAIT_DYDR_HAL();
        reverse = ADS125X_READVOLT_HAL(); 
        ADS125X_STANDBY_HAL();
        
        while (esp_timer_get_time() < t_next_edge);

        // ============================
        // PROCESAMIENTO Y AVANCE
        // ============================
        data.lastindex = dacindex;
        data.forward = forward;
        data.reverse = reverse;
        
        // El tiempo que tarden estas funciones ahora NO estira la frecuencia
        uart_write_bytes(UART_NUM_0, header, 2);
        uart_write_bytes(UART_NUM_0, &data, 10);
        uart_write_bytes(UART_NUM_0, end_byte, 1);

        if (direction == 1) dacindex += config->step_pot_mv;
        else dacindex -= config->step_pot_mv;

        // Reset del Watchdog por seguridad
        // esp_task_wdt_reset(); 
    }

    VOLT_EXP_STOP();
    printf("B");
    return 0;
}*/


/**
 * @brief Executes Linear Sweep Voltammetry (LSV) or Cyclic Voltammetry (CV) experiments.
 * * The function generates a potential ramp with fixed steps of 0.5 mV.
 * If the number of segments is 1, it performs LSV. If it is > 1, it performs CV by reversing
 * [cite_start]the direction at the switching potentials[cite: 107, 108].
 * * @param[in] config         Pointer to the configuration structure containing experiment parameters:
 * - \b initial_pot_mv:      Start potential in millivolts.
 * - \b switching_pot1_mv:   Switching potential 1 in millivolts.
 * - \b switching_pot2_mv:   Switching potential 2 in millivolts.
 * - \b final_pot_mv:        Final potential in millivolts.
 * - \b segments:            Number of segments.
 * - \b scan_rate_mv_s:      Scan rate in mv/s. Execution rate or velocity of the experiment. This is not the sample rate.
 * - \b quiet_time_s:        Equilibration time before the scan starts (seconds).
 * */
/**
 * @brief Ejecuta Linear Sweep o Cyclic Voltammetry con resolución de 0.5 mV.
 * Header: 0xCCDD. Datos: 8 bytes (float voltage, float current).
 */
uint8_t execute_LSV_CV_experiment(LSV_CV_config *config)
{
    const float res = 0.5f;
    float current_v = (float)config->initial_pot_mv;
    
    lsv_cv_packet_t pkt;
    pkt.header[0] = HEADER_LSV[0]; 
    pkt.header[1] = HEADER_LSV[1];
    pkt.tail = PACKET_TAIL[0];

    // 1. CONSTRUCCIÓN DE LA RUTA SEGÚN EL MANUAL
    float targets[config->segments];
    
    for (int i = 0; i < config->segments; i++) {
        int seg_num = i + 1;

        if (seg_num == config->segments) {
            // EL ÚLTIMO segmento siempre termina en el potencial FINAL independiente
            targets[i] = (float)config->final_pot_mv;
        } 
        else if (seg_num == 1) {
            // EL PRIMER segmento siempre va al Switch 1
            targets[i] = (float)config->switching_pot1_mv;
        } 
        else {
            // SEGMENTOS INTERMEDIOS: Oscilan entre Switch 2 y Switch 1
            // Si el segmento es par (2, 4, 6...) va al Switch 2
            // Si el segmento es impar (3, 5, 7...) va al Switch 1
            targets[i] = (seg_num % 2 == 0) ? (float)config->switching_pot2_mv : (float)config->switching_pot1_mv;
        }
    }

    // 2. CONFIGURACIÓN DE TIEMPO
    uint32_t step_delay_us = (uint32_t)((res * 1000000.0f) / config->scan_rate_mv_s);

    /*uint8_t drate;
    if(config->scan_rate_mv_s > 1000) 
        drate = ADS125X_DRATE_30000SPS; // único que alcanza
    else if (config->scan_rate_mv_s >  500) 
        drate = ADS125X_DRATE_7500SPS;  // ~17 bits efectivos
    else if (config->scan_rate_mv_s >  200) 
        drate = ADS125X_DRATE_3750SPS;  // ~18 bits efectivos
    else if (config->scan_rate_mv_s >   50) 
        drate = ADS125X_DRATE_1000SPS;  // ~20 bits efectivos
    else if (config->scan_rate_mv_s >   20) 
        drate = ADS125X_DRATE_100SPS;   // ~22 bits efectivos
    else                                    
        drate = ADS125X_DRATE_30SPS;     // ~23 bits efectivos

    ADS125X_SET_DRATE_HAL(drate);
    // Esperar settling time del filtro sinc tras cambio de drate (~1 ciclo de conversión)
    vTaskDelay(pdMS_TO_TICKS(10));*/

    ADS125X_WAKEUP_HAL();
    VOLT_EXP_START();
    MAX5217_DAC_WRITE_HAL_MV((int16_t)roundf(current_v));

    if (config->quiet_time_s > 0) {
        int64_t q_end = esp_timer_get_time() + (config->quiet_time_s * 1000000LL);
        while (esp_timer_get_time() < q_end) {
            if (ABORT_FLAG) goto abort_experiment;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    // 3. EJECUCIÓN DE LA TRAYECTORIA
    for (int s = 0; s < config->segments; s++) 
    {
        float target_v = targets[s];
        int8_t dir = (target_v > current_v) ? 1 : -1;
        uint32_t n_steps = (uint32_t)roundf(fabsf(target_v - current_v) / res);

        for (uint32_t step = 0; step <= n_steps; step++) 
        {
            if (ABORT_FLAG) 
                goto abort_experiment;

            int64_t t_start = esp_timer_get_time();

            MAX5217_DAC_WRITE_HAL_MV((int16_t)roundf(current_v));

            ADS125X_WAIT_DYDR_HAL(); 
            pkt.voltage_index_mv = current_v;
            pkt.voltage_meas  = ADS125X_READVOLT_HAL();
            
            uart_write_bytes(UART_NUM_0, &pkt, sizeof(lsv_cv_packet_t));
            
            int64_t elapsed = esp_timer_get_time() - t_start;
            int64_t to_wait = (int64_t)step_delay_us - elapsed;
            if (to_wait > 0) esp_rom_delay_us((uint32_t)to_wait);

            if (step < n_steps) current_v += (dir * res);
        }
        current_v = target_v; // Asegurar precisión al final de cada rampa
    }

    VOLT_EXP_STOP();
    ADS125X_STANDBY_HAL();
    uart_write_bytes(UART_NUM_0, MSG_FINISHED, strlen(MSG_FINISHED));
    return 0;

abort_experiment:
    VOLT_EXP_STOP();
    ADS125X_STANDBY_HAL();
    uart_write_bytes(UART_NUM_0, MSG_FINISHED, strlen(MSG_FINISHED));
    ABORT_FLAG = 0;
    return 1;
}

/*uint8_t execute_LSV_CV_experiment(LSV_CV_config *config)
{
    const float res = 0.5f;
    float current_v = (float)config->initial_pot_mv;
    
    lsv_cv_packet_t pkt;
    pkt.header[0] = HEADER_LSV[0]; 
    pkt.header[1] = HEADER_LSV[1];
    pkt.tail = PACKET_TAIL[0];

    // 1. CONSTRUCCIÓN DE LA RUTA SEGÚN EL MANUAL
    float targets[config->segments];
    
    for (int i = 0; i < config->segments; i++) {
        int seg_num = i + 1;

        if (seg_num == config->segments) {
            targets[i] = (float)config->final_pot_mv;
        } 
        else if (seg_num == 1) {
            targets[i] = (float)config->switching_pot1_mv;
        } 
        else {
            targets[i] = (seg_num % 2 == 0) ? (float)config->switching_pot2_mv : (float)config->switching_pot1_mv;
        }
    }

    // 2. CONFIGURACIÓN DE TIEMPO + OSR DINÁMICO
    uint32_t step_delay_us = (uint32_t)((res * 1000000.0f) / config->scan_rate_mv_s);

    // Overhead fijo por iteración: ADC(33) + UART(119) + DAC(10) + ESP32(20) = ~182 µs
    // Margen de seguridad del 20%: overhead_total = 182 * 1.2 = ~220 µs por muestra
    const uint32_t OVERHEAD_PER_SAMPLE_US = 40;
    // El overhead fijo por paso (UART + DAC) no escala con OSR, se paga una sola vez
    const uint32_t OVERHEAD_FIXED_US = 160;  // UART(119) + DAC(10) + ESP32(20) + margen

    uint32_t osr;

    if      (config->scan_rate_mv_s <=   50) osr = 128;
    else if (config->scan_rate_mv_s <=  100) osr =  64;
    else if (config->scan_rate_mv_s <=  200) osr =  32;
    else if (config->scan_rate_mv_s <=  500) osr =  16;
    else if (config->scan_rate_mv_s <= 1000) osr =   8;
    else if (config->scan_rate_mv_s <= 2000) osr =   2;
    else                                     osr =   1;

    // Verificación de seguridad: si el OSR calculado no cabe en el step_delay, reducirlo
    while (osr > 1 && (OVERHEAD_FIXED_US + osr * OVERHEAD_PER_SAMPLE_US) > step_delay_us) {
        osr /= 2;
    }

    ADS125X_WAKEUP_HAL();
    VOLT_EXP_START();
    MAX5217_DAC_WRITE_HAL_MV((int16_t)roundf(current_v));

    if (config->quiet_time_s > 0) {
        int64_t q_end = esp_timer_get_time() + (config->quiet_time_s * 1000000LL);
        while (esp_timer_get_time() < q_end) {
            if (ABORT_FLAG) goto abort_experiment;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    // 3. EJECUCIÓN DE LA TRAYECTORIA
    for (int s = 0; s < config->segments; s++) 
    {
        float target_v = targets[s];
        int8_t dir = (target_v > current_v) ? 1 : -1;
        uint32_t n_steps = (uint32_t)roundf(fabsf(target_v - current_v) / res);

        for (uint32_t step = 0; step <= n_steps; step++) 
        {
            if (ABORT_FLAG) 
                goto abort_experiment;

            int64_t t_start = esp_timer_get_time();

            MAX5217_DAC_WRITE_HAL_MV((int16_t)roundf(current_v));

            // Oversampling dinámico: promedio de osr lecturas del ADC
            float acc = 0.0f;
            for (uint32_t k = 0; k < osr; k++) {
                ADS125X_WAIT_DYDR_HAL();
                acc += ADS125X_READVOLT_HAL();
            }
            pkt.voltage_index_mv = current_v;
            pkt.voltage_meas     = acc / (float)osr;
            
            uart_write_bytes(UART_NUM_0, &pkt, sizeof(lsv_cv_packet_t));
            
            int64_t elapsed = esp_timer_get_time() - t_start;
            int64_t to_wait = (int64_t)step_delay_us - elapsed;
            if (to_wait > 0) esp_rom_delay_us((uint32_t)to_wait);

            if (step < n_steps) current_v += (dir * res);
        }
        current_v = target_v;
    }

    VOLT_EXP_STOP();
    ADS125X_STANDBY_HAL();
    uart_write_bytes(UART_NUM_0, MSG_FINISHED, strlen(MSG_FINISHED));
    return 0;

abort_experiment:
    VOLT_EXP_STOP();
    ADS125X_STANDBY_HAL();
    uart_write_bytes(UART_NUM_0, MSG_FINISHED, strlen(MSG_FINISHED));
    ABORT_FLAG = 0;
    return 1;
}*/



uint8_t execute_PA_experiment(PA_config *config)
{
    return 0;
}

//Constant Potential Electrolysis
uint8_t execute_CPE_experiment(CPE_config *config)
{
    return 0;
}