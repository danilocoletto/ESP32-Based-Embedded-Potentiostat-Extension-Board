/*******************************************************************************
 * @file        ads1255.c
 * @brief       C Library for ads1255/ads1256 family of Analog to Digital
 *              Conterters (ADC)
 * @details     This file implements the functionalities of the ADC.
 * @version     1.0
 * @author      Ing. Danilo Coletto Gallego
 * @date        04.12.2025
 * @copyright   (c) 2025  Universidad Nacional del Sur - CONICET
 * @see         
********************************************************************************
 * @note        Parts of this library are based on Simon Burkhardt STM32 Library 

MIT License

Copyright (c) 2020 Simon Burkhardt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "ads1255.h"


#define ADS_TIMEOUT_TABLE_SIZE                      (sizeof(ADS_TIMEOUT_TABLE)/sizeof(ADS125X_timeout_t))
#define ADS_PROPAGATION_DELAY_TABLE_SIZE            (sizeof(ADS_PROPAGATION_DELAY_TABLE)/sizeof(ADS125X_prop_delay_t))


/********************************************************************************/
/*                  Global Variables for ADC Functionalities                    */
/********************************************************************************/

static ADS125X_t ADS1255 = {
    .csPin = ADS1255_CS,
    .drdyPin = ADS1255_DRDY,
    .vref = ADS125X_VREF_2_5V,
    .oscFreq = ADS125X_OSC_FREQ_7_8M
};

// Tabla basada en el Datasheet del ADS1255/6
// T_settle es aproximadamente 4 / SPS para la primera muestra
// Tiempo en microsegundos, es T_settle mas un margen de seguridad del 25%
const ADS125X_timeout_t ADS_TIMEOUT_TABLE[] = {
    {ADS125X_DRATE_30000SPS, 320},    // 30000 SPS 
    {ADS125X_DRATE_15000SPS, 370},    // 15000 SPS
    {ADS125X_DRATE_7500SPS, 450},    // 7500 SPS
    {ADS125X_DRATE_3750SPS, 600},    // 3750 SPS
    {ADS125X_DRATE_2000SPS, 900},    // 2000 SPS
    {ADS125X_DRATE_1000SPS, 1550},    // 1000 SPS
    {ADS125X_DRATE_500SPS, 2750},   // 500 SPS
    {ADS125X_DRATE_100SPS, 12800},   // 100 SPS
    {ADS125X_DRATE_60SPS, 21100},   // 60 SPS
    {ADS125X_DRATE_50SPS, 25300},  // 50 SPS
    {ADS125X_DRATE_30SPS, 42000},  // 30 SPS
    {ADS125X_DRATE_25SPS, 50275},  // 25 SPS
    {ADS125X_DRATE_15SPS, 83600},  // 15 SPS
    {ADS125X_DRATE_10SPS, 125000}, // 10 SPS
    {ADS125X_DRATE_5SPS, 250000}, // 5 SPS
    {ADS125X_DRATE_2_5SPS, 500000}, // 2.5 SPS
};

const ADS125X_prop_delay_t ADS_PROPAGATION_DELAY_TABLE[] = {
    {ADS125X_DRATE_30000SPS, 250},    // 30000 SPS 
    {ADS125X_DRATE_15000SPS, 290},    // 15000 SPS
    {ADS125X_DRATE_7500SPS, 350},    // 7500 SPS
    {ADS125X_DRATE_3750SPS, 480},    // 3750 SPS
    {ADS125X_DRATE_2000SPS, 720},    // 2000 SPS
    {ADS125X_DRATE_1000SPS, 1220},    // 1000 SPS
    {ADS125X_DRATE_500SPS, 2220},   // 500 SPS
    {ADS125X_DRATE_100SPS, 10220},   // 100 SPS
    {ADS125X_DRATE_60SPS, 16880},   // 60 SPS
    {ADS125X_DRATE_50SPS, 20220},  // 50 SPS
    {ADS125X_DRATE_30SPS, 33550},  // 30 SPS
    {ADS125X_DRATE_25SPS, 40220},  // 25 SPS
    {ADS125X_DRATE_15SPS, 66880},  // 15 SPS
    {ADS125X_DRATE_10SPS, 100180}, // 10 SPS
    {ADS125X_DRATE_5SPS, 200220}, // 5 SPS
    {ADS125X_DRATE_2_5SPS, 400220}, // 2.5 SPS
};


