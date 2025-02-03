#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "cmsis_os2.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/adc.h"

// Defini��es para o ADC e sensor
#define ADC_SEQUENCER     3         // Sequenciador do ADC para uma �nica amostra

// Defini��es para o c�lculo da m�dia
#define NUM_READINGS      10        // N�mero de leituras para m�dia
volatile uint32_t sensorReadings[NUM_READINGS] = {0}; // Vetor para armazenar as �ltimas 10 leituras
volatile uint8_t readingIndex = 0;                   // �ndice corrente para o vetor de leituras

// Objetos do RTOS
osMutexId_t sensorMutex;                // Mutex para acesso ao vetor sensorReadings
osMessageQueueId_t queueAverageResult;  // Fila para enviar o valor m�dio para a thread de UART

uint32_t SysClock;  // Frequ�ncia do sistema

// Configura��o da UART (mesma de antes)
void SetupUart(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0));
    UARTConfigSetExpClk(UART0_BASE, SysClock, 115200,
        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    UARTFIFODisable(UART0_BASE);
    // Configura��o dos pinos
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA));
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
}

void UARTSend(const char *pui8Buffer) {
    while (*pui8Buffer) {
        UARTCharPut(UART0_BASE, *pui8Buffer++);
    }
}

// Configura��o do ADC (conforme o c�digo original)
void SetupADC(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0));

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE));

    // Configura o pino para entrada anal�gica (no exemplo, PE3)
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);

    ADCSequenceConfigure(ADC0_BASE, ADC_SEQUENCER, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, ADC_SEQUENCER, 0,
                             ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, ADC_SEQUENCER);
    ADCIntClear(ADC0_BASE, ADC_SEQUENCER);
}

// Thread 1: Leitura do sensor a cada 0,5 s
void Thread_ReadSensor(void *argument) {
    (void) argument;
    while (true) {
        // Inicia a convers�o ADC
        ADCProcessorTrigger(ADC0_BASE, ADC_SEQUENCER);
        // Aguarda a conclus�o da convers�o
        while(!ADCIntStatus(ADC0_BASE, ADC_SEQUENCER, false));
        ADCIntClear(ADC0_BASE, ADC_SEQUENCER);
        uint32_t sensorValue = 0;
        ADCSequenceDataGet(ADC0_BASE, ADC_SEQUENCER, &sensorValue);

        // Armazena a leitura no vetor com prote��o do mutex
        osMutexAcquire(sensorMutex, osWaitForever);
        sensorReadings[readingIndex] = sensorValue;
        readingIndex = (readingIndex + 1) % NUM_READINGS;
        osMutexRelease(sensorMutex);

        osDelay(500);  // Delay de 500 ms
    }
}

// Thread 2: Calcula a m�dia dos 10 �ltimos valores
void Thread_Average(void *argument) {
    (void) argument;
    while (true) {
        uint32_t sum = 0;
        // Acesso protegido �s leituras
        osMutexAcquire(sensorMutex, osWaitForever);
				char buffer[50];
        for (uint8_t i = 0; i < NUM_READINGS; i++) {
						
						sprintf(buffer, "LDR Value: %u\r\n", sensorReadings[i]);
						UARTSend(buffer);
					
            sum += sensorReadings[i];
        }
        osMutexRelease(sensorMutex);
        uint32_t average = sum / NUM_READINGS;

        // Envia a m�dia para a fila que a thread de UART ir� imprimir
        osMessageQueuePut(queueAverageResult, &average, 0, 0);

        osDelay(500);  
    }
}

// Thread 3: Imprime os dados m�dios na UART
void Thread_UARTWrite(void *argument) {
    (void) argument;
    uint32_t average;
    while (true) {
        // Espera pela m�dia na fila (bloqueia at� receber)
        if (osMessageQueueGet(queueAverageResult, &average, NULL, osWaitForever) == osOK) {
            char buffer[50];
            snprintf(buffer, sizeof(buffer), "Media: %u\r\n", average);
            // Envia a string para a UART caractere a caractere
            for (char *p = buffer; *p; p++) {
                while (!UARTCharPutNonBlocking(UART0_BASE, *p));
            }
        }
    }
}

int main(void) {
    // Configura o clock do sistema
    SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN |
                                   SYSCTL_USE_PLL | SYSCTL_CFG_VCO_240),
                                   120000000);
    
    SetupUart();
    SetupADC();

    // Inicializa o kernel do RTOS
    osKernelInitialize();

    // Cria o mutex para proteger o vetor de leituras
    sensorMutex = osMutexNew(NULL);
    // Cria a fila para enviar os valores m�dios
    queueAverageResult = osMessageQueueNew(10, sizeof(uint32_t), NULL);

    // Cria as threads
    osThreadNew(Thread_ReadSensor, NULL, NULL);
    osThreadNew(Thread_Average, NULL, NULL);
    osThreadNew(Thread_UARTWrite, NULL, NULL);

    // Inicia o kernel do RTOS
    osKernelStart();
    
    while (1);
}
