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

//#define DEBUG_ADS1255
#define GEN_CLOCK

#ifdef GEN_CLOCK
  #define CLOCK_PIN 16 
#endif

#define ADS1255_DOUT_MISO       GPIO_NUM_19
#define ADS1255_DIN_MOSI        GPIO_NUM_23
#define ADS1255_SCLK            GPIO_NUM_18
#define ADS1255_CS              GPIO_NUM_5
#define ADS1255_DRDY            GPIO_NUM_4

//static const char *TAG_INIT = "ADS125X";

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
        .max_transfer_sz = 0, // 0 = default (4092 bytes)
    };

    // Device configuration
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = CLOCK_SPEED_1MHZ,   // 1 MHz (Max ADS1255 ~1.92MHz with a 7.68M CLK)
        .mode = 1,                   // SPI Mode 1 (CPOL=0, CPHA=1). According to datasheet for ADS125x
        .spics_io_num = -1,          // CS is mannually controlled by our library
        .queue_size = 1,
        .flags = 0,                  // FULL DUPLEX COMMUNICATION
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
 * @brief This function initializes an 8 MHz clock for the ADS1255 using the ESP
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
  * @brief  resets, initializes and calibrates the ADS125X
  * @param  *ads pointer to ads handle
  * @param  drate the datarate of the converter
  * @param  gain  the gain of the PGA
  * @param  buffer_en [0 = off, 1 = on]
  * @return 
  * @see    Datasheet Fig. 33 SDATAC Command Sequence
  */
uint8_t ADS125X_Init(ADS125X_t *ads, spi_device_handle_t hspi, uint8_t drate, uint8_t gain, uint8_t buffer_en)
{
  ads->hspix = hspi;
  ads->pga = 1 << gain;

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

  return 0;
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
float ADS125X_ADC_ReadVolt (ADS125X_t *ads)
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
  ret = spi_device_transmit(ads->hspix, &t_cmd);
  if (ret != ESP_OK) 
  {
    ADS125X_CS(ads, ADS125X_OFF);
    return 0.0f; 
  }

  // Delay (t6 in datasheet ~7us). Figure 30.
  esp_rom_delay_us(7);

  // Read 3 bytes
  ret = spi_device_transmit(ads->hspix, &t_read);

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
    spi_device_transmit(ads->hspix, &t_cmd);

    // Delay t6 (Delay from Last SCLK to First SCLK of Read)
    // Datasheet: 50 * tCLKIN. Para 8MHz -> ~6.25us. Usamos 7us.
    esp_rom_delay_us(7);

    //Leer los datos del registro
    spi_device_transmit(ads->hspix, &t_read);

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
  spi_device_transmit(ads->hspix, &t);

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

    // Wait for DRDY (Required before sending commands like SDATAC or SELFCAL)
    ADS125X_DRDY_Wait(ads);

    // Send the command (Blocking/Yielding)
    spi_device_transmit(ads->hspix, &t);

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
    // Timeout de aprox 1 segundo (depende de cuántas vueltas da el loop)
    // En ESP32 a 160MHz, un loop simple corre muy rápido.
    int32_t timeout = 100000; 

    while (gpio_get_level(ads->drdyPin) == 1) {
        if (timeout <= 0) {
            ESP_LOGE("ADS125X", "Error: DRDY Timeout. El sensor no responde.");
            return 1; // Error
        }
        timeout--;
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

