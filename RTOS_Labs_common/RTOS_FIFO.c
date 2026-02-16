// RTOS_FIFO.c
// Runs on any Microcontroller
// Provide functions that initialize a FIFO, put data in, get data out,
// and return the current size.  The FIFO
// uses index implementation.
// Daniel Valvano
// June 20, 2025

/* 
 Copyright 2025 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */
#include <ti/devices/msp/msp.h>
#include "../RTOS_Labs_common/RTOS_UART.h"
#include "../RTOS_Labs_common/RTOS_FIFO.h"
#include "../RTOS_Labs_common/OS.h"
// Two-index implementation of the transmit FIFO
// can hold 0 to TXFIFOSIZE-1 elements
uint32_t volatile TxPutI; // where to put next
uint32_t volatile TxGetI; // where to get next
char static TxFifo[TXFIFOSIZE];

//for debugging 
#define TogglePA8() (GPIOA->DOUTTGL31_0 = (1<<8))
#define TogglePA9() (GPIOA->DOUTTGL31_0 = (1<<9))
#define TogglePA16() (GPIOA->DOUTTGL31_0 = (1<<16))
#define TogglePB4() (GPIOB->DOUTTGL31_0 = (1<<4))
#define TogglePB1() (GPIOB->DOUTTGL31_0 = (1<<1))
#define TogglePB20() (GPIOB->DOUTTGL31_0 = (1<<20))

fifo_semaphores_t rx_fifo_semaphore;
fifo_semaphores_t tx_fifo_semaphore;

void TxFifo_Init(void){
  TxPutI = TxGetI = 0; // empty
  OS_InitSemaphore(&tx_fifo_semaphore.current_size, 0);
  OS_InitSemaphore(&tx_fifo_semaphore.mutex, 1);
}
int TxFifo_Put(char data){
  uint32_t newPutI = (TxPutI+1)&(TXFIFOSIZE-1);
  if(newPutI == TxGetI) return 0; // fail if full
  TxFifo[TxPutI] = data;          // save in Fifo
  TxPutI = newPutI;               // next place to put

  OS_Signal(&tx_fifo_semaphore.current_size);

  return 1;
}
char TxFifo_Get(void){char data;
  OS_Wait(&tx_fifo_semaphore.current_size); // block if full
  OS_Wait(&tx_fifo_semaphore.mutex); // block if busy

  data = TxFifo[TxGetI];              // retrieve data
  TxGetI = (TxGetI+1)&(TXFIFOSIZE-1); // next place to get

  OS_Signal(&tx_fifo_semaphore.mutex);

  return data;
}

uint32_t TxFifo_Size(void){
  return (TxPutI-TxGetI)&(TXFIFOSIZE-1);
}

// Two-index implementation of the receive FIFO
// can hold 0 to RXFIFOSIZE-1 elements

uint32_t volatile RxPutI; // where to put next
uint32_t volatile RxGetI; // where to get next
char static RxFifo[RXFIFOSIZE];

void RxFifo_Init(void){
  RxPutI = RxGetI = 0;  // empty
  OS_InitSemaphore(&rx_fifo_semaphore.current_size, 0);
  OS_InitSemaphore(&rx_fifo_semaphore.mutex, 1);
}
int RxFifo_Put(char data){
  uint32_t newPutI = (RxPutI+1)&(RXFIFOSIZE-1);
  if(newPutI == RxGetI) return 0; // fail if full
  TogglePB4();
  RxFifo[RxPutI] = data;          // save in Fifo
  RxPutI = newPutI;               // next place to put

  OS_Signal(&rx_fifo_semaphore.current_size); // something was added to fifo

  return 1;
}
char RxFifo_Get(void){char data;
  OS_Wait(&rx_fifo_semaphore.current_size); // block if full
  OS_Wait(&rx_fifo_semaphore.mutex); // block if busy
  TogglePA9();
  data = RxFifo[RxGetI];              // retrieve data
  RxGetI = (RxGetI+1)&(RXFIFOSIZE-1); // next place to get

  OS_Signal(&rx_fifo_semaphore.mutex); // unblock fifo

  return data;
}

uint32_t RxFifo_Size(void){
  return (RxPutI-RxGetI)&(RXFIFOSIZE-1);
}