// Asumiendo que usas SPI2 (HSPI). Si usas VSPI, cambia a &SPI3.
static volatile spi_dev_t *hw = &SPI2; 

//uint32_t propagation_delay_us = 0;

//static const char *TAG_INIT = "ADS125X";

/********************************************************************************/
/*                       Internal Functions for ADS1255                         */
/********************************************************************************/

/**
 * @brief Función para inicializar el Bus SPI
 */
spi_device_handle_t init_spi_bus(void)
{
    esp_err_t ret;
    spi_device_handle_t spi_handle;

    // Bus configuration (Physical pinout)
    spi_bus_config_t buscfg = {
        .miso_io_num = ADS1255_DOUT_MISO,
        .mosi_io_num = ADS1255_DIN_MOSI,
        .sclk_io_num = ADS1255_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,                                 // 0 = default (4092 bytes)
    };

    // Device configuration
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = CLOCK_SPEED_1_92MHZ,                // 1 MHz (Max ADS1255 ~1.92MHz with a 7.68M CLK)
        .mode = 1,                                            // SPI Mode 1 (CPOL=0, CPHA=1). According to datasheet for ADS125x
        .spics_io_num = -1,                                   // CS is mannually controlled by our library
        .queue_size = 1,
        .flags = 0,//SPI_DEVICE_HALFDUPLEX,                   // FULL DUPLEX COMMUNICATION
        .pre_cb = NULL,
        .post_cb = NULL,
    };

    // Inicializar el bus SPI
    ret = spi_bus_initialize(SPI_HOST_ID, &buscfg, DMA_CHAN);
    ESP_ERROR_CHECK(ret);

    // Añadir el dispositivo al bus
    ret = spi_bus_add_device(SPI_HOST_ID, &devcfg, &spi_handle);
    ESP_ERROR_CHECK(ret);

    return spi_handle;
}

/**
 * @brief This function initializes a clock for the ADS1255 using the ESP
 *          if the crystal in the board is not used.
 */
void init_adc_clock(uint32_t freq)
{
  // Configure clock generation
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_HIGH_SPEED_MODE,
      .timer_num = LEDC_TIMER_0,
      .duty_resolution = LEDC_TIMER_1_BIT,
      .freq_hz = freq,
      .clk_cfg = LEDC_AUTO_CLK};
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {
      .speed_mode = LEDC_HIGH_SPEED_MODE,
      .channel = LEDC_CHANNEL_0,
      .timer_sel = LEDC_TIMER_0,
      .intr_type = LEDC_INTR_DISABLE,
      .gpio_num = CLOCK_PIN,
      .duty = 1,
      .hpoint = 0};
  ledc_channel_config(&ledc_channel);
  ESP_LOGI("ADC_CLOCK_INITIALIZER", "8 MHz clock for ADS1255 configurated and launched");
}


/**
 * @brief This function configures the CS and DRDY pins for the ADS1255
 */
void ADS125X_Config_Pins(void)
{
  //configure CS and DRDY pins
  config_pin(ADS1255_CS, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE);
  gpio_set_level(ADS1255_CS, 1);
  config_pin(ADS1255_DRDY, GPIO_MODE_INPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE);
}


/**
 * @brief Obtiene el timeout en microsegundos basado en el DRATE actual
 */
int32_t ADS125X_Get_Timeout_Us(uint8_t current_drate) 
{
  for (int i = 0; i < ADS_TIMEOUT_TABLE_SIZE; i++) 
  {
    if (ADS_TIMEOUT_TABLE[i].reg_val == current_drate) 
    {
      // Retornamos en microsegundos
      return (int32_t)ADS_TIMEOUT_TABLE[i].timeout_us;
    }
  }

  return (int32_t) 500000; // Default: 500 ms si no se encuentra el valor
}

/********************************************************************************/
/*                            Functions for ADS1255                             */
/********************************************************************************/


/**
  * @brief  resets, initializes and calibrates the ADS125X
  * @param  *ads pointer to ads handle
  * @param  drate the datarate of the converter
  * @param  gain  the gain of the PGA
  * @param  buffer_en [0 = off, 1 = on]
  * @see    Datasheet Fig. 33 SDATAC Command Sequence
  */
