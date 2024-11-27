#include <stdint.h>
#include "tm4c123gh6pm.h"
#include <stdio.h>

// Global variables for timing
volatile uint32_t countdown_value = 0;
volatile uint8_t running = 0;
volatile uint32_t seconds = 0, minutes = 0, hours = 0, milliseconds = 0;

// Function prototypes
void SysTick_Init(void);
void Buttons_Init(void);
void LED_Init(void);
void UART5_Init(void);
void UART5_SendString(const char *str);
void UART5_SendNumber(uint32_t num);
void DisplayTime(void);
void ControlLED(void);

void SysTick_Init(void) {
    NVIC_ST_CTRL_R = 0;
    NVIC_ST_RELOAD_R = 16000 - 1;  // 1 ms at 16 MHz clock
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

void UART5_Init(void) {
    // Enable UART5 and GPIOE clocks
    SYSCTL_RCGCUART_R |= 0x00000020;
    SYSCTL_RCGCGPIO_R |= 0x00000010;

    // Configure PE4 and PE5 for UART5
    GPIO_PORTE_AFSEL_R |= 0x30;
    GPIO_PORTE_PCTL_R = (GPIO_PORTE_PCTL_R & 0xFF00FFFF) + 0x00110000;
    GPIO_PORTE_DEN_R |= 0x30;

    // Configure UART5 for 9600 baud rate
    UART5_CTL_R &= ~0x0001;               // Disable UART5
    UART5_IBRD_R = 104;                   // Integer part of baud rate divisor
    UART5_FBRD_R = 11;                    // Fractional part of baud rate divisor
    UART5_LCRH_R = 0x0060;                // 8-bit word length, enable FIFO
    UART5_CTL_R = 0x0301;                 // Enable UART5, TXE, RXE
}

void UART5_SendString(const char *str) {
    while(*str) {
        while((UART5_FR_R & 0x0020) != 0);  // Wait until TX FIFO not full
        UART5_DR_R = *str++;
    }
    // Add carriage return and line feed
    while((UART5_FR_R & 0x0020) != 0);
    UART5_DR_R = '\r';
    while((UART5_FR_R & 0x0020) != 0);
    UART5_DR_R = '\n';
}

void UART5_SendNumber(uint32_t num) {
    char buffer[12];  // Enough for a 32-bit integer
    int index = 0;

    // Handle zero separately
    if (num == 0) {
        buffer[index++] = '0';
    } else {
        // Convert number to string in reverse order
        while (num > 0) {
            buffer[index++] = (num % 10) + '0';
            num /= 10;
        }
    }

    // Reverse the string
    int start = 0;
    int end = index - 1;
    while (start < end) {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
        end--;
    }

    // Null-terminate the string
    buffer[index] = '\0';

    // Send the string
    UART5_SendString(buffer);
}

void SysTick_Handler(void) {
    if (running) {
        countdown_value++;
    }
}

void GPIOF_Handler(void) {
    GPIO_PORTF_ICR_R = 0x10;              // Acknowledge the interrupt for SW1

    running = 0;
    countdown_value = 0;                  // Reset countdown to zero
    seconds = 0;
    minutes = 0;
    hours = 0;
    milliseconds = 0;

    ControlLED();

    UART5_SendString("Timer Reset");
}

void GPIOA_Handler(void) {
    // Clear the interrupt flag for PA2
    GPIO_PORTA_ICR_R = 0x04;

    // Read current IR sensor state (active low)
    uint8_t current_ir_state = (GPIO_PORTA_DATA_R & 0x04) == 0;

    if (current_ir_state) {
        // Object detected
        if (!running) {
            // Start timing if not already running
            running = 1;
            countdown_value = 0;
            seconds = 0;
            minutes = 0;
            milliseconds = 0;

            // Green LED on
            ControlLED();

            UART5_SendString("Object Detected - Timing Started");
        }
    } else {
        // No object detected
        if (running) {
            // Stop timing if currently running
            running = 0;

            // Red LED on
            ControlLED();

            // Calculate final timing values
            milliseconds = countdown_value % 1000;
            seconds = (countdown_value / 1000) % 60;
            minutes = (countdown_value / (1000 * 60)) % 60;

            // Send timing data
            UART5_SendString("Timing Duration:");

            // Only send non-zero values
            if (minutes > 0) {
                UART5_SendNumber(minutes);
                UART5_SendString(" min ");
            }
            if (seconds > 0 || minutes > 0) {
                UART5_SendNumber(seconds);
                UART5_SendString(" sec ");
            }
            UART5_SendNumber(milliseconds);
            UART5_SendString(" ms");
        }
    }
}

void DisplayTime(void) {
    if (running) {
        milliseconds = countdown_value % 1000;
        seconds = (countdown_value / 1000) % 60;
        minutes = (countdown_value / (1000 * 60)) % 60;
    }
}

int main(void) {
    // Initialize all necessary modules
    SysTick_Init();
    Buttons_Init();
    LED_Init();
    UART5_Init();

    // Initial LED state
    ControlLED();

    while (1) {
        // Continuously update time if running
        DisplayTime();
    }
}
