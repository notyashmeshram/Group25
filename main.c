#include <stdint.h>
#include "tm4c123gh6pm.h"
#include <stdio.h>

volatile uint32_t countdown_value = 0;     // Total milliseconds elapsed
volatile uint8_t running = 0;              // Stopwatch state
volatile uint32_t seconds = 0, minutes = 0, hours = 0, milliseconds = 0;

void SysTick_Init(void) {
    NVIC_ST_CTRL_R = 0;                   // Disable SysTick during setup
    NVIC_ST_RELOAD_R = 16000 - 1;         // Set reload value for 1ms interrupts
    NVIC_ST_CURRENT_R = 0;                // Clear the current count
    NVIC_ST_CTRL_R = 0x00000007;          // Enable SysTick with clock and interrupts
}

void Buttons_Init(void) {
    // Initialize PortF for onboard button (reset)
    SYSCTL_RCGCGPIO_R |= 0x00000020;      // Enable clock for PortF
    GPIO_PORTF_LOCK_R = 0x4C4F434B;       // Unlock PortF
    GPIO_PORTF_CR_R |= 0x1F;              // Allow changes to all PortF pins
    GPIO_PORTF_DIR_R &= ~0x10;            // Set Pin 4 (SW1) as input
    GPIO_PORTF_PUR_R |= 0x10;             // Enable pull-up resistor for Pin 4
    GPIO_PORTF_DEN_R |= 0x10;             // Enable digital function for Pin 4
    GPIO_PORTF_IS_R &= ~0x10;             // Set Pin 4 as edge-sensitive
    GPIO_PORTF_IBE_R &= ~0x10;            // Set Pin 4 as not both edges
    GPIO_PORTF_IEV_R &= ~0x10;            // Set Pin 4 as falling edge
    GPIO_PORTF_ICR_R = 0x10;              // Clear any prior interrupt
    GPIO_PORTF_IM_R |= 0x10;              // Enable interrupt for Pin 4

    // Initialize PortA for IR module
    SYSCTL_RCGCGPIO_R |= 0x00000001;      // Enable clock for PortA
    GPIO_PORTA_DIR_R &= ~0x04;            // Set PA2 as input
    GPIO_PORTA_DEN_R |= 0x04;             // Enable digital function for PA2
    GPIO_PORTA_IS_R &= ~0x04;             // Edge-sensitive
    GPIO_PORTA_IBE_R &= ~0x04;            // Not both edges
    GPIO_PORTA_IEV_R |= 0x04;             // Rising edge trigger (object removed)
    GPIO_PORTA_ICR_R = 0x04;              // Clear any prior interrupt
    GPIO_PORTA_IM_R |= 0x04;              // Enable interrupt for PA2

    // Enable interrupts in NVIC
    NVIC_EN0_R |= 0x40000000;             // Enable interrupt for PortF
    NVIC_EN0_R |= 0x00000001;             // Enable interrupt for PortA
}

void LED_Init(void) {
    SYSCTL_RCGCGPIO_R |= 0x20;            // Enable clock for PortF
    GPIO_PORTF_DIR_R |= 0x0E;             // Set Pins 1, 2, and 3 as output
    GPIO_PORTF_DEN_R |= 0x0E;             // Enable digital function for Pins 1, 2, and 3
    GPIO_PORTF_DATA_R &= ~0x0E;           // Turn off all LEDs initially
}

void ControlLED(void) {
    if (running) {
        GPIO_PORTF_DATA_R &= ~0x06;       // Turn off red and blue LEDs
        GPIO_PORTF_DATA_R |= 0x08;        // Turn on green LED
    } else {
        GPIO_PORTF_DATA_R &= ~0x0A;       // Turn off green and blue LEDs
        GPIO_PORTF_DATA_R |= 0x02;        // Turn on red LED
    }
}

void SysTick_Handler(void) {
    if (running) {
        countdown_value++;                // Increment the total time counter
    }
}

void GPIOF_Handler(void) {
    GPIO_PORTF_ICR_R = 0x10;              // Acknowledge the interrupt for SW1

    // Onboard SW1 button handling (reset functionality)
    running = 0;
    countdown_value = 0;                  // Reset countdown to zero
    ControlLED();
}

void GPIOA_Handler(void) {
    GPIO_PORTA_ICR_R = 0x04;              // Acknowledge the interrupt

    // Active low IR sensor - 0 means object detected, 1 means no object
    uint8_t current_ir_state = (GPIO_PORTA_DATA_R & 0x04) == 0;

    // Toggle running state when IR input changes
    running = !running;

    ControlLED();
}

void DisplayTime(void) {
    // Convert countdown_value to H:M:S format
    milliseconds = countdown_value % 1000;
    seconds = (countdown_value / 1000) % 60;
    minutes = (countdown_value / (1000 * 60)) % 60;
    hours = (countdown_value / (1000 * 3600)) % 24;
}

int main(void) {
    SysTick_Init();
    Buttons_Init();
    LED_Init();

    while (1) {
        DisplayTime();
    }
}