void ADS125X_Init(ADS125X_t *ads, uint8_t drate, uint8_t gain, uint8_t buffer_en)
{
  bool found = false;

  ads->hspix = init_spi_bus();      // Inicializar SPI y ADS
  ads->pga = 1 << gain;
  ads->current_drate = drate;

#ifdef GEN_CLOCK
  init_adc_clock(ads->oscFreq);
#endif

  ADS125X_Config_Pins();
  ADS125X_CMD_Send(ads, ADS125X_CMD_RESET);
  // This freeze the CPU for 5ms.
  esp_rom_delay_us(5000);
  ADS125X_CMD_Send(ads, ADS125X_CMD_SDATAC);

#ifdef DEBUG_ADS1255
  uint8_t tmp[5]; // buffer
  ADS125X_Register_Read(ads, ADS125X_REG_STATUS, tmp, 1);
  printf("STATUS: %#.2x\n", tmp[0]);
#endif

  // enable clockout | ADS125X_PGA1
  ADS125X_Register_Write(ads, ADS125X_REG_ADCON, ADS125X_CLKOUT_1 | gain); // enable clockout = clkin/1
#ifdef DEBUG_ADS1255
  ADS125X_Register_Read(ads, ADS125X_REG_ADCON, tmp, 1);
  printf("ADCON: %#.2x\n", tmp[0]);
#endif

  ADS125X_Register_Write(ads, ADS125X_REG_DRATE, drate);
#ifdef DEBUG_ADS1255
  ADS125X_Register_Read(ads, ADS125X_REG_DRATE, tmp, 1);
  printf("DRATE: %#.2x\n", tmp[0]);
#endif

  ADS125X_Register_Write(ads, ADS125X_REG_MUX, ADS125X_MUXP_AIN0 | ADS125X_MUXN_AINCOM); // mux, medir ADC
#ifdef DEBUG_ADS1255
  ADS125X_Register_Read(ads, ADS125X_REG_IO, tmp, 1);
  printf("IO   : %#.2x\n", tmp[0]);
#endif

  ADS125X_Register_Write(ads, ADS125X_REG_IO, 0x00); // all GPIOs are outputs (do not leave floating) - D0 is CLKOUT
#ifdef DEBUG_ADS1255
  ADS125X_Register_Read(ads, ADS125X_REG_IO, tmp, 1);
  printf("IO   : %#.2x\n", tmp[0]);
#endif

  ADS125X_CMD_Send(ads, ADS125X_CMD_SELFCAL);
  ADS125X_DRDY_Wait(ads); // wait ADS1256 to settle after self calibration

  // This propagation delay store in microseconds corresponds to the Settling Time (T18)
  // descripted in the Table 13 of the ADS1255 datasheet.
  for (int i = 0; i < ADS_PROPAGATION_DELAY_TABLE_SIZE; i++) 
  {
    if (ADS_PROPAGATION_DELAY_TABLE[i].reg_val == drate) 
    {
      ads->propagation_delay_us = ADS_PROPAGATION_DELAY_TABLE[i].propagation_delay_us;
      found = true;
      break;
    }
  }

  if (!found)
    printf("#ERR: Invalid ADC data rate specified.\n");

  return;
}


/**
  * @brief  converts an <int32_t> array of 2's complement code to <float> array of voltage
  * @param  *ads pointer to ads handle
  * @param  *pCode pointer to input array
  * @param  *pVolt pointer to output array
  * @param  size the length of the array
  * @see    Datasheet Table 16. Ideal Ouput Code vs. Input Signal
  */
void ADS125X_ADC_Code2Volt (ADS125X_t *ads, int32_t *pCode, float *pVolt, uint16_t size)
{
  uint16_t i;
  // Pre-calculate the scale factor to not do division in each iteration (faster)
  // Factor = (2 * Vref) / (PGA * MaxCode)
  float scale_factor = (2.0f * ads->vref) / ( (float)ads->pga * 8388607.0f );

  for(i = 0; i < size; i++)
  {
    // Sign extension from 24-bit to 32-bit
    if(pCode[i] & 0x800000)
    {
       pCode[i] |= 0xFF000000;  // Converts 0x00FFFFFF into 0xFFFFFFFF (-1)
    }

    // Conversion to volts using a multiplication (faster than division)
    pVolt[i] = (float)pCode[i] * scale_factor;
  }
}

/**
  * @brief  read an analog voltage from the currently selected channel
  * @param  *ads pointer to ads handle
  * @return <float> voltage value on analog input
  * @see    Datasheet Fig. 30 RDATA Command Sequence
  */
