/*******************************************************************************
 * @file        max5217.c
 * @brief       C Library for MAX5217 Digital to Analog Converter
 * @details     This file implements the functionalities of the DAC.
 * @version     1.0
 * @author      Ing. Danilo Coletto Gallego
 * @date        08.12.2025
 * @copyright   (c) 2025  Universidad Nacional del Sur - CONICET
 * @see         
******************************************************************************** 

MIT License
*/

#include "max5217.h"

static const char *TAG = "DAC_SETUP";

// Global static variable to keep the bus handle
static i2c_master_bus_handle_t bus_handle = NULL;

/**
 * @brief Configures the I2C DAC.
 * @param devID I2C device address (e.g., 0x48).
 * @return i2c_master_dev_handle_t Device handle, or NULL on failure.
 */
i2c_master_dev_handle_t setup_DAC(uint8_t devID)
{
    config_pin(MAX_5217_NAUX, GPIO_MODE_OUTPUT, GPIO_PULLUP_ENABLE, GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE);
    gpio_set_level(MAX_5217_NAUX, HIGH);    // Lo deshabilito

    // Initialize the I2C BUS (just if it hadn't been done before)
    if (bus_handle == NULL) {
        i2c_master_bus_config_t i2c_mst_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = -1,                                     // Let the driver select the port automatically
            .scl_io_num = MAX_5217_SCL,
            .sda_io_num = MAX_5217_SDA,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,              // Disable internal pullups
        };

        esp_err_t ret = i2c_new_master_bus(&i2c_mst_config, &bus_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Fallo al inicializar el bus I2C master");
            return NULL;
        }
    }

    // Configurate the device ( DAC - MAX5217)
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = devID,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    i2c_master_dev_handle_t dac_handle;
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dac_handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al inicializar la comunicación I2C con DAC (ID: 0x%02x)", devID);
        return NULL;
    }

    ESP_LOGI(TAG, "Comunicación I2C con DAC configurada exitosamente.");

    /* --- SOBRE LA PARTE COMENTADA ---
       wiringPiI2CWriteReg16(DAC_prep, MAX5217_CMD_USER_CONFIG, (0 | 32));
       
       En ESP-IDF v5, para escribir harías algo así:
       
       uint8_t data_to_send[] = { MAX5217_CMD_USER_CONFIG, (0 | 32) }; // Ojo con el orden de bytes (Endianness)
       i2c_master_transmit(dac_handle, data_to_send, sizeof(data_to_send), -1);
    */
    //i2c_scanner(bus_handle);
    // Retornamos el handle del dispositivo en lugar de un int (file descriptor)
    return dac_handle; 
}


/**
 * @brief Converts a voltage value to DAC counts and writes it to the device via I2C.
 *
 * This function converts the input voltage (mV) into a 16-bit raw value based on the 
 * reference voltage. It explicitly swaps the MSB and LSB to match the required 
 * endianness for the transmission protocol before sending the command to the MAX5217.
 *
 * @param dac_handle Handle to the I2C DAC device (ESP-IDF driver).
 * @param mvolt_value Target output voltage (voltage values goes between -2500 mV and 2500 mV) in millivolts. 
 */
