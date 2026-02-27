/*******************************************************************************
 * @file        ads1255.h
 * @brief       C Header Library for ads1255/ads1256 family of Analog to Digital
 *              Conterters (ADC)
 * @details     This file contains the prototypes and definitions needed for 
 *              the library to implements the functionalities of the ADC.
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

#ifndef ADS1255_H
#define	ADS1255_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
//#include "esp_log.h"
//#include "esp_attr.h"
#include "soc/spi_struct.h"
#include "soc/spi_reg.h"

#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "gpio_config.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "muxes.h"

//#define DEBUG_ADS1255

// reference voltage
#define ADS125X_VREF_2_5V (2.5f)
#define ADS125X_VREF_5_0V (5.0f)
#define ADS125X_OSC_FREQ_7_8M (7680000)
#define ADS125X_OSC_FREQ_8M (8000000)

#define ADS125X_ON          1
#define ADS125X_OFF         0


/*
const defaults = {
      clkinFrequency: 8000000,
      spiFrequency: 976563,
      spiMode: 1,
      vRef: 5.0,
    };
*/

// Configuración del SPI
#define SPI_HOST_ID             SPI2_HOST  // HSPI en ESP32
#define DMA_CHAN                SPI_DMA_CH_AUTO

// --- SPI SCLOCK SPEEDS ---
#define CLOCK_SPEED_100KHZ      100000
#define CLOCK_SPEED_250KHZ      250000
#define CLOCK_SPEED_500KHZ      500000
#define CLOCK_SPEED_1MHZ        1000000
#define CLOCK_SPEED_1_2MHZ      1200000
#define CLOCK_SPEED_1_5MHZ      1500000
#define CLOCK_SPEED_1_7MHZ      1700000
#define CLOCK_SPEED_1_92MHZ     1920000
#define CLOCK_SPEED_2MHZ        2000000

// --- ADS1255 PINOUT FOR ESP32 ---
#define GEN_CLOCK

#ifdef GEN_CLOCK
  #define ADS1255_CLOCK_PIN     GPIO_NUM_16 
#endif

#define ADS1255_DOUT_MISO       GPIO_NUM_19
#define ADS1255_DIN_MOSI        GPIO_NUM_23
#define ADS1255_SCLK            GPIO_NUM_18
#define ADS1255_CS              GPIO_NUM_5
#define ADS1255_DRDY            GPIO_NUM_4

// ADS1256 Register
#define ADS125X_REG_STATUS      0x00
#define ADS125X_REG_MUX         0x01
#define ADS125X_REG_ADCON       0x02
#define ADS125X_REG_DRATE       0x03
#define ADS125X_REG_IO          0x04
#define ADS125X_REG_OFC0        0x05
#define ADS125X_REG_OFC1        0x06
#define ADS125X_REG_OFC2        0x07
#define ADS125X_REG_FSC0        0x08
#define ADS125X_REG_FSC1        0x09
#define ADS125X_REG_FSC2        0x0A

// ADS1256 Command
// Datasheet p. 34 / Table 24
// All of the commands are stand-alone
// except for the register reads and writes (RREG, WREG) 
// which require a second command byte plus data

#define ADS125X_CMD_WAKEUP      0x00
#define ADS125X_CMD_RDATA       0x01
#define ADS125X_CMD_RDATAC      0x03
#define ADS125X_CMD_SDATAC      0x0f
#define ADS125X_CMD_RREG        0x10
#define ADS125X_CMD_WREG        0x50
#define ADS125X_CMD_SELFCAL     0xF0
#define ADS125X_CMD_SELFOCAL    0xF1
#define ADS125X_CMD_SELFGCAL    0xF2
#define ADS125X_CMD_SYSOCAL     0xF3
#define ADS125X_CMD_SYSGCAL     0xF4
#define ADS125X_CMD_SYNC        0xFC
#define ADS125X_CMD_STANDBY     0xFD
#define ADS125X_CMD_RESET       0xFE

#define ADS125X_BUFON           0x02
#define ADS125X_BUFOFF          0x00

#define ADS125X_CLKOUT_OFF      0x00
#define ADS125X_CLKOUT_1        0x20
#define ADS125X_CLKOUT_HALF     0x40
#define ADS125X_CLKOUT_QUARTER  0x60

#define ADS125X_RDATA           0x01    /* Read Data */
#define ADS125X_RDATAC          0x03    /* Read Data Continuously */
#define ADS125X_SDATAC          0x0F    /* Stop Read Data Continuously */
#define ADS125X_RREG            0x10    /* Read from REG */
#define ADS125X_WREG            0x50    /* Write to REG */
#define ADS125X_SELFCAL         0xF0    /* Offset and Gain Self-Calibration */
#define ADS125X_SELFOCAL        0xF1    /* Offset Self-Calibration */
#define ADS125X_SELFGCAL        0xF2    /* Gain Self-Calibration */
#define ADS125X_SYSOCAL         0xF3    /* System Offset Calibration */
#define ADS125X_SYSGCAL         0xF4    /* System Gain Calibration */
#define ADS125X_SYNC            0xFC    /* Synchronize the A/D Conversion */
#define ADS125X_STANDBY         0xFD    /* Begin Standby Mode */
#define ADS125X_RESET           0xFE    /* Reset to Power-Up Values */
#define ADS125X_WAKEUP          0xFF    /* Completes SYNC and Exits Standby Mode */