float ADS125X_ADC_ReadVolt(ADS125X_t *ads)
{
  uint8_t cmd = ADS125X_CMD_RDATA;
  uint8_t spiRx[3] = {0, 0, 0};
  uint8_t spiTxDummy[3] = {0xFF, 0xFF, 0xFF};
  esp_err_t ret;

  //Send transaction (Command)
  spi_transaction_t t_cmd;
  memset(&t_cmd, 0, sizeof(t_cmd));
  t_cmd.length = 8;             // 8 bits
  t_cmd.tx_buffer = &cmd;
  t_cmd.rx_buffer = NULL;       
    
  //Receive transaction (Data)
  spi_transaction_t t_read;
  memset(&t_read, 0, sizeof(t_read));
  t_read.length = 24;           // 24 bits
  t_read.rxlength = 24;
  t_read.tx_buffer = spiTxDummy;      
  t_read.rx_buffer = spiRx;

	// --- Communication Init ---
  ADS125X_CS(ads, ADS125X_ON); // CS Low (Select)

  // Send command RDTA. We use 'spi_device_trasnmit'
  //ret = spi_device_transmit(ads->hspix, &t_cmd);
  ret = spi_device_polling_transmit(ads->hspix, &t_cmd);
  if (ret != ESP_OK) 
  {
    ADS125X_CS(ads, ADS125X_OFF);
    return 0.0f; 
  }

  // Delay (t6 in datasheet ~7us). Figure 30.
  //esp_rom_delay_us(7);

  // Read 3 bytes
  //ret = spi_device_transmit(ads->hspix, &t_read);
  ret = spi_device_polling_transmit(ads->hspix, &t_read);

  ADS125X_CS(ads, ADS125X_OFF); // CS High (Deselect)
  // --- COMMUNICATION END ---

#ifdef DEBUG_ADS1255
    printf("RDATA: %#.2x%.2x%.2x\n", spiRx[0], spiRx[1], spiRx[2]);
#endif

  // --- CONVERSION AND MATH ---
    
  // Build the int32 (Big Endian)
  int32_t adsCode = (spiRx[0] << 16) | (spiRx[1] << 8) | (spiRx[2]);

  // Sign extension (Bit 23 is the sign)
  if(adsCode & 0x800000) 
  {
    adsCode |= 0xFF000000;
  }

  // Vout = (Code * 2 * Vref) / (PGA * 2^23 - 1) = PGA * 0x7fffff = 8388607.0f
  return ( (float)adsCode * (2.0f * ads->vref) ) / ( (float)ads->pga * 8388607.0f );
}


/**
 * @brief Ultrafast read contrilling the SPI operation directly with registers (HAL).
 * @param ads *ads pointer to ads handle
 */
