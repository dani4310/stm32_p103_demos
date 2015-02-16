#define USE_STDPERIPH_DRIVER
#include "stm32f10x.h"

#include <stdlib.h>

/* Flag whenever the button is pressed.
 * Note that the interrupt handler is initialized to only
 * fire when the button is pressed, not released.
 */
uint8_t button_pressed = 0;
void EXTI0_IRQHandler(void)
{
    /* Make sure the line has a pending interrupt
     * (should this always be true if we are inside the interrupt handle?) */
    if(EXTI_GetITStatus(EXTI_Line0) != RESET) {
        button_pressed = 1;
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}

void TIM2_IRQHandler(void)
{
    /* Note that I think we could have used TIM_GetFlagStatus
       and TIM_ClearFlag instead of TIM_GetITStatus and TIM_ClearITPendingBit.
       These routines seem to overlap quite a bit in functionality. */

    /* Make sure the line has a pending interrupt
     * which should always be true.
     *  */
    if(TIM_GetITStatus(TIM2, USART_IT_TXE) != RESET) {
        /* Toggle the LED */
        GPIOC->ODR ^= 0x00001000;
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}



/* Functions for sending numbers through the UART */
char hex_to_char(unsigned hex_number)
{
    if(hex_number < 0xA) {
        return hex_number + '0';
    } else {
        return hex_number - 0xA + 'A';
    }
}

void send_byte(uint8_t b)
{
    /* Wait until the RS232 port can receive another byte. */
    while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);

    /* Toggle the LED just to show that progress is being made. */
    GPIOC->ODR ^= 0x00001000;

    /* Send the byte */
    USART_SendData(USART2, b);
}

void send_adc_sample(unsigned int sample)
{
    send_byte(hex_to_char((sample >> 8) & 0xf));
    send_byte(hex_to_char((sample >> 4) & 0xf));
    send_byte(hex_to_char(sample & 0xf));
}

void send_number(unsigned long sample, int radix)
{
    int digit;
    unsigned long  mod;
    char str[100];

    digit = 0;
    do {
        mod = sample % radix;
        str[digit] = hex_to_char(mod);
        sample /= radix;
        digit++;
    } while(sample != 0);

    while(digit != 0) {
        digit--;
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
        USART_SendData(USART2, str[digit]);
    }
}


/* ADC functions */
void init_timer(void) {
    /* Configure peripheral clock. */
    /* Let's leave PCLK1 at it's default setting of 36 MHz. */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* Time base configuration */
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Period = 65535;
    uint16_t prescaler_value = (uint16_t) (36000000 / 65535 / 1);
    TIM_TimeBaseStructure.TIM_Prescaler = 2 * prescaler_value;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    /* Enable the timer IRQ in the NVIC module (so that the USART2 interrupt
     * handler is enabled). */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM2, ENABLE);
}




int main(void)
{
    /* Initialization */
    init_led();

    init_timer();

    init_button();
    enable_button_interrupts();

    init_rs232();
    enable_rs232();

    rs232_print_str("Starting...\n");

    /* Infinite loop to sample ADC and print results.
     * There are several modes - see the README. */
    int mode = 1;
    while(1) {
        uint16_t adc_value;
        switch(mode) {
            case 1:
                rs232_print_str("Counter value=");
                send_number(TIM_GetCounter(TIM2), 10);
                rs232_print_str("\n");
                break;
        }

        /* A button has been pressed.  Update the mode. */
        if(button_pressed) {
            button_pressed = 0;

            mode++;
            if(mode > 4) {
                mode = 1;
            }

            if(mode == 4) {
                RCC_ClocksTypeDef RCC_Clocks;
                RCC_GetClocksFreq(&RCC_Clocks);
                rs232_print_str("\nMODE 4\n");
                rs232_print_str("SYSCLK=");
                send_number(RCC_Clocks.SYSCLK_Frequency, 10);
                rs232_print_str("\n");
                rs232_print_str("HCLK=");
                send_number(RCC_Clocks.HCLK_Frequency, 10);
                rs232_print_str("\n");
                rs232_print_str("PCLK1=");
                send_number(RCC_Clocks.PCLK1_Frequency, 10);
                rs232_print_str("\n");
            }
        }
    }
}
