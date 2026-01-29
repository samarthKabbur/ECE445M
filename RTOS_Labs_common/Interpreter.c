// *************Interpreter.c**************
// Students implement this as part of EE445M Lab 1,2,3,4,5,6 
// high level OS user interface
// Solution to labs 1,2,3,4,5,6
// Runs on MSPM0
// Jonathan W. Valvano 12/29/2025, valvano@mail.utexas.edu
#include <stdint.h>
#include <string.h> 
#include <stdio.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735_SDC.h"
#include "../inc/ADC.h"
#include "../RTOS_Labs_common/RTOS_UART.h"
#include "../RTOS_Labs_common/LPF.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/Interpreter.h"


void Interpreter(void) {  
  UART_OutString("\r\nhello world");


  UART_OutString("\r\nGreetings Userr\n");
  UART_OutString("Type '?' for help.\r\n");
  while (1) {                        // Loop forever
      UART_OutString("\r\n> "); 
      char cmdBuffer[32];
      uint32_t numInput;
      // Handle input
      UART_InString(cmdBuffer, 31);   // instring doesn't return until it sees a "carriage return"
      UART_OutString("\r\n");
      
      if (strcmp(cmdBuffer, "?") == 0 || strcmp(cmdBuffer, "help") == 0) {
          UART_OutString("Available Commands:\r\n");
          UART_OutString("  ?       : Show help menu\r\n");
          UART_OutString("  time    : Print current OS_MsTime\r\n");
          UART_OutString("  results : Run Lab1_Results()\r\n");
          UART_OutString("  num     : Test numerical I/O\r\n");
      } 

      // Time
      else if (strcmp(cmdBuffer, "time") == 0) {
          UART_OutString("System Time: ");
          UART_OutUDec(OS_MsTime());
          UART_OutString(" ms\r\n");
      }
      
      // results
      else if (strcmp(cmdBuffer, "results") == 0) {
          UART_OutString("Running Lab1_Results:\r\n");
          Lab1_Results(); 
      }

      // numerical io
      else if (strcmp(cmdBuffer, "num") == 0) {
          UART_OutString("Enter an integer: ");
          
          numInput = UART_InUDec();
          
          UART_OutString("\r\nNumber: ");
          UART_OutUDec(numInput);
          UART_OutString("\r\n");
      }
      
      // Errors
      else {
          UART_OutString("bad command: ");
          UART_OutString(cmdBuffer);
          UART_OutString("\r\n");
      }
  }
}
