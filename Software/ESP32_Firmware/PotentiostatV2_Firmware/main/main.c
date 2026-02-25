#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "esp_log.h"
//#include "esp_attr.h"
//#include "esp_rom_sys.h"

// Tus librerías
#include "gpio_config.h"
#include "ads1255.h"
#include "max5217.h"
#include "muxes.h"
#include "general.h"
#include "fsm.h"

// --- DEFINICIONES DE HARDWARE ---
#define GREEN_LED          GPIO_NUM_2   


static const char *TAG = "MAIN_APP";


// --- PROCESAMIENTO DE COMANDOS (Lógica Central) ---
void procesar_comando_completo(int comando) {
    
    // CASO 1: LEER REGISTROS (Comandos 1 al 11)
    if (comando >= 1 && comando <= 11) {
        
        const uint8_t mapa_registros[] = {
            ADS125X_REG_STATUS, ADS125X_REG_MUX, ADS125X_REG_ADCON, ADS125X_REG_DRATE, ADS125X_REG_IO, 
            ADS125X_REG_OFC0, ADS125X_REG_OFC1, ADS125X_REG_OFC2, ADS125X_REG_FSC0, ADS125X_REG_FSC1, ADS125X_REG_FSC2
        };
        const char *nombres_registros[] = {
            "STATUS", "MUX", "ADCON", "DRATE", "IO", "OFC0", "OFC1", "OFC2", "FSC0", "FSC1", "FSC2"
        };
        
        uint8_t index = comando - 1;
        uint8_t reg_addr = mapa_registros[index];
        uint8_t reg_val = 0;
        

        ADS125X_READ_REG_HAL(reg_addr, &reg_val, 1);
        printf("[CMD %d] REG %s (0x%02X) = 0x%02X\n", comando, nombres_registros[index], reg_addr, reg_val);
    }
    
    // CASO 2: LEER MUESTRA ADC (Comando 12)
    else if (comando == 12) {
        float voltage = ADS125X_READVOLT_HAL();
        printf("ADC Sample Voltage: %f V\n", voltage);
    }
    
    // CASO 3: RESETEAR ADC (Comando 13)
    else if (comando == 13) {
        ADS125X_CMD_SEND_HAL(ADS125X_CMD_RESET);
        esp_rom_delay_us(5000);
    }
    
    // CASO 4: RESERVADOS (Comandos 14-18) - Preparados para futuro
    else if (comando >= 14 && comando <= 27) {
        switch (comando) {
            case 14: printf("Comando 14: RESERVADO (Sin implementar)\n");
                     MAX5217_DAC_WRITE_HAL_MV(0.0); 
                     break;
            case 15: printf("Comando 15: RESERVADO (Sin implementar)\n");
                     MAX5217_DAC_WRITE_HAL_MV(2500.0); 
                     break;
            case 16: printf("Comando 16: RESERVADO (Sin implementar)\n");
                     MAX5217_DAC_WRITE_HAL_MV(-2400.0);
                     break;
            case 17: printf("Comando 17: RESERVADO (Sin implementar)\n");
                     MAX5217_DAC_WRITE_HAL_MV(-2500.0);
                     break;
            case 18: printf("Comando 18: RESERVADO (Sin implementar)\n");
                     MAX5217_DAC_WRITE_HAL_MV(-2000.0);
                     break;
            case 19: printf("Comando 18: RESERVADO (Sin implementar)\n");
                     MAX4617_Set_Gain(NO_GAIN);
                     break;
            case 20: printf("Comando 18: RESERVADO (Sin implementar)\n");
                     MAX4617_Set_Gain(GAIN1_100);
                     break;
            case 21: printf("Comando 18: RESERVADO (Sin implementar)\n");
                     MAX4617_Set_Gain(GAIN2_3K);
                     break;
            case 22: printf("Comando 18: RESERVADO (Sin implementar)\n");
                     MAX4617_Set_Gain(GAIN3_30K);
                     break;
            case 23: printf("Comando 18: RESERVADO (Sin implementar)\n");
                     MAX4617_Set_Gain(GAIN4_300K);
                     break;
            case 24: printf("Comando 18: RESERVADO (Sin implementar)\n");
                     MAX4617_Set_Gain(GAIN5_3M);
                     break;
            case 25: printf("Comando 18: RESERVADO (Sin implementar)\n");
                     MAX4617_Set_Gain(GAIN6_30M);
                     break;
            case 26: printf("Comando 18: RESERVADO (Sin implementar)\n");
                     MAX4617_Set_Gain(GAIN7_100M);
                     break;
        }
    }
    
    // ERROR
    else {
        printf("Comando desconocido: %d\n", comando);
    }
}


