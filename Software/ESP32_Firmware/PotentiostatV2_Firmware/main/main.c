#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Necesario para atoi()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_rom_sys.h"
#include "driver/uart.h"

// Tus librerías
#include "gpio_config.h"
#include "ads1255.h"
#include "max5217.h"
#include "muxes.h"

// --- DEFINICIONES DE HARDWARE ---
#define GREEN_LED          GPIO_NUM_2   

// --- CONFIGURACIÓN UART ---
#define UART_PORT_NUM      UART_NUM_0
#define UART_BAUD_RATE     115200
#define BUF_SIZE           1024
#define UART_RX_BUFFER_SIZE 32 // Tamaño máximo de la línea de comando (ej: "10\n")

static const char *TAG = "MAIN_APP";

// Instancia global del ADC
ADS125X_t my_ads;
i2c_master_dev_handle_t dac_handle; // Handle para el DAC MAX5217

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
        
        ADS125X_Register_Read(&my_ads, reg_addr, &reg_val, 1);
        printf("[CMD %d] REG %s (0x%02X) = 0x%02X\n", comando, nombres_registros[index], reg_addr, reg_val);
    }
    
    // CASO 2: LEER MUESTRA ADC (Comando 12)
    else if (comando == 12) {
        float voltage = ADS125X_ADC_ReadVolt(&my_ads);
        printf("ADC Sample Voltage: %f V\n", voltage);
    }
    
    // CASO 3: RESETEAR ADC (Comando 13)
    else if (comando == 13) {
        ADS125X_CMD_Send(&my_ads, ADS125X_CMD_RESET);
        esp_rom_delay_us(5000);
    }
    
    // CASO 4: RESERVADOS (Comandos 14-18) - Preparados para futuro
    else if (comando >= 14 && comando <= 18) {
        double valor;
        switch (comando) {
            case 14: printf("Comando 14: RESERVADO (Sin implementar)\n");
                     write_DAC(dac_handle, 0.0); 
                     break;
            case 15: printf("Comando 15: RESERVADO (Sin implementar)\n");
                     valor = 1250.0;
                     write_DAC(dac_handle, valor); 
                     break;
            case 16: printf("Comando 16: RESERVADO (Sin implementar)\n");
                     write_DAC(dac_handle, 2500.0);
                     break;
            case 17: printf("Comando 17: RESERVADO (Sin implementar)\n");
                     valor = -2500.0;
                     write_DAC(dac_handle, valor);
                     break;
            case 18: printf("Comando 18: RESERVADO (Sin implementar)\n"); break;
        }
    }
    
    // ERROR
    else {
        printf("Comando desconocido: %d\n", comando);
    }
}

// Inicialización de la UART
void init_uart(void) {
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, BUF_SIZE, 0, NULL, 0);
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

// --- MAIN ---
void app_main(void)
{
    // Inicializar UART
    init_uart();
    config_pin(GREEN_LED, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_ENABLE, GPIO_INTR_DISABLE);
    MAX4737_Config_Pins();
    MAX4617_Config_Pins();
    gpio_set_level(MAX4737_CE_EN, HIGH);
    gpio_set_level(MAX4737_RE_FB_EN, HIGH);

    // --- INICIALIZACIÓN DAC MAX5217 ---
    ESP_LOGI(TAG, "Configurando DAC MAX5217...");
    uart_wait_tx_done(UART_NUM_0, 100); // Wait up to 1000ms
    dac_handle = setup_DAC(DAC1_DEVICE_ID); //
    if (dac_handle) {
        clear_DAC(dac_handle); // Pone el DAC en 2.048V inicial
    } else {
        ESP_LOGE(TAG, "Error inicializando DAC!");
    }

    // Inicializar SPI y ADS
    spi_device_handle_t ADS1255_SPI_HANDLER = init_spi_bus();

    my_ads.csPin = ADS1255_CS;
    my_ads.drdyPin = ADS1255_DRDY;
    my_ads.vref = ADS125X_VREF_2_5V;
    my_ads.oscFreq = ADS125X_OSC_FREQ_7_8M;

    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Inicializando ADS1255...");
    ADS125X_Init(&my_ads, ADS1255_SPI_HANDLER, ADS125X_DRATE_15SPS, ADS125X_PGA1, ADS125X_BUFOFF);
    vTaskDelay(pdMS_TO_TICKS(100)); // Pequeña espera
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
        if (dac_handle) {
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
            write_DAC(dac_handle, wave_volt);
        }

        // C. Parpadeo del LED
        ms_counter += 10; 
        if (ms_counter >= blink_period_ms) {
            led_state = !led_state;
            gpio_set_level(GREEN_LED, led_state);
            ms_counter = 0;
        }
    }
}