float ADS125X_ADCFast_ReadVolt_HAL(ADS125X_t *ads)
{

    // 1. ADQUIRIR EL BUS (Pausa el driver de ESP-IDF para que no interfiera)
    // Si eres el único dispositivo en el bus, puedes mover esto al inicio del programa
    spi_device_acquire_bus(ads->hspix, portMAX_DELAY);

    // 2. BAJAR CHIP SELECT (Manual)
    ADS125X_CS(ads, ADS125X_ON); 

    // --- FASE 1: ENVIAR COMANDO (RDATA: 0x01) ---
    
    // Configurar registros para TX (MOSI)
    hw->user.usr_mosi = 1;      // Habilitar salida MOSI
    hw->user.usr_miso = 0;      // Deshabilitar entrada MISO
    hw->user.usr_dummy = 0;     // Sin dummy bits por hardware
    hw->user.doutdin = 0;       // Half-duplex (primero sale, luego entra)
    
    // Configurar longitud de bits (N-1)
    // Queremos 8 bits. Registro espera valor 7.
    hw->mosi_dlen.usr_mosi_dbitlen = 7; 
    
    // Cargar datos en el buffer W0 (El ESP32 es Little Endian, el registro W0 es de 32 bits)
    // Para enviar 0x01 (8 bits), simplemente escribimos en el byte más bajo.
    hw->data_buf[0] = 0x01; 

    // Disparar la transferencia (Bit USR)
    hw->cmd.usr = 1;

    // Esperar a que termine (Polling directo al registro, ~8us a 2MHz)
    while (hw->cmd.usr);

    // --- FASE 2: TIEMPO DE ESPERA (t6 = 7us) ---
    // Aquí el reloj está detenido y el CS sigue bajo. Perfecto.
    esp_rom_delay_us(7);

    // --- FASE 3: LEER DATOS (24 Bits) ---

    // Configurar registros para RX (MISO)
    hw->user.usr_mosi = 0;      // Deshabilitar MOSI
    hw->user.usr_miso = 1;      // Habilitar MISO
    
    // Configurar longitud de lectura (N-1)
    // Queremos 24 bits. Registro espera 23.
    hw->miso_dlen.usr_miso_dbitlen = 23;

    // Disparar la transferencia
    hw->cmd.usr = 1;

    // Esperar a que termine
    while (hw->cmd.usr);

    // Leer el buffer W0
    // El hardware del ESP32 rellena W0. 
    // Nota: El orden de los bytes depende de cómo el hardware llena W0.
    // Normalmente para < 32 bits, los datos quedan alineados.
    // ADS envía: [MSB] [MID] [LSB]. 
    // ESP32 W0 (Little Endian) típicamente contendrá: 0x00[LSB][MID][MSB] o similar.
    // Para 24 bits, solemos necesitar invertir el orden de bytes (swap).
    
    // Reconstrucción manual para asegurar Endianness correcto:
    // W0 se llena byte a byte. W0[0] es el primer byte recibido (MSB del ADS).
    // W0[1] es el segundo...
    // Como data_buf es uint32_t*, al leerlo en una CPU Little Endian, los bytes se invierten.
    // Usamos __builtin_bswap32 para invertirlo rápidamente.
    
    // Ajuste para 24 bits: bswap32 convierte ABCD -> DCBA.
    // Nosotros recibimos 3 bytes: A B C (MSB MID LSB).
    // En W0 (raw) se verán como 0x??CBA (Little Endian interpreta el primer byte recibido como el menos significativo).
    // Entonces raw_data & 0xFF es el MSB real (Byte A).
    
    // FORMA SEGURA (Independiente del SWAP de hardware):
    // Accedemos al buffer como array de bytes para no liarnos con el Little Endian de la CPU.
    uint8_t *bytes = (uint8_t *)&hw->data_buf[0];

    // 3. SUBIR CHIP SELECT
    ADS125X_CS(ads, ADS125X_OFF); // CS High (Deselect)

    // 4. LIBERAR BUS
    spi_device_release_bus(ads->hspix);

  // Build the int32 (Big Endian)
  int32_t adsCode = (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];

  // Sign extension (Bit 23 is the sign)
  if(adsCode & 0x800000) 
  {
    adsCode |= 0xFF000000;
  }

  // Vout = (Code * 2 * Vref) / (PGA * 2^23 - 1) = PGA * 0x7fffff = 8388607.0f
  return ( (float)adsCode * (2.0f * ads->vref) ) / ( (float)ads->pga * 8388607.0f );
}

/**
 * @brief  Reads from internal registers sequentially.
 * @details
 * Sends the RREG (Read Register) command to initiate the read operation. This function 
 * automatically handles the ADS125x two-byte protocol:
 * 1. Command Byte: (0x10 | Start Address).
 * 2. Count Byte:   (Number of registers to read - 1).
 * * It also handles the critical timing delay (t6 ~6.5us) between command transmission 
 * and data reception as specified in the datasheet.
 *
 * @param  *ads   Pointer to the ADS125X handle structure.
 * @param  reg    Address of the FIRST register to be read (e.g., ADS125X_REG_STATUS).
 * @param  *pData Pointer to the buffer where the read data will be stored.
 * WARNING: Ensure the buffer size is at least 'n' bytes.
 * @param  n      Number of registers to read.
 * - If n=1: Reads a single register.
 * - If n>1: Reads 'n' consecutive registers (Burst Read).
 * @return 0 on success.
 * @see    Datasheet Fig. 34: RREG Command Example and Table 23 (Command Definitions).
 */
