#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/pwm.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "driverlib/adc.h"

#define ADC_SEQUENCER 3         // Sequenciador do ADC para uma única amostra
#define PWM_FREQUENCY 12000     // Frequência do PWM


#define LED_PORTN GPIO_PORTN_BASE   // LEDs 1 e 2
#define LED_PORTF GPIO_PORTF_BASE   // LEDs 3 e 4
#define LED1 GPIO_PIN_1
#define LED2 GPIO_PIN_0
#define LED3 GPIO_PIN_4
#define LED4 GPIO_PIN_0



// Variáveis Globais
uint32_t SysClock;
volatile uint32_t g_ui32LDRValue = 0;     // Valor lido do LDR
volatile bool g_bNewLDRValue = false;     // Flag para indicar novo valor do LDR disponível
uint32_t g_ui32PWMDutyCycle = 0;          // Duty cycle atual

// Prototipos
void SetupUart(void);
void SetupTimer(void);
void SetupADC(void);
void SetupPWM(void);
void Timer0IntHandler(void);
void UARTSend(const char *pui8Buffer);
void ProcessLDRValue(uint32_t ldrValue);
void SetupLEDs(void);

int main(void) {
    // Configuração do clock do sistema
    SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN |
                                   SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);

    SetupUart();
    SetupTimer();
    SetupADC();
    SetupPWM();
		SetupLEDs();

    while (1) {
        if (g_bNewLDRValue) {
            // Reseta a flag de novo valor do LDR
            g_bNewLDRValue = false;

            // Processa o valor do LDR
            ProcessLDRValue(g_ui32LDRValue);
        }
    }
}


void SetupPWM(void) {
    // Habiplita PWM e o port do PWM
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    GPIOPinTypePWM(GPIO_PORTG_BASE, GPIO_PIN_1);
    GPIOPinConfigure(GPIO_PG1_M0PWM5);
    // Configura o clock do PWM 
    SysCtlPWMClockSet(SYSCTL_PWMDIV_2); 

    // Configura gerador PWM
    uint32_t pwmPeriod = (SysClock/1) / PWM_FREQUENCY;  // Adjust clock division here
    PWMGenConfigure(PWM0_BASE, PWM_GEN_2, PWM_GEN_MODE_DOWN);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_2, pwmPeriod);
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_5, g_ui32PWMDutyCycle);
    PWMOutputState(PWM0_BASE, PWM_OUT_5_BIT, true);
    PWMGenEnable(PWM0_BASE, PWM_GEN_2);
}

void UARTIntHandler(void) {
    uint32_t status = UARTIntStatus(UART0_BASE, true);
    UARTIntClear(UART0_BASE, status);
}

void SetupUart(void) {
		// Enable UART0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0));
	
    //UARTConfigSetExpClk(UART0_BASE, SysClock, 115200,
        //(UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    //UARTFIFODisable(UART0_BASE);
    //UARTIntEnable(UART0_BASE, UART_INT_RX);
    //UARTIntRegister(UART0_BASE, UARTIntHandler);
	
	
		    // 115200, 8-N-1 
    UARTConfigSetExpClk(UART0_BASE, SysClock, 115200,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);
	
		// Configure GPIO pins for UART0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA));
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
}


void SetupLEDs(void) {
    // Habilita GPIO para LEDs (Portas N e F)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    // Aguarda os periféricos ficarem prontos
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPION));
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF));


    // Configura pinos como saída
    GPIOPinTypeGPIOOutput(LED_PORTN, LED1 | LED2);
    GPIOPinTypeGPIOOutput(LED_PORTF, LED3 | LED4);

    // Inicializa LEDs apagados
    GPIOPinWrite(LED_PORTN, LED1 | LED2, 0);
    GPIOPinWrite(LED_PORTF, LED3 | LED4, 0);
}


void SetupADC(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0));

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE));

    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);

    ADCSequenceConfigure(ADC0_BASE, ADC_SEQUENCER, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, ADC_SEQUENCER, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, ADC_SEQUENCER);
    ADCIntClear(ADC0_BASE, ADC_SEQUENCER);
}

void SetupTimer(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0));
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, SysClock / 100); // 10ms
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    TimerIntRegister(TIMER0_BASE, TIMER_A, Timer0IntHandler);
    TimerEnable(TIMER0_BASE, TIMER_A);
}

void Timer0IntHandler(void) {
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    uint32_t adcValue;
    ADCProcessorTrigger(ADC0_BASE, ADC_SEQUENCER);
    while (!ADCIntStatus(ADC0_BASE, ADC_SEQUENCER, false));
    ADCIntClear(ADC0_BASE, ADC_SEQUENCER);
    ADCSequenceDataGet(ADC0_BASE, ADC_SEQUENCER, &adcValue);

    g_ui32LDRValue = adcValue;
    g_bNewLDRValue = true; // Sinaliza que há um novo valor disponível
}

void ProcessLDRValue(uint32_t ldrValue) {
		uint32_t pwmPeriod = PWMGenPeriodGet(PWM0_BASE, PWM_GEN_2);	  

    if (ldrValue < 3000) {
				GPIOPinWrite(LED_PORTN, LED1, LED1);
				GPIOPinWrite(LED_PORTN, LED2, 0);
				GPIOPinWrite(LED_PORTF, LED3 | LED4, 0);
        g_ui32PWMDutyCycle = 0;
    } else if (ldrValue < 3500) {
				GPIOPinWrite(LED_PORTN, LED1, 0);
				GPIOPinWrite(LED_PORTN, LED2, LED2);
				GPIOPinWrite(LED_PORTF, LED3 | LED4, 0);
        g_ui32PWMDutyCycle = (pwmPeriod * 25) / 100;
    } else if (ldrValue < 4000) {
				GPIOPinWrite(LED_PORTN, LED1 | LED2, 0);
				GPIOPinWrite(LED_PORTF, LED3, LED3);
				GPIOPinWrite(LED_PORTF, LED4, 0);
        g_ui32PWMDutyCycle = (pwmPeriod * 50) / 100;
    } else {
				GPIOPinWrite(LED_PORTN, LED1 | LED2, 0);
				GPIOPinWrite(LED_PORTF, LED3, 0);
				GPIOPinWrite(LED_PORTF, LED4, LED4);
        g_ui32PWMDutyCycle = (pwmPeriod * 75) / 100;
    }
		
		char buffer[50];
		sprintf(buffer, "LDR Value: %u\r\n", g_ui32LDRValue);
		UARTSend(buffer);
		
		sprintf(buffer, "Duty cycle: %u\r\n", g_ui32PWMDutyCycle);
		UARTSend(buffer);

    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_5, g_ui32PWMDutyCycle);
}

void UARTSend(const char *pui8Buffer) {
    while (*pui8Buffer) {
        UARTCharPut(UART0_BASE, *pui8Buffer++);
    }
}
