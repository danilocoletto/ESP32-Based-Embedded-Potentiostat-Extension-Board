
#include "general.h"


volatile bool ABORT_FLAG = 0;

/********************************************************************************/
/*                                  UART Functions                              */
/********************************************************************************/

/**
 * @brief Initializes the UART peripheral with the specified configuration.
 * * Sets the baud rate, data bits, parity, and flow control. It installs the UART 
 * driver with internal buffers and configures the default communication pins.
 */
void init_UART(void) 
{
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