// multiplexer codes
#define ADS125X_MUXP_AIN0       0x00
#define ADS125X_MUXP_AIN1       0x10
#define ADS125X_MUXP_AIN2       0x20
#define ADS125X_MUXP_AIN3       0x30
#define ADS125X_MUXP_AIN4       0x40
#define ADS125X_MUXP_AIN5       0x50
#define ADS125X_MUXP_AIN6       0x60
#define ADS125X_MUXP_AIN7       0x70
#define ADS125X_MUXP_AINCOM     0x80

#define ADS125X_MUXN_AIN0       0x00
#define ADS125X_MUXN_AIN1       0x01
#define ADS125X_MUXN_AIN2       0x02
#define ADS125X_MUXN_AIN3       0x03
#define ADS125X_MUXN_AIN4       0x04
#define ADS125X_MUXN_AIN5       0x05
#define ADS125X_MUXN_AIN6       0x06
#define ADS125X_MUXN_AIN7       0x07
#define ADS125X_MUXN_AINCOM     0x08

// gain codes
#define ADS125X_PGA1            0x00
#define ADS125X_PGA2            0x01
#define ADS125X_PGA4            0x02
#define ADS125X_PGA8            0x03
#define ADS125X_PGA16           0x04
#define ADS125X_PGA32           0x05
#define ADS125X_PGA64           0x06

// data rate codes
/** @note: Data Rate vary depending on crystal frequency. 
  * Data rates listed below assumes the crystal frequency is 7.68Mhz
  * for other frequency consult the datasheet.
  */
#define ADS125X_DRATE_30000SPS  0xF0
#define ADS125X_DRATE_15000SPS  0xE0
#define ADS125X_DRATE_7500SPS   0xD0
#define ADS125X_DRATE_3750SPS   0xC0
#define ADS125X_DRATE_2000SPS   0xB0
#define ADS125X_DRATE_1000SPS   0xA1
#define ADS125X_DRATE_500SPS    0x92
#define ADS125X_DRATE_100SPS    0x82
#define ADS125X_DRATE_60SPS     0x72
#define ADS125X_DRATE_50SPS     0x63
#define ADS125X_DRATE_30SPS     0x53
#define ADS125X_DRATE_25SPS     0x43
#define ADS125X_DRATE_15SPS     0x33
#define ADS125X_DRATE_10SPS     0x23
#define ADS125X_DRATE_5SPS      0x13
#define ADS125X_DRATE_2_5SPS    0x03

// Struct "Object"
typedef struct {
    spi_device_handle_t hspix;
    float               vref;
    uint8_t             pga;
    uint8_t             current_drate;
    uint32_t            propagation_delay_us;
    float               convFactor;
    uint32_t            oscFreq;
    gpio_num_t          csPin;
    gpio_num_t          drdyPin;
} ADS125X_t;


typedef struct {
    uint8_t reg_val;      // Valor del registro DRATE (ej: 0x03 para 2.5 SPS)
    uint32_t timeout_us;  // Timeout recomendado en us
} ADS125X_timeout_t;


typedef struct {
    uint8_t reg_val;      // Valor del registro DRATE (ej: 0x03 para 2.5 SPS)
    uint32_t propagation_delay_us;  // Timeout recomendado en us
} ADS125X_prop_delay_t;

/**************Functions for ADS1255************/

uint8_t  ADS125X_CS                          (ADS125X_t *ads, uint8_t state);
uint8_t  ADS125X_DRDY_Wait                   (ADS125X_t *ads);
void     ADS125X_Init                        (ADS125X_t *ads, uint8_t drate, uint8_t gain, uint8_t buffer_en);
uint8_t  ADS125X_Register_Read               (ADS125X_t *ads, uint8_t reg, uint8_t* pData, uint8_t n);
uint8_t  ADS125X_Register_Write              (ADS125X_t *ads, uint8_t reg, uint8_t data);
uint8_t  ADS125X_CMD_Send                    (ADS125X_t *ads, uint8_t cmd);
void     ADS125X_ADC_Code2Volt               (ADS125X_t *ads, int32_t *pCode, float *pVolt, uint16_t size);
float    ADS125X_ADC_ReadVolt                (ADS125X_t *ads);
void     ADS125X_Channel_Set                 (ADS125X_t *ads, int8_t chan);
uint8_t  ADS125X_ChannelDiff_Set             (ADS125X_t *ads, int8_t p_chan, int8_t n_chan);
float    ADS125X_ADCFast_ReadVolt_HAL        (ADS125X_t *ads);

float    ADS125X_Get_Prop_Delay_Us(void);
int32_t ADS125X_Get_Timeout_Us(uint8_t current_drate);

/*********Internal Functions for ADS1255*********/

spi_device_handle_t   init_spi_bus            (void);
void                  init_adc_clock          (uint32_t freq);
void                  ADS125X_Config_Pins     (void);

/*********HAL Layer Functions for ADS1255*********/

void     ADS125X_INIT_HAL           (uint8_t drate, uint8_t gain, uint8_t buffer_en);
void     ADS125X_WAKEUP_HAL         (void);
void     ADS125X_STANDBY_HAL        (void);
void     ADS125X_SDATAC_HAL         (void);
void     ADS125X_CMD_SEND_HAL       (uint8_t cmd);
float    ADS125X_READVOLT_HAL       (void);
uint8_t  ADS125X_READ_REG_HAL       (uint8_t reg, uint8_t* pData, uint8_t n);
void     ADS125X_WRITE_REG_HAL      (uint8_t reg, uint8_t data);
uint8_t  ADS125X_WAIT_DYDR_HAL      (void);



#endif	/* ADS1255_H */