// --- MAIN ---
/*void app_main(void)
{
    // Inicializar UART
    init_uart();
    config_pin(GREEN_LED, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    MAX4737_Config_Pins();
    MAX4617_Config_Pins();
    gpio_set_level(MAX4737_CE_EN, HIGH);
    gpio_set_level(MAX4737_RE_FB_EN, HIGH);
    gpio_set_level(MAX4737_WE_EN, HIGH);

    MAX4617_Set_Gain(GAIN4_300K);

    // --- INICIALIZACIÓN DAC MAX5217 ---
    ESP_LOGI(TAG, "Configurando DAC MAX5217...");
    uart_wait_tx_done(UART_NUM_0, 100); // Wait up to 1000ms

    MAX5217_DAC_Setup_HAL();

    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Inicializando ADS1255...");
    ADS125X_INIT_HAL(ADS125X_DRATE_30000SPS, ADS125X_PGA1, ADS125X_BUFOFF);
    vTaskDelay(pdMS_TO_TICKS(100)); // Pequeña espera
    ADS125X_STANDBY_HAL();
    ESP_LOGI(TAG, "Sistema Listo. Escriba numero + ENTER (Ej: '10' o '12').");

    ESP_LOGI(TAG, "Sistema Listo. Generando onda en DAC...");
    // Variables UART
    uint8_t data[1]; 
    char rx_buffer[UART_RX_BUFFER_SIZE];
    int rx_index = 0;
    
    // Variables LED
    int led_state = 0;
    const int blink_period_ms = 500;
    int ms_counter = 0;

    // Variables para Generación de Onda (Triangular)
    double wave_volt = 0.0;
    double wave_step = 50.0; // Saltos de 50mV cada ~10ms

// --- BUCLE PRINCIPAL ---
    while (1) {
        // A. Lectura de UART (Timeout 10ms - Marca el ritmo del bucle)
        int len = uart_read_bytes(UART_PORT_NUM, data, 1, 10 / portTICK_PERIOD_MS);

        if (len > 0) {
            char c = (char)data[0];
            if (c == '\n' || c == '\r') {
                if (rx_index > 0) {
                    rx_buffer[rx_index] = '\0';
                    int comando_num = atoi(rx_buffer);
                    if (comando_num > 0) procesar_comando_completo(comando_num);
                    rx_index = 0;
                }
            } else {
                if (rx_index < UART_RX_BUFFER_SIZE - 1) rx_buffer[rx_index++] = c;
                else rx_index = 0;
            }
        }

        // B. Generación de Onda Cíclica (DAC)
        // Se actualiza cada ciclo del while (aprox cada 10ms debido al timeout del UART)
    
        wave_volt += wave_step;
            
        // Límites para oscilar entre -2500mV y +2500mV (Rango completo del DAC según tu lib)
        if (wave_volt >= 2500.0) {
            wave_volt = 2500.0;
            wave_step = -wave_step; // Invertir dirección
        } else if (wave_volt <= -2500.0) {
            wave_volt = -2500.0;
            wave_step = -wave_step; // Invertir dirección
        }
            
        // Escribir al DAC
        MAX5217_DAC_WRITE_HAL_MV(wave_volt);
        

        // C. Parpadeo del LED
        ms_counter += 10; 
        if (ms_counter >= blink_period_ms) {
            led_state = !led_state;
            gpio_set_level(GREEN_LED, led_state);
            ms_counter = 0;
        }
        ADS125X_WAKEUP_HAL();
        ADS125X_WAIT_DYDR_HAL();
        float voltage = ADS125X_READVOLT_HAL();
        ADS125X_STANDBY_HAL();
        //printf("ADC Sample Voltage: %f V\n", voltage);
        printf("%f\n", voltage);
    }
}*/



