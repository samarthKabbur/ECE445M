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

extern void Lab1_Results(uint32_t d);
extern void Lab2(void);
extern void DFT(void);
extern void Jitter(void);

typedef struct{
  const char *name;
  void (*handler)(char *args, int l);
} Command ;

void Cmd_Flush(char* args, int l);
void Cmd_Print(char* args, int l);
void Cmd_Lab1_Results(char* args, int l);
void Cmd_Lab2_Results(char* args, int l);
void Cmd_DFT(char* args, int l);
void Cmd_Jitter(char* args, int l);
void Cmd_Help(char* args, int l);

Command commands[] = {
  {"flush", Cmd_Flush},
  {"print", Cmd_Print},
  {"lab1results", Cmd_Lab1_Results},
  {"lab2results", Cmd_Lab2_Results},
  {"jitter", Cmd_Jitter},
  {"dft", Cmd_DFT},
  {"?", Cmd_Help},
};

#define NUM_COMMANDS (sizeof(commands)/sizeof(Command))
#define CARROT 62
#define NEW_LINE 10
#define NULL_CHAR '\0'

void Cmd_Flush(char* args, int l){
ST7735_FillScreen(ST7735_BLACK);
}


void Cmd_Print(char* args, int l){
  if(args == 0){
      UART_OutString("Error: No message provided\n");
      return;
  }

  UART_OutString(args);
  UART_OutChar('\n');
  ST7735_FillScreen(ST7735_BLACK);
  ST7735_DrawString(0, l, args, ST7735_YELLOW);
}


void Cmd_Lab1_Results(char* args, int l){
  // Lab1_Results(1);
  UART_OutString("\n");
}

void Cmd_Lab2_Results(char* args, int l){
  Lab2();
  UART_OutString("\n");
}

void Cmd_DFT(char* args, int l){
  DFT();
  UART_OutString("\n");
}

void Cmd_Jitter(char* args, int l){
  Jitter();
  UART_OutString("\n");
}


void Cmd_Help(char* args, int l){
  UART_OutString("\r\nflush                 clears screen");
  UART_OutString("\r\nprint *message*       prints message onto screen");
  UART_OutString("\r\nlab1results           prints lab 1 results onto screen");
  UART_OutString("\r\nlab2results           prints lab 2 results onto screen");
  UART_OutString("\r\ndft                   prints lab 2 dft results onto screen");
  UART_OutString("\r\njitter                prints lab 2  jitter results onto screen");
  UART_OutString("\r\n?                     displays syntax of all commands");
  UART_OutString("\n");
  ST7735_FillScreen(ST7735_BLACK);
  // ST7735_DrawString(0, l, "flush", ST7735_YELLOW);
  // l++;
  // ST7735_DrawString(0, l, "print *message*", ST7735_YELLOW);
  // l++;
  // ST7735_DrawString(0, l, "lab1results", ST7735_YELLOW);
  // l++;
  // ST7735_DrawString(0, l, "?", ST7735_YELLOW);
  
  

  
}


void ParseArgs(char *string, int l){
  char *cmd = string;
  char *args = 0;
  int i = 0;
  while(string[i] != NULL_CHAR){
    if(string[i] == ' '){
      string[i] = NULL_CHAR;
      args = &string[i+1];
      break;
    }
    i++;
  }

  for(int i = 0; i < NUM_COMMANDS; i++){
    if(strcmp(cmd, commands[i].name) == 0){
      commands[i].handler(args, l);
      return;
    }
  }

  UART_OutString("Unknown command. Enter ? to display commands\n");


}

//new Interpreter old one is below this just incase this doesnt work
void Interpreter(void) {  
  char string[50]; // didnt need to do this couldve just used InString(); will change later on
  int stringIndex = 0;
  int l = 3; //line number

  UART_OutChar(CARROT); //begin by printing arrow

  while (1) {                        // Loop forever
    UART_InString(string, 50);
    ParseArgs(string, l);
  }
}
// void Interpreter(void) {
//     char string[50];
//     int stringIndex = 0;
//     int l = 3; 

//     UART_OutChar(CARROT); 

//     while (1) {
//         uint8_t letter = UART_InChar();

//         if (letter == 13 || letter == 10) { 
//             if (stringIndex > 0) {
//                 UART_OutChar(NEW_LINE);
//                 string[stringIndex] = NULL_CHAR;
                
//                 ParseArgs(string, l);
                
//                 stringIndex = 0;
//                 l++; 
//                 UART_OutChar(CARROT);
//             }
//         }
//         else if (letter == 127 || letter == 8) {
//             if (stringIndex > 0) {
//                 stringIndex--;
//                 UART_OutChar(8);
//                 UART_OutChar(' '); // Erase char
//                 UART_OutChar(8);   // Move cursor back again
//             }
//         }
//         else {
//             if (stringIndex < 49) { // Leave room for NULL terminator
//                 UART_OutChar(letter);
//                 string[stringIndex] = letter; 
//                 stringIndex++;
//             }
//         }
//     }
// }