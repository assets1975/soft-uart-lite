/*
 Software Uart (Lite)
 By Liyanboy74
 https://github.com/liyanboy74
*/

#include "softuart.h"

// All Soft Uart Config and State
SoftUart_S       	SUart   [Number_Of_SoftUarts];

// TX RX Data Buffer
SoftUartBuffer_S 	SUBuffer[Number_Of_SoftUarts];

// Read RX single Pin Value
GPIO_PinState SoftUartGpioReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	return HAL_GPIO_ReadPin(GPIOx,GPIO_Pin);
}

// Write TX single Pin Value
void SoftUartGpioWritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState)
{
	HAL_GPIO_WritePin(GPIOx,GPIO_Pin,PinState);
}

// Initial Soft Uart
SoftUartState_E SoftUartInit(uint8_t SoftUartNumber,GPIO_TypeDef *TxPort,uint16_t TxPin,GPIO_TypeDef *RxPort,uint16_t RxPin)
{
	if(SoftUartNumber>=Number_Of_SoftUarts)return SoftUart_Error;
	
	SUart[SoftUartNumber].TxNComplated=0;
	
	SUart[SoftUartNumber].RxBitCounter=0;
	SUart[SoftUartNumber].RxBitShift=0;
	SUart[SoftUartNumber].RxIndex=0;

	SUart[SoftUartNumber].TxEnable=0;
	SUart[SoftUartNumber].RxEnable=0;
	
	SUart[SoftUartNumber].TxBitCounter=0;
	SUart[SoftUartNumber].TxBitShift=0;
	SUart[SoftUartNumber].TxIndex=0;
	
	SUart[SoftUartNumber].TxSize=0;
	
	SUart[SoftUartNumber].Buffer=&SUBuffer[SoftUartNumber];
	
	SUart[SoftUartNumber].RxPort=RxPort;
	SUart[SoftUartNumber].RxPin=RxPin;
	
	SUart[SoftUartNumber].TxPort=TxPort;
	SUart[SoftUartNumber].TxPin=TxPin;
	
	SUart[SoftUartNumber].RxTimingFlag=0;
	SUart[SoftUartNumber].RxBitOffset=0;
	
	return SoftUart_OK;
}

// Send one bit to TX pin
void SoftUartTransmitBit(SoftUart_S *SU,uint8_t Bit0_1)
{
	SoftUartGpioWritePin(SU->TxPort,SU->TxPin,(GPIO_PinState)Bit0_1);
}

// Enable Soft Uart Receiving
SoftUartState_E SoftUartEnableRx(uint8_t SoftUartNumber)
{
	if(SoftUartNumber>=Number_Of_SoftUarts)return SoftUart_Error;
	SUart[SoftUartNumber].RxEnable=1;
	return SoftUart_OK;
}

// Disable Soft Uart Receiving
SoftUartState_E SoftUartDisableRx(uint8_t SoftUartNumber)
{
	if(SoftUartNumber>=Number_Of_SoftUarts)return SoftUart_Error;
	SUart[SoftUartNumber].RxEnable=0;
	return SoftUart_OK;
}

// Read Size of Received Data in buffer
uint8_t SoftUartRxAlavailable(uint8_t SoftUartNumber)
{
	return SUart[SoftUartNumber].RxIndex;
}

// Move Received Data to Another Buffer
SoftUartState_E SoftUartReadRxBuffer(uint8_t SoftUartNumber,uint8_t *Buffer,uint8_t Len)
{
	int i;
	if(SoftUartNumber>=Number_Of_SoftUarts)return SoftUart_Error;
	for(i=0;i<Len;i++)
	{
		Buffer[i]=SUart[SoftUartNumber].Buffer->Rx[i];
	}
	for(i=0;i<SUart[SoftUartNumber].RxIndex;i++)
	{
		SUart[SoftUartNumber].Buffer->Rx[i]=SUart[SoftUartNumber].Buffer->Rx[i+Len];
	}
	SUart[SoftUartNumber].RxIndex-=Len;
	return SoftUart_OK;
}

