#include <stdint.h>
#include "tm4c123gh6pm.h"
#include <stdio.h>

volatile uint32_t countdown_value = 0;
volatile uint8_t running = 0;
volatile uint32_t seconds = 0, minutes = 0, hours = 0, milliseconds = 0;

void SysTick_Init(void) {
    NVIC_ST_CTRL_R = 0;
    NVIC_ST_RELOAD_R = 16000 - 1;
    NVIC_ST_CURRENT_R = 0;
    NVIC_ST_CTRL_R = 0x00000007;
}

void Buttons_Init(void) {
    // Initialize PortF for onboard button (reset)
    SYSCTL_RCGCGPIO_R |= 0x00000020;
    GPIO_PORTF_LOCK_R = 0x4C4F434B;
    GPIO_PORTF_CR_R |= 0x1F;
    GPIO_PORTF_DIR_R &= ~0x10;
    GPIO_PORTF_PUR_R |= 0x10;
    GPIO_PORTF_DEN_R |= 0x10;
    GPIO_PORTF_IS_R &= ~0x10;
    GPIO_PORTF_IBE_R &= ~0x10;
    GPIO_PORTF_IEV_R &= ~0x10;
    GPIO_PORTF_ICR_R = 0x10;
    GPIO_PORTF_IM_R |= 0x10;

    // Initialize PortA for IR module
    SYSCTL_RCGCGPIO_R |= 0x00000001;
    GPIO_PORTA_DIR_R &= ~0x04;
    GPIO_PORTA_DEN_R |= 0x04;
    GPIO_PORTA_IS_R &= ~0x04;
    GPIO_PORTA_IBE_R &= ~0x04;
    GPIO_PORTA_IEV_R |= 0x04;
    GPIO_PORTA_ICR_R = 0x04;
    GPIO_PORTA_IM_R |= 0x04;

    // Enable interrupts in NVIC
    NVIC_EN0_R |= 0x40000000;
    NVIC_EN0_R |= 0x00000001;
}

void LED_Init(void) {
    SYSCTL_RCGCGPIO_R |= 0x20;
    GPIO_PORTF_DIR_R |= 0x0E;
    GPIO_PORTF_DEN_R |= 0x0E;
    GPIO_PORTF_DATA_R &= ~0x0E;
}

void UART5_Init(void) {
    SYSCTL_RCGCUART_R |= 0x00000020;
    SYSCTL_RCGCGPIO_R |= 0x00000010;

    GPIO_PORTE_AFSEL_R |= 0x30;
    GPIO_PORTE_PCTL_R = (GPIO_PORTE_PCTL_R & 0xFF00FFFF) + 0x00110000;
    GPIO_PORTE_DEN_R |= 0x30;

    UART5_CTL_R &= ~0x0001;
    UART5_IBRD_R = 104;
    UART5_FBRD_R = 11;
    UART5_LCRH_R = 0x0060;
    UART5_CTL_R = 0x0301;
}

void UART5_SendString(char *str) {
    while(*str) {
        while((UART5_FR_R & 0x0020) != 0);
        UART5_DR_R = *str++;
    }
}

void UART5_SendNumber(uint32_t num) {
    char buffer[20];
    sprintf(buffer, "%lu", num);
    UART5_SendString(buffer);
}

void SysTick_Handler(void) {
    if (running) {
        countdown_value++;
    }
}

void GPIOF_Handler(void) {
    GPIO_PORTF_ICR_R = 0x10;

    running = 0;
    countdown_value = 0;
    seconds = 0;
    minutes = 0;
    hours = 0;
    milliseconds = 0;
    
    // Red LED on for reset
    GPIO_PORTF_DATA_R &= ~0x0E;
    GPIO_PORTF_DATA_R |= 0x02;
    
    UART5_SendString("Timer Reset\r\n");
}

void GPIOA_Handler(void) {
    GPIO_PORTA_ICR_R = 0x04;  // Acknowledge interrupt

    uint8_t current_ir_state = (GPIO_PORTA_DATA_R & 0x04) == 0;

    if (current_ir_state && !running) {
        // State 1: Start timing when IR is ON
        running = 1;
        countdown_value = 0;
        
        // Green LED on
        GPIO_PORTF_DATA_R &= ~0x0E;
        GPIO_PORTF_DATA_R |= 0x08;
        
        UART5_SendString("Object Detected - Timing Started\r\n");
    } 
    else if (!current_ir_state && running) {
        // State 2: Stop timing when IR is OFF
        running = 0;
        
        // Red LED on
        GPIO_PORTF_DATA_R &= ~0x0E;
        GPIO_PORTF_DATA_R |= 0x02;
        
        // Send timing data
        UART5_SendString("Timing Duration: ");
        UART5_SendNumber(minutes);
        UART5_SendString(" min, ");
        UART5_SendNumber(seconds);
        UART5_SendString(" sec, ");
        UART5_SendNumber(milliseconds);
        UART5_SendString(" ms\r\n");
    }
}

void DisplayTime(void) {
    milliseconds = countdown_value % 1000;
    seconds = (countdown_value / 1000) % 60;
    minutes = (countdown_value / (1000 * 60)) % 60;
    hours = (countdown_value / (1000 * 3600)) % 24;
}

int main(void) {
    SysTick_Init();
    Buttons_Init();
    LED_Init();
    UART5_Init();

    while (1) {
        DisplayTime();
    }
}