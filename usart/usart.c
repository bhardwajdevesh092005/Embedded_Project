#include "usart.h"

void usart_clock_setup(void){
    /* Enable GPIOG clock for LED & USARTs. */
    rcc_periph_clock_enable(RCC_GPIOG);
    rcc_periph_clock_enable(RCC_GPIOA);

    /* Enable clocks for USART1. */
    rcc_periph_clock_enable(RCC_USART1); 
}

void usart_setup(void){
    usart_set_baudrate(USART1, 115200);
    usart_set_databits(USART1, 8);
    usart_set_stopbits(USART1, USART_STOPBITS_1);

    /* TX + RX mode */
    usart_set_mode(USART1, USART_MODE_TX_RX);

    usart_set_parity(USART1, USART_PARITY_NONE);
    usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

    usart_enable(USART1);
}

void gpio_setup(void){
    /* Setup GPIO pin GPIO13 on GPIO port G for LED. */
    gpio_mode_setup(GPIOG, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);

    /* 🔥 FIX: Setup GPIO pins for USART1 transmit (PA9) AND receive (PA10). */
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9 | GPIO10);

    /* 🔥 FIX: Setup USART1 TX/RX pins as alternate function 7. */
    gpio_set_af(GPIOA, GPIO_AF7, GPIO9 | GPIO10);
}

void usart_send_string(const char *str)
{
    while (*str) {
        usart_send_blocking(USART1, *str++);
    }
}

char usart_read_char(void)
{
    return usart_recv_blocking(USART1);
}