// Soft Uart Transmit Data Process
void SoftUartTxProcess(SoftUart_S *SU)
{
	if(SU->TxEnable)
	{
		// Start
		if(SU->TxBitCounter==0)
		{
			SU->TxNComplated=1;
			SU->TxBitShift=0;
			SoftUartTransmitBit(SU,0);
			SU->TxBitCounter++;
		}
		// Data
		else if(SU->TxBitCounter<9)
		{
			SoftUartTransmitBit(SU,((SU->Buffer->Tx[SU->TxIndex])>>(SU->TxBitShift))&0x01);
			SU->TxBitCounter++;
			SU->TxBitShift++;
		}
		// Stop
		else if(SU->TxBitCounter==9)
		{
			SoftUartTransmitBit(SU,1);
			SU->TxBitCounter++;
		}
		//Complete
		else if(SU->TxBitCounter==10)
		{
			// Reset Bit Counter
			SU->TxBitCounter=0;
			
			// Ready To Send Another Data
			SU->TxIndex++;

			// Check Size of Data
			if(SU->TxSize > SU->TxIndex)
			{
				// Continue Sending 
				SU->TxNComplated=1;
				SU->TxEnable=1;
			}
			else
			{
				// Finish
				SU->TxNComplated=0;
				SU->TxEnable=0;
			}
		}
	}
}

// Soft Uart Receive Data Process
void SoftUartRxDataBitProcess(SoftUart_S *SU,uint8_t B0_1)
{
	if(SU->RxEnable)
	{
		// Start
		if(SU->RxBitCounter==0)
		{
			// Start Bit is 0
			if(B0_1)return;

			SU->RxBitShift=0;
			SU->RxBitCounter++;
			SU->Buffer->Rx[SU->RxIndex]=0;
		}
		// Data
		else if(SU->RxBitCounter<9)
		{
			SU->Buffer->Rx[SU->RxIndex]|=((B0_1&0x01)<<SU->RxBitShift);
			SU->RxBitCounter++;
			SU->RxBitShift++;
		}
		// Stop and Complete
		else if(SU->RxBitCounter==9)
		{
			SU->RxBitCounter=0;
			SU->RxTimingFlag=0;

			//Stop Bit must be 1
			if(B0_1)
			{
				// Received successfully
				// Change RX Buffer Index
				if((SU->RxIndex)<(SoftUartRxBufferSize-1))(SU->RxIndex)++;
			}
			// if not : ERROR -> Overwrite data
		}
	}
}

// Wait Until Transmit Completed
// You do not usually need to use this function!
void SoftUartWaitUntilTxComplate(uint8_t SoftUartNumber)
{
	while(SUart[SoftUartNumber].TxNComplated);
}

// Copy Data to Transmit Buffer and Start Sending
SoftUartState_E SoftUartPuts(uint8_t SoftUartNumber,uint8_t *Str,uint8_t Len)
{
	int i;

	if(SoftUartNumber>=Number_Of_SoftUarts)return SoftUart_Error;
	if(SUart[SoftUartNumber].TxNComplated) return SoftUart_Error;
	
	SUart[SoftUartNumber].TxIndex=0;
	SUart[SoftUartNumber].TxSize=Len;
	
	for(i=0;i<Len;i++)
	{
		SUart[SoftUartNumber].Buffer->Tx[i]= Str[i];
	}
	
	SUart[SoftUartNumber].TxNComplated=1;
	SUart[SoftUartNumber].TxEnable=1;
	
	return SoftUart_OK;
}

// SoftUartHandler must call in interrupt every 1*(1/BR)
// if BR=9600 then 1*(1/9600)=104.16 uS
void SoftUartHandler(void)
{
	int i;
	
	for(i=0;i<Number_Of_SoftUarts;i++)
	{
		if(SUart[i].TxEnable)SoftUartTxProcess(&SUart[i]);
	}
	
	for(i=0;i<Number_Of_SoftUarts;i++)
	{
		if(SUart[i].RxTimingFlag)SoftUartRxDataBitProcess(&SUart[i],SoftUartGpioReadPin(SUart[i].RxPort,SUart[i].RxPin));
	}
}

void SoftUartRxHelper(uint8_t ID)
{
	if(!SUart[ID].RxTimingFlag && SUart[ID].RxEnable)
	{
		htim3.Instance->CNT=52;	//Half OF Timer Period
		SUart[ID].RxTimingFlag=1;
		__HAL_TIM_CLEAR_IT(&htim3,TIM_IT_UPDATE);
	}
}