// --- MAIN ---
/*void app_main(void)
{
    // Inicializar UART
    init_uart();
    config_pin(GREEN_LED, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    MAX4737_Config_Pins();
    MAX4617_Config_Pins();
    gpio_set_level(MAX4737_CE_EN, LOW);
    gpio_set_level(MAX4737_RE_FB_EN, LOW);
    gpio_set_level(MAX4737_WE_EN, LOW);

    MAX4617_Set_Gain(GAIN5_3M);

    // --- INICIALIZACIÓN DAC MAX5217 ---
    ESP_LOGI(TAG, "Configurando DAC MAX5217...");
    uart_wait_tx_done(UART_NUM_0, 100); // Wait up to 1000ms

    MAX5217_DAC_Setup_HAL();
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Inicializando ADS1255...");
    ADS125X_INIT_HAL(ADS125X_DRATE_30000SPS, ADS125X_PGA1, ADS125X_BUFOFF);
    vTaskDelay(pdMS_TO_TICKS(100)); 
    ADS125X_STANDBY_HAL();
    ESP_LOGI(TAG, "Sistema Listo.");

    SWV_config experiment_swv1;
    LSV_CV_config experiment_cv1;

    // Variables LED
    int led_state = 0;

    experiment_swv1.initial_pot_mv = (int16_t) -1000;
    experiment_swv1.final_pot_mv = (int16_t) -200;
    experiment_swv1.freq_hz = 40;
    experiment_swv1.pulse_amplitude_mv = 25;
    experiment_swv1.step_pot_mv = 2;
    experiment_swv1.quiet_time_s = 2;

    experiment_cv1.initial_pot_mv = (int16_t) -1000;
    experiment_cv1.switching_pot1_mv = (int16_t) 500;
    experiment_cv1.switching_pot2_mv = (int16_t) -1000;
    experiment_cv1.final_pot_mv = (int16_t) 500;
    experiment_cv1.segments = 10;
    experiment_cv1.scan_rate_mv_s = 100;
    experiment_cv1.quiet_time_s = 10;


    //vTaskDelay(pdMS_TO_TICKS(10000));
    //if (execute_SWV_experiment(&experiment_swv1) == 0)
    if (execute_LSV_CV_experiment(&experiment_cv1) == 0)
        printf("Experiment executed with success \n");

    while (1) 
    {

        led_state = !led_state;
        gpio_set_level(GREEN_LED, led_state);
        vTaskDelay(pdMS_TO_TICKS(500));
        //if (execute_SWV_experiment(&experiment_swv1) == 0)
            //printf("Experiment executed with success \n");
    }
}*/


TaskHandle_t xTaskUARTControlHandle = NULL;
potentiostat_fsm_t* main_fsm;

