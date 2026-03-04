
#include "esp_wifi.h"
//#include "esp_bt.h"


// Tus librerías
#include "gpio_config.h"
#include "ads1255.h"
#include "max5217.h"
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
        uart_write_bytes(UART_NUM_0, MSG_WAITING, strlen(MSG_WAITING));

        printf("Core 1: Experimento finalizado. Retornando a WAITING\n");
    }
}


// --- MAIN ---
void app_main(void)
{
    

    // Inicializar UART
    init_UART();
    esp_wifi_stop();
    //esp_bt_controller_disable();
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
        configMAX_PRIORITIES - 1,                     // Prioridad máxima (Crítica para el timing)
        get_ExpControlTask_pointer(),                   // Handle
        1                       // <--- PINNED TO CORE 1
    );
    
}
