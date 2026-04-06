#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

void usart_clock_setup(void);
void usart_setup(void);
void gpio_setup(void);
char usart_read_char(void);
void usart_send_string(const char *str);