uint8_t ADS125X_Register_Read(ADS125X_t *ads, uint8_t reg, uint8_t* pData, uint8_t n)
{

  //Preparar el buffer de transmisión (Comando RREG)
    uint8_t spiTx[2];
    uint8_t spiTxDummy[1] = {0xFF};
    spiTx[0] = ADS125X_CMD_RREG | reg; // 1er byte: Comando | Dirección
    spiTx[1] = n - 1;                  // 2do byte: (Número de bytes a leer) - 1

    //Definir la transacción de ESCRITURA (Comando)
    spi_transaction_t t_cmd;
    memset(&t_cmd, 0, sizeof(t_cmd));
    t_cmd.length = 16;             // 2 bytes * 8 bits
    t_cmd.tx_buffer = spiTx;
    t_cmd.rx_buffer = NULL;

    //Definir la transacción de LECTURA (Datos)
    spi_transaction_t t_read;
    memset(&t_read, 0, sizeof(t_read));
    t_read.length = n * 8;         // n bytes * 8 bits
    t_read.rxlength = n * 8;
    t_read.tx_buffer = spiTxDummy;
    t_read.rx_buffer = pData;

    //ADS125X_CMD_Send(ads, ADS125X_CMD_SDATAC);

    // --- INICIO SECUENCIA ---
    ADS125X_CS(ads, ADS125X_ON); // Select

    // Esperar a que DRDY esté bajo (requerido por datasheet antes de enviar comandos si hay conversión activa)
    ADS125X_DRDY_Wait(ads);

    //Enviar comando (RREG + Count)
    // Usamos transmit (interrupción/bloqueo de tarea)
    spi_device_polling_transmit(ads->hspix, &t_cmd);
    //spi_device_transmit(ads->hspix, &t_cmd);

    // Delay t6 (Delay from Last SCLK to First SCLK of Read)
    // Datasheet: 50 * tCLKIN. Para 8MHz -> ~6.25us. Usamos 7us.
    esp_rom_delay_us(7);

    //Leer los datos del registro
    spi_device_polling_transmit(ads->hspix, &t_read);
    //spi_device_transmit(ads->hspix, &t_read);

    // Delay t11 (Delay Command Decode) - Opcional pero recomendado
    // Datasheet: 4 * tCLKIN (~0.5us). Ponemos 1us por seguridad antes de subir el CS.
    esp_rom_delay_us(1);

    ADS125X_CS(ads, ADS125X_OFF); // Deselect
    // --- FIN SECUENCIA ---

    return 0;
}

/**
 * @brief  Writes a single value to a specific internal register.
 * @details
 * This function is hardcoded to write exactly ONE register at a time.
 * It sends the WREG command followed by the data payload in a single SPI transaction:
 * 1. Command Byte: (0x50 | Register Address).
 * 2. Count Byte:   (0x00) -> Indicates 1 byte payload (n-1).
 * 3. Data Byte:    The value to write.
 * * @param  *ads   Pointer to the ADS125X handle structure.
 * @param  reg    Address of the register to write to.
 * @param  data   The 8-bit value to write into the register.
 * @return 0 on success.
 * @see    Datasheet Fig. 35: WREG Command Example (t11 delay required after command).
 */
uint8_t ADS125X_Register_Write(ADS125X_t *ads, uint8_t reg, uint8_t data)
{

  // Prepare the 3-byte packet: [CMD | COUNT | DATA]
  uint8_t spiTx[3];
  spiTx[0] = ADS125X_CMD_WREG | reg; // 1st byte: Command + Register Address
  spiTx[1] = 0;                      // 2nd byte: (Number of bytes - 1). 0 means 1 byte.
  spiTx[2] = data;                   // 3rd byte: The payload

  // Define the SPI transaction
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = 24;          // 3 bytes * 8 bits
  t.tx_buffer = spiTx;    // Pointer to data
  t.rx_buffer = NULL;     // We don't expect a response

  // --- START COMMUNICATION ---
  ADS125X_CS(ads, ADS125X_ON); // Select

  // Wait for DRDY to go low before sending WREG (Required by datasheet)
  ADS125X_DRDY_Wait(ads);

  // Send the packet
  spi_device_polling_transmit(ads->hspix, &t);

  // Delay t11 (Command Decode Delay). Datasheet requires 4 * tCLKIN (~0.5us). We use 1us for safety.
  esp_rom_delay_us(1);

  ADS125X_CS(ads, ADS125X_OFF); // Deselect
  // --- END COMMUNICATION ---

  return 0;
}

