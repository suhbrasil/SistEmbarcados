#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "driverlib/pwm.h"
#include "driverlib/adc.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"

// Configuração dos LEDs
#define LED_PORTN GPIO_PORTN_BASE
#define LED_PORTF GPIO_PORTF_BASE
#define LED1 GPIO_PIN_1
#define LED2 GPIO_PIN_0
#define LED3 GPIO_PIN_4
#define LED4 GPIO_PIN_0

// Configuração do ADC e PWM
#define ADC_SEQUENCER 3 // Sequenciador do ADC para uma única amostra
#define PWM_FREQUENCY 12000
#define MAX_TEMP 100
#define MIN_TEMP 0

uint32_t SysClock;
volatile uint32_t g_ui32PWMDutyCycle = 0; // Duty cycle do PWM
volatile uint32_t g_ui32Temperature = 0; // Temperatura lida do LM35

void UARTIntHandler(void);
void Timer0IntHandler(void);
void SetupUart(void);
void SetupTimer(void);
void SetupADC(void);
void SetupPWM(void);
void SetupLEDs(void);
void UpdateLEDs(uint32_t temperature);
void UARTSend(const char *pui8Buffer);

int main(void)
{
    SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN |
                                   SYSCTL_USE_PLL | SYSCTL_CFG_VCO_240), 120000000);

    SetupUart();
    SetupTimer();
    SetupADC();
    SetupPWM();
    SetupLEDs();

    while (1)
    {
        // Loop principal
    }
}

void SetupPWM(void)
{
    // Habilita PWM e o port do PWM
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);

    GPIOPinTypePWM(GPIO_PORTG_BASE, GPIO_PIN_1);
    GPIOPinConfigure(GPIO_PG1_M0PWM5);

    SysCtlPWMClockSet(SYSCTL_PWMDIV_2);

    uint32_t pwmPeriod = SysClock / (PWM_FREQUENCY * 2);
    PWMGenConfigure(PWM0_BASE, PWM_GEN_2, PWM_GEN_MODE_DOWN);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_2, pwmPeriod);
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_5, 0); // Inicializa com 0% duty cycle
    PWMOutputState(PWM0_BASE, PWM_OUT_5_BIT, true);
    PWMGenEnable(PWM0_BASE, PWM_GEN_2);
}

void SetupADC(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0));
    ADCSequenceConfigure(ADC0_BASE, ADC_SEQUENCER, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, ADC_SEQUENCER, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, ADC_SEQUENCER);
    ADCIntClear(ADC0_BASE, ADC_SEQUENCER);
}

void SetupTimer(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0));
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    uint32_t timerPeriod = SysClock / 10; // Para ~100ms
    TimerLoadSet(TIMER0_BASE, TIMER_A, timerPeriod - 1);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    TimerIntRegister(TIMER0_BASE, TIMER_A, Timer0IntHandler);
    TimerEnable(TIMER0_BASE, TIMER_A);
}

void Timer0IntHandler(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT); // Limpa a interrupção do Timer

    // Inicia uma conversão ADC
    ADCProcessorTrigger(ADC0_BASE, ADC_SEQUENCER);
    while (!ADCIntStatus(ADC0_BASE, ADC_SEQUENCER, false));
    uint32_t adcValue;
    ADCSequenceDataGet(ADC0_BASE, ADC_SEQUENCER, &adcValue);
    ADCIntClear(ADC0_BASE, ADC_SEQUENCER);

    // Converte valor ADC para temperatura
    float voltage = (adcValue * 3.3) / 4095.0; // Referência ADC: 3.3V
    g_ui32Temperature = voltage * 100.0;      // LM35: 10 mV/°C

    // Ajusta o duty cycle com base na temperatura
    if (g_ui32Temperature > MAX_TEMP)
        g_ui32Temperature = MAX_TEMP;
    else if (g_ui32Temperature < MIN_TEMP)
        g_ui32Temperature = MIN_TEMP;

    g_ui32PWMDutyCycle = (g_ui32Temperature * PWMGenPeriodGet(PWM0_BASE, PWM_GEN_2)) / MAX_TEMP;
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_5, g_ui32PWMDutyCycle);

    // Atualiza os LEDs com base na temperatura
    UpdateLEDs(g_ui32Temperature);

    char buffer[50];
    sprintf(buffer, "Temp: %u°C, Duty: %u\r\n", g_ui32Temperature, g_ui32PWMDutyCycle);
    UARTSend(buffer);
}

void SetupLEDs(void)
{
    // Habilita GPIO para os LEDs
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPION));
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF));

    GPIOPinTypeGPIOOutput(LED_PORTN, LED1 | LED2); // Configura LEDs 1 e 2 como saída
    GPIOPinTypeGPIOOutput(LED_PORTF, LED3 | LED4); // Configura LEDs 3 e 4 como saída

    // Desliga todos os LEDs inicialmente
    GPIOPinWrite(LED_PORTN, LED1 | LED2, 0);
    GPIOPinWrite(LED_PORTF, LED3 | LED4, 0);
}

void UpdateLEDs(uint32_t temperature)
{
    // Atualiza LEDs com base na temperatura
    if (temperature < 25)
    {
        GPIOPinWrite(LED_PORTN, LED1, LED1); // Acende LED 1
        GPIOPinWrite(LED_PORTN, LED2, 0);
        GPIOPinWrite(LED_PORTF, LED3 | LED4, 0);
    }
    else if (temperature >= 25 && temperature <= 30)
    {
        GPIOPinWrite(LED_PORTN, LED1, 0);
        GPIOPinWrite(LED_PORTN, LED2, LED2); // Acende LED 2
        GPIOPinWrite(LED_PORTF, LED3 | LED4, 0);
    }
    else if (temperature > 30 && temperature <= 35)
    {
        GPIOPinWrite(LED_PORTN, LED1 | LED2, 0);
        GPIOPinWrite(LED_PORTF, LED3, LED3); // Acende LED 3
        GPIOPinWrite(LED_PORTF, LED4, 0);
    }
    else
    {
        GPIOPinWrite(LED_PORTN, LED1 | LED2, 0);
        GPIOPinWrite(LED_PORTF, LED3, 0);
        GPIOPinWrite(LED_PORTF, LED4, LED4); // Acende LED 4
    }
}

void SetupUart(void)
{
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

void UARTIntHandler(void)
{
    uint32_t status = UARTIntStatus(UART0_BASE, true);
    UARTIntClear(UART0_BASE, status);
}

void UARTSend(const char *pui8Buffer)
{
    // Envia strings pela UART
    while (*pui8Buffer)
    {
        UARTCharPut(UART0_BASE, *pui8Buffer++);
    }
}