void uart_task(void *pvParameters) 
{
    uint8_t rx_data[128];

    while (1) {
        // Lectura manual de UART siguiendo tu requerimiento
        int len = uart_read_bytes(UART_NUM_0, rx_data, sizeof(rx_data), 10 / portTICK_PERIOD_MS);
        
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                process_char_FSM(main_fsm, (char)rx_data[i]);
            }
        }

        // Si el estado es EXECUTING, aquí puedes enviar los datos binarios 
        // acumulados en un buffer por el Core 1
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void experiment_exec_task(void *pvParameters) 
{
    exp_config* exp_config_pointer = get_exp_config_pointer();
    while (1) {
        // Esperar notificación de inicio (START_EXP recibido en FSM)
        // La tarea no consume CPU mientras espera.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        printf("Core 1: Iniciando ejecución de hardware...\n");

        //Identificar y ejecutar el experimento seleccionado
        // Usamos el ID del experimento guardado durante la fase de configuración.
        if (main_fsm->current_experiment == EXP_SWV) 
        {
            // Se asume que 'current_swv' fue llenada por sscanf en la FSM
            execute_SWV_experiment(&(exp_config_pointer->SWV));
        } 
        else if (main_fsm->current_experiment == EXP_LSV) 
        {
            execute_LSV_CV_experiment(&(exp_config_pointer->LSV));
        }
        else if (main_fsm->current_experiment == EXP_CPE) 
        {
            //execute_CPE_experiment(&(exp_config_pointer->CPE));
        }
        //Al salir de la función (por fin natural o por return 1 de ABORT):
        // Forzamos el retorno al estado WAITING para permitir nuevos comandos.
        main_fsm->current_experiment = EXP_NONE;
        transition_state_FSM(main_fsm, STATE_WAITING);
        uart_write_bytes(UART_NUM_0, "WAITING\n", 8);

        printf("Core 1: Experimento finalizado. Retornando a WAITING\n");
    }
}


// --- MAIN ---
void app_main(void)
{
    

    // Inicializar UART
    init_UART();
    main_fsm = get_fsm_pointer();
    init_FSM(main_fsm);
    config_pin(GREEN_LED, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    MAX4737_Config_Pins();
    MAX4617_Config_Pins();
    gpio_set_level(MAX4737_CE_EN, LOW);
    gpio_set_level(MAX4737_RE_FB_EN, LOW);
    gpio_set_level(MAX4737_WE_EN, LOW);

    MAX4617_Set_Gain(GAIN4_300K);

    // --- INICIALIZACIÓN DAC MAX5217 ---
    ESP_LOGI(TAG, "Configurando DAC MAX5217...");
    uart_wait_tx_done(UART_NUM_0, 100); // Wait up to 1000ms

    MAX5217_DAC_Setup_HAL();
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Inicializando ADS1255...");
    ADS125X_INIT_HAL(ADS125X_DRATE_30000SPS, ADS125X_PGA1, ADS125X_BUFOFF);
    vTaskDelay(pdMS_TO_TICKS(100)); 
    ADS125X_STANDBY_HAL();
    ESP_LOGI(TAG, "Sistema Listo.");


    // Variables LED
    int led_state = 0;

    // 2. Inicialización de periféricos (DAC, ADS1255, GPIOs)
    // init_potentiostat_hardware();

    // 3. Creación de la Tarea de Comunicaciones (Core 0)
    // Se encarga de la FSM, encuestas y configuración.
    xTaskCreatePinnedToCore(
        uart_task,          // Función de la tarea
        "UART_COMMS_TASK",      // Nombre
        4096,                   // Stack size
        NULL,                   // Parámetros
        10,                     // Prioridad alta
        &xTaskUARTControlHandle,                   // Handle
        0                       // <--- PINNED TO CORE 0
    );

    // 4. Creación de la Tarea de Ejecución (Core 1)
    // Se encarga del lazo de control y lectura del ADC.
    xTaskCreatePinnedToCore(
        experiment_exec_task,   // Función de la tarea
        "EXP_CONTROL_TASK",      // Nombre
        8192,                   // Stack size (más grande por procesamiento)
        NULL,                   // Parámetros
        20,                     // Prioridad máxima (Crítica para el timing)
        get_ExpControlTask_pointer(),                   // Handle
        1                       // <--- PINNED TO CORE 1
    );
    
}