/**
 * @brief  Sends a single control command to the ADS125X.
 * @details
 * This function handles the standard command sequence:
 * 1. Lowers CS (Chip Select).
 * 2. Waits for DRDY to go low (ensuring the chip is ready to receive).
 * 3. Sends the 8-bit command (e.g., SYNC, RESET, SELFOCAL).
 * 4. Waits for the Command Decode delay (t11).
 * 5. Raises CS.
 * * @param  *ads   Pointer to the ADS125X handle structure.
 * @param  cmd    The 8-bit command to send (e.g., ADS125X_CMD_SDATAC).
 * @return 0 on success.
 * @see    Datasheet Fig. 33: SDATAC Command Sequence and Table 24 (Command Definitions).
 */
uint8_t ADS125X_CMD_Send(ADS125X_t *ads, uint8_t cmd)
{
    // Define the SPI transaction
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;               // 8 bits
    t.tx_buffer = &cmd;         // Pointer to the command byte
    t.rx_buffer = NULL;         // No response expected

    // --- START COMMUNICATION ---
    ADS125X_CS(ads, ADS125X_ON); // Select

    // Wait for DRDY (Required only before sending commands like SDATAC or RDATAC)
    if( cmd == ADS125X_CMD_RDATAC || cmd == ADS125X_CMD_SDATAC)
      ADS125X_DRDY_Wait(ads);

    // Send the command (Blocking/Yielding)
    spi_device_polling_transmit(ads->hspix, &t);

    // Delay t11 (Command Decode Delay). Minimal delay required is 4 * tCLKIN (~0.5us). We wait 1us to be safe.
    esp_rom_delay_us(1);

    ADS125X_CS(ads, ADS125X_OFF); // Deselect
    // --- END COMMUNICATION ---

    return 0;
}

/**
 * @brief  Sets the internal multiplexer to a differential input pair.
 * @details
 * Writes to the MUX register (Address 0x01) to select the Positive (AINp) 
 * and Negative (AINn) inputs.
 * After changing the MUX, it sends SYNC and WAKEUP to restart the conversion
 * cycle immediately, ensuring the digital filter settles on the new channel.
 *
 * @param  *ads   Pointer to ADS125X handle.
 * @param  p_chan Positive input channel definition (e.g., ADS125X_MUXP_AIN0).
 * @param  n_chan Negative input channel definition (e.g., ADS125X_MUXN_AIN1).
 * @return 0 on success.
 * @see    Datasheet p. 31 MUX Register and Cycling the ADS1256.
 */
uint8_t ADS125X_ChannelDiff_Set(ADS125X_t *ads, int8_t p_chan, int8_t n_chan)
{
  // CÁLCULO DEL VALOR MUX
  uint8_t mux_val = p_chan | n_chan;

  // SI usas índices crudos (0, 1, 2...), DEBERÍAS usar la línea comentada:
  // uint8_t mux_val = ((p_chan << 4) & 0xF0) | (n_chan & 0x0F);

  //Escribir el nuevo MUX
  ADS125X_Register_Write(ads, ADS125X_REG_MUX, mux_val);

  // Reiniciar la conversión (SYNC + WAKEUP)
  // Esto es crucial. Si cambias el canal sin resetear, la lectura actual 
  // será una mezcla corrupta del canal anterior y el nuevo (settling time).
  ADS125X_CMD_Send(ads, ADS125X_CMD_SYNC);
  ADS125X_CMD_Send(ads, ADS125X_CMD_WAKEUP);

#ifdef DEBUG_ADS1255
  uint8_t tmp = 0;
  // Leemos para verificar que se escribió bien
  ADS125X_Register_Read(ads, ADS125X_REG_MUX, &tmp, 1);
  printf("MUX Set: %#.2x (Requested: %#.2x)\n", tmp, mux_val);
#endif

  return 0;
}



/**
  * @brief  waits for DRDY pin to go low
  * @param  *ads pointer to ads handle
  */
uint8_t ADS125X_DRDY_Wait(ADS125X_t *ads) {
    // Timeout se selecciona automaticamente dependiendo del Settle Time del ADS1255 
    int32_t timeout_us = ADS125X_Get_Timeout_Us(ads->current_drate);
    int64_t t_start = esp_timer_get_time();     // Marca de tiempo (µs)
    while (gpio_get_level(ads->drdyPin) == 1) {
        if((esp_timer_get_time() - t_start) >= timeout_us) {
            ESP_LOGE("ADS125X", "Error: DRDY Timeout. El sensor no responde.");
            return 1; // Error
        }
    }
    return 0; // Éxito
}

