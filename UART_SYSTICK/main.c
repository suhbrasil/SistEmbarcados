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

volatile uint32_t count = 0;   // Contador de tempo
volatile bool sw1Pressed = false;  // Estado do SW1

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


// SysTick Handler - Conta apenas quando SW1 está pressionado
void SysTickHandler(void) {
    if (sw1Pressed) {
        count++;

        // Imprime tempo no Putty a cada 1 segundo
        if (count % 1000 == 0) {  // Considerando SysTick configurado para 1ms
            char buffer[50];
            snprintf(buffer, sizeof(buffer), "Tempo: %lu segundos\r\n", count / 1000);
            UARTSendString(buffer);
        }

        if (count >= 10000) {  // Timeout de 10 segundos
            count = 0;
            sw1Pressed = false;  // Desativa a contagem
            timeoutFlag = true;

            // Liga LEDs 3 e 4 e desliga LEDs 1 e 2
            GPIOPinWrite(LED_PORTN, LED1 | LED2, 0);
            GPIOPinWrite(LED_PORTF, LED3 | LED4, LED3 | LED4);

            UARTSendString("Fim de jogo!\r\n");
        }
    }
}


// Configuração do SysTick
void SetupSysTick(void) {
    SysTickPeriodSet(SysClock / 1000);  // Configura SysTick para 1ms
    SysTickIntRegister(SysTickHandler);  // Registra o handler
    SysTickIntEnable();  // Habilita interrupções do SysTick
    SysTickEnable();  // Habilita o SysTick
}


// Handler para SW1 e SW2
void SwitchHandler(void) {
    uint32_t status = GPIOIntStatus(SW_PORT, true);
    GPIOIntClear(SW_PORT, status);

    if (status & SW1) {
        // Verifica se SW1 está pressionado ou solto
        if (GPIOPinRead(SW_PORT, SW1) == 0) {
            sw1Pressed = true;  // Inicia a contagem
						count = 0;

            // Liga LEDs 1 e 2 e desliga LEDs 3 e 4
            GPIOPinWrite(LED_PORTN, LED1 | LED2, LED1 | LED2);
            GPIOPinWrite(LED_PORTF, LED3 | LED4, 0);

            UARTSendString("SW1 pressionado. Contagem iniciada.\r\n");
        } else {
            sw1Pressed = false;  // Para a contagem
            UARTSendString("SW1 solto.\r\n");
        }
    }

    if (status & SW2) {
        // Reinicia o jogo ao pressionar SW2
        sw1Pressed = false;
        count = 0;
        timeoutFlag = false;

        // Liga todos os LEDs
        GPIOPinWrite(LED_PORTN, LED1 | LED2, LED1 | LED2);
        GPIOPinWrite(LED_PORTF, LED3 | LED4, LED3 | LED4);

        UARTSendString("Jogo reiniciado.\r\n");
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
