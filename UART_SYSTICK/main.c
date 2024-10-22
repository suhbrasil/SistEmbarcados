#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"

#define LED_PORTN GPIO_PORTN_BASE   // LEDs 1 e 2
#define LED_PORTF GPIO_PORTF_BASE   // LEDs 3 e 4
#define LED1 GPIO_PIN_1
#define LED2 GPIO_PIN_0
#define LED3 GPIO_PIN_4
#define LED4 GPIO_PIN_0

#define SW_PORT GPIO_PORTJ_BASE  // Chaves SW1 e SW2
#define SW1 GPIO_PIN_0
#define SW2 GPIO_PIN_1

uint32_t SysClock;
volatile bool timeoutFlag = false;  // Flag para controle do timeout

void UARTSendString(const char *str);
void SwitchHandler(void);
void SysTickHandler(void);  // Protótipo do SysTick Handler



// Handler UART - Processa comandos via UART
void UARTIntHandler(void) {
    uint32_t status = UARTIntStatus(UART0_BASE, true);
    UARTIntClear(UART0_BASE, status);
    char command = (char)UARTCharGetNonBlocking(UART0_BASE);

    switch (command) {
        case '1':
            GPIOPinWrite(LED_PORTN, LED1, LED1);
						GPIOPinWrite(LED_PORTN, LED2, 0);
						GPIOPinWrite(LED_PORTF, LED3 | LED4, 0);
            UARTSendString("LED 1 LIGADO\r\n");
            break;
        case '2':
            GPIOPinWrite(LED_PORTN, LED2, LED2);
						GPIOPinWrite(LED_PORTN, LED1, 0);
						GPIOPinWrite(LED_PORTF, LED3 | LED4, 0);
            UARTSendString("LED 2 LIGADO\r\n");
            break;
        case '3':
            GPIOPinWrite(LED_PORTF, LED3, LED3);
						GPIOPinWrite(LED_PORTN, LED1 | LED2, 0);
						GPIOPinWrite(LED_PORTF, LED4, 0);
            UARTSendString("LED 3 LIGADO\r\n");
            break;
        case '4':
            GPIOPinWrite(LED_PORTF, LED4, LED4);
						GPIOPinWrite(LED_PORTN, LED1 | LED2, 0);
						GPIOPinWrite(LED_PORTF, LED3, 0);
            UARTSendString("LED 4 LIGADO\r\n");
            break;
				case '5':
            if (GPIOPinRead(SW_PORT, SW1) == 0) {
                UARTSendString("SW1 Pressionada\r\n");
            } else {
                UARTSendString("SW1 Nao Pressionada\r\n");
            }
            break;
        case '6':
            if (GPIOPinRead(SW_PORT, SW2) == 0) {
                UARTSendString("SW2 Pressionada\r\n");
            } else {
                UARTSendString("SW2 Nao Pressionada\r\n");
            }
            break;
        default:
            UARTSendString("Comando Invalido\r\n");
            break;
    }
}


// SysTick Handler - Verifica timeout e alterna LEDs
void SysTickHandler(void) {
    static uint32_t count = 0;
    count++;

    if (count >= 10000) {  // Timeout de 10 seg
        count = 0;
        timeoutFlag = true;  // Seta a flag de timeout

        // Alterna LEDs conforme diagrama
        GPIOPinWrite(LED_PORTN, LED1 | LED2, 0);  // Desliga LED 1 e 2
        GPIOPinWrite(LED_PORTF, LED3 | LED4, LED3 | LED4);  // Liga LED 3 e 4
    }
}


// Configuração do SysTick
void SetupSysTick(void) {
    SysTickPeriodSet(SysClock / 1000);  // Configura SysTick para 1ms
    SysTickIntRegister(SysTickHandler);  // Registra o handler
    SysTickIntEnable();  // Habilita interrupções do SysTick
    SysTickEnable();  // Habilita o SysTick
}


// Handler para interrupções dos switches SW1 e SW2
void SwitchHandler(void) {
    uint32_t status = GPIOIntStatus(SW_PORT, true);
    GPIOIntClear(SW_PORT, status);

    if (status & SW1) {
				GPIOPinWrite(LED_PORTN, LED1 | LED2, LED1 | LED2);	 // liga LED 1 e 2 
				GPIOPinWrite(LED_PORTF, LED3 | LED4, 0);	// desliga LED 3 e 4 
    }

    if (status & SW2) {
			
        // Reinicia a contagem do SysTick
        SysTickDisable();  // Desabilita o SysTick temporariamente
        SysTickPeriodSet(SysClock / 1000);  // Configura o SysTick para 1ms
        SysTickEnable();  // Habilita o SysTick novamente

        // Reconfigura os LEDs para o estado inicial
        GPIOPinWrite(LED_PORTN, LED1 | LED2, LED1 | LED2);
        GPIOPinWrite(LED_PORTF, LED3 | LED4, LED3 | LED4);

        // Reseta a flag de timeout
        timeoutFlag = false;
    }
}

// Configurações da UART
void SetupUart(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0));
    UARTConfigSetExpClk(UART0_BASE, SysClock, 115200,
        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    UARTFIFODisable(UART0_BASE);
    UARTIntEnable(UART0_BASE, UART_INT_RX);
    UARTIntRegister(UART0_BASE, UARTIntHandler);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA));
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
}

// Configuração dos LEDs e switches
void ConfigPeripherals(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIOPinTypeGPIOOutput(LED_PORTN, LED1 | LED2);
    GPIOPinTypeGPIOOutput(LED_PORTF, LED3 | LED4);

    // Liga todos os LEDs no início
    GPIOPinWrite(LED_PORTN, LED1 | LED2, LED1 | LED2);
    GPIOPinWrite(LED_PORTF, LED3 | LED4, LED3 | LED4);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ);
    GPIOPinTypeGPIOInput(SW_PORT, SW1 | SW2);
    GPIOPadConfigSet(SW_PORT, SW1 | SW2, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    // Configuração das interrupções dos switches
    GPIOIntTypeSet(SW_PORT, SW1 | SW2, GPIO_FALLING_EDGE);
    GPIOIntRegister(SW_PORT, SwitchHandler);
    GPIOIntEnable(SW_PORT, SW1 | SW2);
}

// Função para enviar strings pela UART
void UARTSendString(const char *str) {
    while (*str != '\0') {
        UARTCharPut(UART0_BASE, *str++);
    }
}

// Função principal
int main(void) {
    SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | 
                SYSCTL_USE_PLL | SYSCTL_CFG_VCO_240), 120000000);
	

    SetupSysTick();  // Inicializa o SysTick
    ConfigPeripherals();
    SetupUart();

    while (1) {
        __asm(" WFI");  // Aguardando interrupção
    }
}