/**
  * @brief  toggles chip select pin of ADS
  * @param  *ads pointer to ads handle
  * @param  state [OFF = 0 = unselect, ON = 1 = select]     // ACTIVE LOW CHIP SELECT - INVERTED LOGIC HERE TO EASIER COMPREHENSION
  */
uint8_t ADS125X_CS(ADS125X_t *ads, uint8_t state)
{
    char cs_state = 0;

    if(state) 
        cs_state = 0;
    else 
        cs_state= 1;

    gpio_set_level(ads->csPin, cs_state);
	return 0;
}


/**
  * @brief  set internal multiplexer to single channel with AINCOM as negative input
  * @param  *ads pointer to ads handle
  * @param  p_chan positive analog input
  * @see    Datasheet p. 31 MUX : Input Multiplexer Control Register (Address 01h)
  */
void ADS125X_Channel_Set(ADS125X_t *ads, int8_t channel)
{ 
  ADS125X_ChannelDiff_Set(ads, channel, ADS125X_MUXN_AINCOM); 
}


/**
 * @brief Reads the current from the Transimpedance Amplifier (TIA) connected to the ADC.
 *
 * This function switches the ADS1255 input multiplexer to the current sensing channel 
 * (AIN1 - AINCOM), reads the voltage, and calculates the current based on the 
 * active gain of the TIA.
 *
 * @note **Optimization:** To maximize sampling speed, this function checks the currently 
 * active MUX channel stored in the handle. It only writes to the ADC MUX register 
 * if a channel switch is strictly necessary. This avoids the filter settling 
 * delay (approx 5/DataRate) on repeated reads of the same channel.
 *
 * @param[in,out] ads Pointer to the ADS125X handle structure. 
 *
 * @return float The calculated current in Amperes (A).
 */
float ADS125X_ADC_ReadCurrent(ADS125X_t *ads)
{
  int gain = MAX4617_get_gain();
  float potential = 0.0;
  float current = 0.0;
  uint8_t reg_val = 0;
        
  ADS125X_Register_Read(ads, ADS125X_REG_MUX, &reg_val, 1);
  if(reg_val != (ADS125X_MUXP_AIN0 | ADS125X_MUXN_AINCOM))
    ADS125X_Register_Write(ads, ADS125X_REG_MUX, ADS125X_MUXP_AIN0 | ADS125X_MUXN_AINCOM); // We READ AIN1 - AINCOM - THIS MEAN AT THE OUTPUT OF THE TRANSIMPEDANCE AMPLIFIER

  potential = ADS125X_ADC_ReadVolt(ads);

  current = (potential / gain);
  
  return current;
}


/**
 * @brief Returns the value of propagation_delay_s
 *
 */
float ADS125X_Get_Prop_Delay_Us(void)
{
  return ADS1255.propagation_delay_us;
}

/********************************************************************************/
/*                       HAL Layer Functions for ADS1255                        */
/********************************************************************************/


void ADS125X_INIT_HAL (uint8_t drate, uint8_t gain, uint8_t buffer_en)
{
  ADS125X_Init(&ADS1255, drate, gain, buffer_en);
}

void  ADS125X_WAKEUP_HAL (void)
{
  ADS125X_CMD_Send(&ADS1255, ADS125X_CMD_WAKEUP);
}

void  ADS125X_STANDBY_HAL (void)
{
  ADS125X_CMD_Send(&ADS1255, ADS125X_CMD_STANDBY);
}

void  ADS125X_SDATAC_HAL (void)
{
  ADS125X_CMD_Send(&ADS1255, ADS125X_CMD_SDATAC);
}

void  ADS125X_CMD_SEND_HAL (uint8_t cmd)
{
  ADS125X_CMD_Send(&ADS1255, cmd);
}

float ADS125X_READVOLT_HAL (void)
{
  return ADS125X_ADCFast_ReadVolt_HAL(&ADS1255);
}

uint8_t ADS125X_READ_REG_HAL (uint8_t reg, uint8_t* pData, uint8_t n)
{
  return ADS125X_Register_Read(&ADS1255, reg, pData, n);
}

void ADS125X_WRITE_REG_HAL (uint8_t reg, uint8_t data)
{
  ADS125X_Register_Write(&ADS1255, reg, data);
}

void ADS125X_WAIT_DYDR_HAL (void)
{
  ADS125X_DRDY_Wait(&ADS1255);
}