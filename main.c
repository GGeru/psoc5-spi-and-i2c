/* ========================================
 *
 * Kerttu Kautto
 * April 28 2022
 * 
 * Temperature sensor project with CY8CKIT-059 using SPI and I2C.
 * 
 * This project reads temperature from a LM36 analog temperature sensor as
 * millivolts, counts the boxcar average of the recieved data during a 0.5s
 * period, and uses a MCP3201 ADC converter to convert it to DC. 
 * It also measures the temperature through a TC74A2-5.0VAT digital sensor
 * (I2C) and the ADC converter as a SPI device. The project uses interrupts to
 * aquire data from the SPI and ADC converter. It outputs all of the four
 * values to UART com port as JSON data in 0.5s intervals.
 *
 * ========================================
 */

#include <project.h>
#include "stdio.h"

#define FALSE  0
#define TRUE   1
#define TRANSMIT_BUFFER_SIZE  40

/* Flags for the clock interrupt */
uint8 count_flag;
uint8 spi_flag;

CY_ISR_PROTO(clockISR);     /* Proto of the clock ISR for calculations */
CY_ISR_PROTO(spiISR);       /* Proto of spi ISR */

/******************************************************************************
* Function Name: main
*******************************************************************************
*
* Summary:
*  main() performs following functions:
*  1: Starts the ADC, UART and Timer components.
*  2: Checks if SPI data is transferred and stores it
*  3: Checks for ADC end of conversion.  Stores latest result in output
*     if conversion complete.
*  4: outputs all of the 4 variables to UART com port.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
******************************************************************************/



int main()
{
    volatile uint16 sSPIval;
    volatile uint16 sI2Cval;
    int TC74addr = 0x4a;
    
    
    int dec_first;      /* First part of the temperature float */
    int dec_last;       /* Last part of the temperature float */
    int temp;           /* The calculated temperature in millivolts */
    int temp_sum;       /* Sum of the temperatures of a clock cycle */
    uint32 Output;      /* Variable to store ADC result */
    uint32 sample_amnt; /* Amount of samples in the clock cycle */
    uint8 Ch;           /* Variable to store UART received character */
    uint8 EmulatedData; /* Variable used to send emulated data */
    char TransmitBuffer[TRANSMIT_BUFFER_SIZE];
    
    /* Start the components */
    ADC_DelSig_1_Start();
    UART_1_Start();
    Timer_1_Start();
    SPIM_Start();
    I2C_Start();

    /* Start the ISR */
    isr_clock_StartEx(clockISR);
    isr_SPI_StartEx(spiISR);
   
    /* Initialize Variables */
    sample_amnt = 1;
    EmulatedData = 0;
    count_flag = 0;
    spi_flag = 0;
    temp_sum = 0;
    sSPIval = 0x0000;
    sI2Cval = 0x0000;
    
    /* Start the ADC conversion */
    ADC_DelSig_1_StartConvert();
    
    /* Send message to verify COM port is connected properly */
    UART_1_PutString("COM Port Open\r\n");
    
    /* Enable global interrupts */
    CyGlobalIntEnable;
    
    for(;;) {      
        if (spi_flag == 1) {
            CyGlobalIntDisable;
            //write something (16bit data)
            SPIM_WriteTxData(sSPIval);            
            //read the rx buffer. use interrupts for this
            sSPIval = SPIM_ReadRxData();
            //12 bit masking so the highest bit goes to the place of 0
            sSPIval = sSPIval & 0x0fff;
            CyDelay(100);
            
            spi_flag = 0;
            CyGlobalIntEnable;
        }
        
        /* I2C */
        I2C_MasterSendStart(TC74addr, I2C_WRITE_XFER_MODE);
        I2C_MasterWriteByte(0x00);
        I2C_MasterSendRestart(TC74addr, I2C_READ_XFER_MODE);
        sI2Cval = I2C_MasterReadByte(I2C_NAK_DATA); /* Change to read mode */
        I2C_MasterSendStop();                       /* Stop I2C */
        
        /* Check to see if the ADC conversion has completed */
        if(ADC_DelSig_1_IsEndConversion(ADC_DelSig_1_RETURN_STATUS))
        {
            /* Use the GetResult16 API to get an 8 bit unsigned result in
             * single ended mode.  The API CountsTo_mVolts is then used
             * to convert the ADC counts into mV.
             */
            Output = ADC_DelSig_1_CountsTo_mVolts(ADC_DelSig_1_GetResult16());
            /* Sum up the readings from this clock cycle */
            temp_sum += Output;
            sample_amnt++;
        }    
        
        if (count_flag == 1) {
            /* Disable global interrupts during the calculations */
            CyGlobalIntDisable;
            /* Count the boxcar average of the temperatures 
             * during a 0.5s long period. 
             */
            temp = temp_sum / sample_amnt;
 
            /* Convert the temperature to celcius */
            dec_first = temp / 10;
            dec_last = temp % 10;
            
            /* Format ADC result for transmition */
            sprintf(TransmitBuffer, "{ \"ADC\":%d, \"LMD35\":%d.%d,
                \"SPI\":%d.%d, \"I2C\":%d }\r\n", temp, dec_first, dec_last,
                (sSPIval / 10), (sSPIval % 10), sI2Cval);
            
            UART_1_PutString(TransmitBuffer);   /* Send out the data */
            
            /* Reset counter variables and flag */
            temp_sum = 0;
            sample_amnt = 0;
            count_flag = 0;
            CyGlobalIntEnable;
        }
    }
}

/* Interrupt code for the clock. Called every 0.5 seconds */
CY_ISR(clockISR)
{
    count_flag = 1;
    Timer_1_ReadStatusRegister();   /* Clear the pending interrupt */
}

/* Interrupt code for the SPI. Called when data transmission is over */
CY_ISR(spiISR)
{
    spi_flag = 1;
    SPIM_ReadStatus();              /* Clear the pending interrupt */
}

/* [] END OF FILE */