void write_DAC(i2c_master_dev_handle_t dac_handle, double mvolt_value)
{
    // 1. Protección de Handle nulo
    if (dac_handle == NULL) return;

    // 2. CLAMPING: Protección de rangos (CRÍTICO)
    // Esto evita el "Integer Underflow". Si mvolt_value llega como -2500.000001,
    // la suma daría negativo y al pasarlo a unsigned int se iría al máximo (65535).
    // Aquí lo forzamos a mantenerse dentro de los límites seguros.
    if (mvolt_value < -2500.0) mvolt_value = -2500.0;
    if (mvolt_value > 2500.0)  mvolt_value = 2500.0;

    unsigned int valor_DAC = 0;
    int msg = 0;

    // 3. Cálculo con REDONDEO
    // Se suma +0.5 antes del cast (int) para redondear al entero más cercano 
    // en lugar de truncar decimales (función piso).
    double raw_val = ((mvolt_value + (double) DAC_REF_2_5V) * (double) MAX_VALUE_DAC) / (double) DAC_REF_5V;
    valor_DAC = (unsigned int)(raw_val + 0.5);

    // 4. Swap de Endianness (Lógica original preservada)
    // Intercambio de lugar de los 8 bits menos significativos con los 8 mas significativos.
    msg = (((valor_DAC & 0x00FF) << 8) | ((valor_DAC & 0xFF00) >> 8));
    
    // 5. Preparación del Buffer
    uint8_t write_buffer[3];
    write_buffer[0] = MAX5217_CMD_CODE_LOAD;       // Comando
    write_buffer[1] = (uint8_t)(msg & 0xFF);       // Byte Alto (gracias al swap)
    write_buffer[2] = (uint8_t)((msg >> 8) & 0xFF); // Byte Bajo

    // 6. Transmisión I2C Segura (Con Timeout)
    // Usamos un timeout de 50ms para no bloquear el sistema si hay un error eléctrico momentáneo.
    //esp_err_t err = i2c_master_transmit(dac_handle, write_buffer, sizeof(write_buffer), 50 / portTICK_PERIOD_MS);
    esp_err_t err = i2c_master_transmit(dac_handle, write_buffer, sizeof(write_buffer), -1);
    
    if (err != ESP_OK) {
        // Solo reportamos el error, NO abortamos ni reiniciamos el ESP32
        ESP_LOGE(TAG, "I2C Write Failed: %s", esp_err_to_name(err));
    }
}

 /*
void write_DAC(i2c_master_dev_handle_t dac_handle, double mvolt_value)
{
    int valor_DAC = 0;
    int msg = 0;
    

    // Logic to calculate the DAC value depending on the desire voltage in milivolts
    valor_DAC = (unsigned int)(((mvolt_value + (double) DAC_REF_2_5V) * MAX_VALUE_DAC) / DAC_REF_5V);
    

    // Lógica Original Preservada (Movimiento de bits)

    // Intercambio de lugar de los 8 bits menos significativos con los 8 mas significativos.
    // Si valor_DAC era 0xAABB, msg será 0xBBAA.
    msg = (((valor_DAC & 0x00FF)<<8) | ((valor_DAC & 0xFF00)>>8));
    uint8_t write_buffer[3];
    
    // Byte 0: El registro de comando (Command Byte)
    write_buffer[0] = MAX5217_CMD_CODE_LOAD;
    
    // Byte 1: La parte baja de 'msg' (que gracias a tu swap, contiene el byte ALTO original)
    write_buffer[1] = (uint8_t)(msg & 0xFF);

    //printf("My Byte: 0x%02X\n", write_buffer[1]);
    
    // Byte 2: La parte alta de 'msg' (que gracias a tu swap, contiene el byte BAJO original)
    write_buffer[2] = (uint8_t)((msg >> 8) & 0xFF);

    // %02X means: Print in Hex (X), minimum 2 digits (2), pad with zeros (0)
    //printf("My Byte: 0x%02X\n", write_buffer[2]);

    //ESP_ERROR_CHECK(i2c_master_transmit(dac_handle, write_buffer, sizeof(write_buffer), -1));

    // --- NEW CODE (Prevents Reset) ---
    esp_err_t err = i2c_master_transmit(dac_handle, write_buffer, sizeof(write_buffer), -1);
    
    if (err != ESP_OK) {
        // Just print the error, do NOT abort/reset
        ESP_LOGE(TAG, "I2C Write Failed: %s", esp_err_to_name(err));
    }
}
*/

/**
 * @brief Resets the DAC output to a specific baseline voltage.
 * * This function "clears" the DAC by forcing it to a default voltage level 
 * of 2.048V using the standard write routine. 
 *
 * @param dac_handle Handle to the I2C DAC device.
 */
void clear_DAC(i2c_master_dev_handle_t dac_handle)
{
    // Usamos la función write_DAC que acabamos de portar
    write_DAC(dac_handle, 2500);

    /* 
       uint8_t clear_cmd[3] = { MAX5217_CMD_SW_CLEAR, 0x00, 0x00 };
       ESP_ERROR_CHECK(i2c_master_transmit(dac_handle, clear_cmd, sizeof(clear_cmd), -1));
    */
}


void i2c_scanner(i2c_master_bus_handle_t bus_handle) {
    printf("--- I2C SCANNER START ---\n");
    printf("Scanning I2C bus...\n");
    
    int devices_found = 0;
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        // In ESP-IDF v5, we use i2c_master_probe to check for a device
        esp_err_t ret = i2c_master_probe(bus_handle, addr, 50); // 50ms timeout
        
        if (ret == ESP_OK) {
            printf(" -> Device found at address: 0x%02X\n", addr);
            devices_found++;
        }
    }
    
    if (devices_found == 0) {
        printf(" -> NO DEVICES FOUND. Check wiring/pull-ups.\n");
    } else {
        printf("--- SCAN COMPLETE: Found %d device(s) ---\n", devices_found);
    }
}