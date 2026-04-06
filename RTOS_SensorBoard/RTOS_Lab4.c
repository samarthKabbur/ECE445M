/* RTOS_Lab4.c
 * Jonathan Valvano
 * December 30, 2025
 * Remove 3.3V J101 jumper to run RTOS sensor board or motor board
 * A two-pin female header is required on the LaunchPad TP10(XDS_VCC) and TP9(!RSTN)
 */

#include <ti/devices/msp/msp.h>
#include "../inc/LaunchPad.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../inc/Clock.h"
#include "../RTOS_Labs_common/ST7735_SDC.h"
#include "../RTOS_Labs_common/RTOS_UART.h"
#include "../RTOS_Labs_common/Interpreter.h"
#include "../RTOS_Labs_common/IRDistance.h"
#include "../RTOS_Labs_common/LPF.h"
#include "../RTOS_Labs_common/DFT16.h"
#include "../RTOS_Labs_common/TFLuna1.h"
#include "../RTOS_Labs_common/TFLuna2.h"
#include "../RTOS_Labs_common/TFLuna3.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/fixed.h"
#include "../RTOS_Labs_common/CAN.h"
#include <stdio.h>
// PA10 is UART0 Tx    index 20 in IOMUX PINCM table
// PA11 is UART0 Rx    index 21 in IOMUX PINCM table
// Insert jumper J25: Connects PA10 to XDS_UART
// Insert jumper J26: Connects PA11 to XDS_UART
//  PA0 is red LED1,   index 0 in IOMUX PINCM table, negative logic
// PB22 is BLUE LED2,  index 49 in IOMUX PINCM table
// PB26 is RED LED2,   index 56 in IOMUX PINCM table
// PB27 is GREEN LED2, index 57 in IOMUX PINCM table
// PA18 is S1 positive logic switch,  conflict with TFLuna1, so S1 will not be used
// PB21 is S2 negative logic switch,  used for aperiodic task
// IR analog distance sensors
//   30 cm GP2Y0A41SK0F or 80 cm long range GP2Y0A21YK0F 
//   PA26 Right  ADC0_1
//   PA24 Center ADC0_3, used in Labs 1,2,3,4
//   PA22 Left   ADC0_7
//   PA27 Extra  ADC0_0

// RTOS sensor board supported three TF-Luna sensors
//    Serial TxD: PA17 is UART1 Tx (MSPM0 to TFLuna1)
//    Serial RxD: PA18 is UART1 Rx (TFLuna1 to MSPM0), conflict with LaunchPad S1
//    Serial TxD: PB17 is UART2 Tx (MSPM0 to TFLuna2), used in Labs 1,2,3,4
//    Serial RxD: PB18 is UART2 Rx (TFLuna2 to MSPM0), used in Labs 1,2,3,4
//    Serial TxD: PB12 is UART3 Tx (MSPM0 to TFLuna3), 
//    Serial RxD: PB13 is UART3 Rx (TFLuna3 to MSPM0), shared with LD19 Lidar 
//UART3 is shared between LD19 and TFLuna3 (can have either but not both)

// **** OS must run disk_timerproc();  at 1000Hz, every 1ms *****
uint32_t Running;           // true while robot is running
uint32_t NumCreated;   // number of foreground threads created

extern fifo_t tfluna1_fifo;
extern fifo_t tfluna2_fifo;
extern fifo_t tfluna3_fifo;
extern fifo_t ir1_fifo;
extern fifo_t ir2_fifo;

Median5_data_t tfluna1_median_data;
Median5_data_t tfluna2_median_data;
Median5_data_t tfluna3_median_data;

//---------------------User debugging-----------------------

// Unused sensor board pins, made outputs for debugging
// Jumper J14 select PA9
// Jumper J15 select PA16
void Logic_Init(void){
  IOMUX->SECCFG.PINCM[PA8INDEX] = (uint32_t) 0x00000081;
  IOMUX->SECCFG.PINCM[PA9INDEX] = (uint32_t) 0x00000081;
  IOMUX->SECCFG.PINCM[PA16INDEX] = (uint32_t) 0x00000081;
  IOMUX->SECCFG.PINCM[PB4INDEX] = (uint32_t) 0x00000081;
  IOMUX->SECCFG.PINCM[PB1INDEX] = (uint32_t) 0x00000081;
  IOMUX->SECCFG.PINCM[PB20INDEX] = (uint32_t) 0x00000081;
  GPIOA->DOE31_0 |= (1<<8)|(1<<9)|(1<<16);
  GPIOB->DOE31_0 |= (1<<4)|(1<<1)|(1<<20);
}
#define TogglePA8() (GPIOA->DOUTTGL31_0 = (1<<8))
#define SetPA8() (GPIOA->DOUTSET31_0 = (1<<8))
#define ClrPA8() (GPIOA->DOUTCLR31_0 = (1<<8))
#define TogglePA9() (GPIOA->DOUTTGL31_0 = (1<<9))
#define SetPA9() (GPIOA->DOUTSET31_0 = (1<<9))
#define ClrPA9() (GPIOA->DOUTCLR31_0 = (1<<9))
#define TogglePA16() (GPIOA->DOUTTGL31_0 = (1<<16))
#define TogglePB4() (GPIOB->DOUTTGL31_0 = (1<<4))
#define SetPB4() (GPIOB->DOUTSET31_0 = (1<<4))
#define ClrPB4() (GPIOB->DOUTCLR31_0 = (1<<4))
#define TogglePB1() (GPIOB->DOUTTGL31_0 = (1<<1))
#define TogglePB20() (GPIOB->DOUTTGL31_0 = (1<<20))

uint32_t Checks; // number of times virus checking has run
uint32_t ChecksWork; // number of checks in 10 second
//------------------Task 1--------------------------------
// real-time sampling ADC0 channel 3, using software start trigger
// 60-Hz notch high-Q, IIR filter, assuming fs=1000 Hz
// y(n) = (256x(n) -476x(n-1) + 256x(n-2) + 471y(n-1)-251y(n-2))/256 (1k sampling)
#define PERIOD TIME_1MS      // DAS 1kHz sampling period in system time units
#define FS 1000              // DAS sampling
#define RUNLENGTH (10000)     // display results and quit when FilterWork==RUNLENGTH
Sema4_t LCDFree;  // SDC and LCD sharing
uint32_t FilterWork;


uint32_t MaxJitter3;  
#define JITTERSIZE3 512
uint32_t const JitterSize3=JITTERSIZE3;
uint32_t JitterHistogram3[JITTERSIZE3]={0,};
void Jitter3_Init(void){
  for(int i=0;i<JitterSize3;i++){
    JitterHistogram3[i] = 0;
  }
  MaxJitter3 = 0;
}

uint32_t FilterOutputDAS1;
uint32_t DataLostDAS1;
uint32_t DistanceDAS1;
uint32_t CountDAS1;
uint32_t IndexDAS1;
#define BUFSIZEDAS1 30
uint32_t DataBufDAS1[BUFSIZEDAS1]; // distance in mm
int32_t TheSignalDAS1,TheNoiseDAS1,SNRDAS1,sumDAS1,sumsqDAS1;
int32_t x1DAS[16],ReX1DAS[16],ImX1DAS[16]; 
//******** DAS *************** 
// background thread, calculates 60Hz notch filter
// runs 1000 times/sec
// samples PA24 Center ADC0_3, calculates Distance
// inputs:  none
// outputs: none
void DAS1(void){ 
  uint32_t input;
  uint32_t d2;  
  static uint32_t LastTime;      // time at previous ADC sample, 12.5 ns
  uint32_t thisTime;             // time at current ADC sample, 12.5 ns
  uint32_t jitter; 
  TogglePA8();                   // toggle PA8
  ADC_InDual(ADC0, &input, &d2);
  TogglePA8();                   // toggle PA8
  thisTime = OS_Time();          // current time, 12.5 ns
  FilterOutputDAS1 = Filter(input);
  DistanceDAS1 = IRDistance_Convert(FilterOutputDAS1,0); // in mm
  CountDAS1++;
  DataBufDAS1[IndexDAS1] = DistanceDAS1;
  sumDAS1 = sumDAS1 + DistanceDAS1;
  IndexDAS1++;
  if (IndexDAS1 >= BUFSIZEDAS1) {
    IndexDAS1 = 0;
    TheSignalDAS1 = sumDAS1/BUFSIZEDAS1;
    sumsqDAS1 = 0;
    for (int i = 0; i < BUFSIZEDAS1; i++) {
      int32_t v;
      v = 100 * (DataBufDAS1[i] - TheSignalDAS1);
      sumsqDAS1 = sumsqDAS1+v*v;
    }
    TheNoiseDAS1 = sqrt2(sumsqDAS1/BUFSIZEDAS1);
    if(TheNoiseDAS1 != 0){
      SNRDAS1 = (100*TheSignalDAS1)/TheNoiseDAS1;
    }else{
      SNRDAS1 = 0;
    }
    sumDAS1 = 0;
  }

  if(Running){    // finite time run
    FilterWork++;        // calculation finished
    if(FilterWork>2){    // ignore timing of first interrupt
      uint32_t diff = OS_TimeDifference(LastTime,thisTime);
      if(diff>PERIOD){
        jitter = (diff-PERIOD);  // in 12.5 ns
      }else{
        jitter = (PERIOD-diff);  // in 12.5 ns
      }
      if(jitter > MaxJitter3){
        MaxJitter3 = jitter; // in 12.5 ns
        // jitter = 0;
      }       // jitter should be 0    
      if(jitter >= JitterSize3){
        jitter = JitterSize3 -1;
      }
      JitterHistogram3[jitter]++; 
    }
    ChecksWork = Checks;
    LastTime = thisTime;
  }
  
  if(OS_Fifo_Put_Specific(DistanceDAS1, &ir1_fifo) == 0){ // send to consumer
      DataLostDAS1++;
  } 
  TogglePA8();    // toggle PA8
}

uint32_t FilterOutputDAS2;
uint32_t DataLostDAS2;
uint32_t DistanceDAS2;
uint32_t CountDAS2;
uint32_t IndexDAS2;
#define BUFSIZEDAS2 30
uint32_t DataBufDAS2[BUFSIZEDAS2]; // distance in mm
int32_t TheSignalDAS2,TheNoiseDAS2,SNRDAS2,sumDAS2,sumsqDAS2;
int32_t x1DAS[16],ReX1DAS[16],ImX1DAS[16]; 
//******** DAS *************** 
// background thread, calculates 60Hz notch filter
// runs 1000 times/sec
// samples PA24 Center ADC0_3, calculates Distance
// inputs:  none
// outputs: none
void DAS2(void){ 
  uint32_t input;  
  uint32_t d1;
  static uint32_t LastTime;      // time at previous ADC sample, 12.5 ns
  uint32_t thisTime;             // time at current ADC sample, 12.5 ns
  uint32_t jitter; 
  TogglePA8();                   // toggle PA8
  ADC_InDual(ADC0, &d1, &input);
  TogglePA8();                   // toggle PA8
  thisTime = OS_Time();          // current time, 12.5 ns
  FilterOutputDAS2 = Filter(input);
  DistanceDAS2 = IRDistance_Convert(FilterOutputDAS2,0); // in mm
  CountDAS2++;
  DataBufDAS2[IndexDAS2] = DistanceDAS2;
  sumDAS2 = sumDAS2 + DistanceDAS2;
  IndexDAS2++;
  if (IndexDAS2 >= BUFSIZEDAS2) {
    IndexDAS2 = 0;
    TheSignalDAS2 = sumDAS2/BUFSIZEDAS2;
    sumsqDAS2 = 0;
    for (int i = 0; i < BUFSIZEDAS2; i++) {
      int32_t v;
      v = 100 * (DataBufDAS2[i] - TheSignalDAS2);
      sumsqDAS2 = sumsqDAS2+v*v;
    }
    TheNoiseDAS2 = sqrt2(sumsqDAS2/BUFSIZEDAS2);
    if(TheNoiseDAS2 != 0){
      SNRDAS2 = (100*TheSignalDAS2)/TheNoiseDAS2;
    }else{
      SNRDAS2 = 0;
    }
    sumDAS2 = 0;
  }
  
  if(OS_Fifo_Put_Specific(DistanceDAS2, &ir2_fifo) == 0){ // send to consumer
      DataLostDAS2++;
  } 
  TogglePA8();    // toggle PA8
}
//--------------end of Task 1-----------------------------

//------------------Task 2--------------------------------
// background thread executes with PA28 button
// PA28 negative logic switch 
// one foreground task created with each button push
// foreground tread outputs a message and dies
uint32_t DataLost;     // data sent by Producer, but not received by Consumer

// ***********PA28Push*************
int ArmCrash=1;
void HandleCrash(void){
  TogglePA9();
  TogglePA9();
  uint32_t myId = OS_Id(); 
  ST7735_Message(1,0,"myID        =",myId); 
  ST7735_Message(1,1,"*Crash*,  t= ",OS_MsTime());
  ArmCrash=1;
  TogglePA9();
  OS_Kill();
} 
void PA28Push(void){ // real time task
  if(ArmCrash){
    ArmCrash = 0; // debounce
    NumCreated += OS_AddThread(&HandleCrash,128,1);  // test robot crash
  }
} 

//------------------Task 3--------------------------------
// hardware-triggered TFLuna distance sampling at 100Hz
// Producer runs as part of UART2 ISR
// Producer uses fifo to transmit 100 distance samples/sec to Consumer
// every 64 samples, Consumer calculates FFT
// every 2.5ms*64 = 160 ms (6.25 Hz), consumer sends data to Display via mailbox
// Display thread updates LCD with measurement

uint32_t DataLost1;        // data sent by Producer, but not received by Consumer
uint32_t Distance1;       // mm
uint32_t Count1;
uint32_t Index1;
#define BUFSIZE1 30
uint32_t DataBuf1[BUFSIZE1]; // distance in mm
int32_t TheSignal1,TheNoise1,SNR1,sum1,sumsq1;
int32_t x1[16],ReX1[16],ImX1[16];           // input and output arrays for FFT

//******** Producer1 *************** 
// The Producer in this lab will be called from the UART2 ISR
// The TFLuna1 samples distance at about 100 Hz
// sends data to the consumer, runs periodically at 100Hz
void Producer1(uint32_t data){ 
  if(Running){           // finite time run
    TogglePA16();        // toggle PA16
    // Distance1 = Median5_Specific((int32_t) data, &tfluna1_median_data);
    Distance1 = data;
    Count1++;
    DataBuf1[Index1] = Distance1;
    sum1 = sum1+Distance1;
    Index1++;        // calculation finished
    if(Index1 >= BUFSIZE1){
      Index1 = 0;
      TheSignal1 = sum1/BUFSIZE1;       // units 1
      sumsq1 = 0;
      for(int i=0; i<BUFSIZE1; i++){int32_t v;
        v = 100*(DataBuf1[i]-TheSignal1);
        sumsq1 = sumsq1+v*v;
      }
      TheNoise1 = sqrt2(sumsq1/BUFSIZE1);   // units 0.01
      SNR1 = (100*TheSignal1)/TheNoise1;  // units 1
      sum1 = 0;   
    }
    TogglePA16();        // toggle PA16
    if(OS_Fifo_Put_Specific(Distance1, &tfluna1_fifo) == 0){ // send to consumer
      DataLost++;
    } 
    TogglePA16();        // toggle PA16
  } 
}

uint32_t DataLost2;        // data sent by Producer, but not received by Consumer
uint32_t Distance2;       // mm
uint32_t Count2;
uint32_t Index2;
#define BUFSIZE2 50
uint32_t DataBuf2[BUFSIZE2]; // distance in mm
int32_t TheSignal2,TheNoise2,SNR2,sum2,sumsq2;
int32_t x2[16],ReX2[16],ImX2[16];           // input and output arrays for FFT

//******** Producer2 *************** 
// The Producer in this lab will be called from the UART2 ISR
// The TFLuna2 samples distance at about 100 Hz
// sends data to the consumer, runs periodically at 100Hz
void Producer2(uint32_t data){ 
  if(Running){           // finite time run
    TogglePA16();        // toggle PA16
    // Distance2 = Median5_Specific((int32_t) data, &tfluna2_median_data);
    Distance2 = data;
    Count2++;
    DataBuf2[Index2] = Distance2;
    sum2 = sum2+Distance2;
    Index2++;        // calculation finished
    if(Index2 >= BUFSIZE2){
      Index2 = 0;
      TheSignal2 = sum2/BUFSIZE2;       // units 1
      sumsq2 = 0;
      for(int i=0; i<BUFSIZE2; i++){int32_t v;
        v = 100*(DataBuf2[i]-TheSignal2);
        sumsq2 = sumsq2+v*v;
      }
      TheNoise2 = sqrt2(sumsq2/BUFSIZE2);   // units 0.01
      SNR2 = (100*TheSignal2)/TheNoise2;  // units 1
      sum2 = 0;   
    }
    TogglePA16();        // toggle PA16
    if(OS_Fifo_Put_Specific(Distance2, &tfluna2_fifo) == 0){ // send to consumer
      DataLost++;
    } 
    TogglePA16();        // toggle PA16
  } 
}

uint32_t DataLost3;        // data sent by Producer, but not received by Consumer
uint32_t Distance3;       // mm
uint32_t Count3;
uint32_t Index3;
#define BUFSIZE3 50
uint32_t DataBuf3[BUFSIZE3]; // distance in mm
int32_t TheSignal3,TheNoise3,SNR3,sum3,sumsq3;
int32_t x3[16],ReX3[16],ImX3[16];           // input and output arrays for FFT

//******** Producer 3*************** 
// The Producer in this lab will be called from the UART2 ISR
// The TFLuna3 samples distance at about 100 Hz
// sends data to the consumer, runs periodically at 100Hz
void Producer3(uint32_t data){ 
  if(Running){           // finite time run
    TogglePA16();        // toggle PA16
    // Distance3 = Median5_Specific((int32_t) data, &tfluna3_median_data);
    Distance3 = data;
    Count3++;
    DataBuf3[Index3] = Distance3;
    sum3 = sum3+Distance3;
    Index3++;        // calculation finished
    if(Index3 >= BUFSIZE3){
      Index3 = 0;
      TheSignal3 = sum3/BUFSIZE3;       // units 1
      sumsq3 = 0;
      for(int i=0; i<BUFSIZE3; i++){int32_t v;
        v = 100*(DataBuf3[i]-TheSignal3);
        sumsq3 = sumsq3+v*v;
      }
      TheNoise3 = sqrt2(sumsq3/BUFSIZE3);   // units 0.01
      SNR3 = (100*TheSignal3)/TheNoise3;  // units 1
      sum3 = 0;   
    }
    TogglePA16();        // toggle PA16
    if(OS_Fifo_Put_Specific(Distance3, &tfluna3_fifo) == 0){ // send to consumer
      DataLost++;
    } 
    TogglePA16();        // toggle PA16
  } 
}

void Display(void); 

// Describe the error with text on the LCD and then stall. 
// If you are getting disk errors, rerun Testmain1 Testmain2 Testmain3
void diskError(char *errtype, int32_t code){
  OS_bSignal(&LCDFree);
  ST7735_DrawString(0, 1, "Err: ", ST7735_RED);
  ST7735_DrawString(5, 1, errtype, ST7735_RED);
  ST7735_DrawString(0, 2, "Code:     ", ST7735_RED);
  ST7735_SetCursor(6, 2);
  ST7735_SetTextColor(ST7735_RED);
  ST7735_OutUDec(code);
  Running = 0;
  OS_Kill();
}
void StartFileDump(char *pt){
  OS_bWait(&LCDFree);
  eFile_Create(pt); // ignore error if file already exists
  if(eFile_WOpen(pt))  diskError("eFile_WOpen",0);
  if(eFile_WriteString("time(s)\tdist(mm)\tdist(mm)\n\r"))  diskError("eFile_WriteString",0);
  OS_bSignal(&LCDFree);
}
void EndFileDump(){
  OS_bWait(&LCDFree);
  if(eFile_WClose())            diskError("eFile_WClose",0);
  OS_bSignal(&LCDFree);
}

void FileDump(uint32_t data, uint32_t data2){
  SetPB4();
  OS_bWait(&LCDFree);
  eFile_WriteUFix2(OS_MsTime()/10); eFile_Write('\t');
  eFile_WriteUDec(data); eFile_Write('\t');
  eFile_WriteUDec(data2); eFile_WriteString("\n\r");
  OS_bSignal(&LCDFree);
  ClrPB4();
}
//******** Robot *************** 
// foreground Consumer thread, accepts data from producer
// inputs:  none
// outputs: none
char FileName[8]="robot0";

// Global buffer for mailbox communication (persistent, valid memory)
static tfluna_mail_t tfluna_mail_buffer;

void Robot(void){   
  uint32_t LastHeaderPrint;
  DataLost = 0;       // new run with no lost data 
  FilterWork = 0;
  Running = 1;
  Jitter3_Init();

  OS_ClearMsTime();    
  OS_Fifo_Init(256);
  OS_Fifo_Init_Specific(32, &tfluna1_fifo);
  OS_Fifo_Init_Specific(32, &tfluna2_fifo);
  OS_Fifo_Init_Specific(32, &tfluna3_fifo);
  OS_Fifo_Init_Specific(32, &ir1_fifo);
  OS_Fifo_Init_Specific(32, &ir2_fifo);
  
  NumCreated += OS_AddThread(&Display,128,0); 
  UART_OutString("Robot running...");
  StartFileDump(FileName);
  UART_OutString("\r\nTime(ms) |   IR1 |   IR2 |   TF1 |   TF2 |   TF3 |  SNR1 |  SNR2 | SNRDAS");
  UART_OutString("\r\n---------+-------+-------+-------+-------+-------+-------+-------+-------");
  LastHeaderPrint = OS_MsTime();

  // while(FilterWork < RUNLENGTH) { 
  while (true) {

    uint32_t data1;      // in mm, from TFLuna
    uint32_t sum1=0;
    for(int t = 0; t < 16; t++){   // collect 16 TFLuna samples
      data1 = OS_Fifo_Get_Specific(&tfluna1_fifo);    // get from producer, mm
      x1[t] = data1;
      sum1 += data1;             // average
    }
    
    uint32_t data2;      // in mm, from TFLuna
    uint32_t sum2=0;
    for(int t = 0; t < 16; t++){   // collect 16 TFLuna samples
      data2 = OS_Fifo_Get_Specific(&tfluna2_fifo);    // get from producer, mm
      x2[t] = data2;
      sum2 += data2;             // average
    }
    
    // uint32_t data3;      // in mm, from TFLuna
    // uint32_t sum3=0;
    // for(int t = 0; t < 16; t++){   // collect 16 TFLuna samples
    //   data3 = OS_Fifo_Get_Specific(&tfluna3_fifo);    // get from producer, mm
    //   x3[t] = data3;
    //   sum3 += data3;             // average
    // }

    uint32_t dataDAS1;
    uint32_t sumDAS1=0;
    for (int t = 0; t < 16; t++) { // collect 16 IR samples 
      dataDAS1 = OS_Fifo_Get_Specific(&ir1_fifo);
      x1DAS[t] = dataDAS1;
      sumDAS1 += dataDAS1;
    }

    uint32_t dataDAS2;
    uint32_t sumDAS2=0;
    for (int t = 0; t < 16; t++) { // collect 16 IR samples 
      dataDAS2 = OS_Fifo_Get_Specific(&ir2_fifo);
      x1DAS[t] = dataDAS2;
      sumDAS2 += dataDAS2;
    }

    Distance1 = sum1>>4;  // in mm
    Distance2 = sum2>>4;  // in mm
    Distance3 = sum3>>4;  // in mm
    DistanceDAS1 = sumDAS1 >>4;
    DistanceDAS2 = sumDAS2 >>4;

    // double angle_to_wall = acos((double)Distance1 / Distance2) * 180.0 / 3.14159265359;

    if((OS_MsTime() - LastHeaderPrint) >= 5000){
      UART_OutString("\r\nTime(ms)|  IR1  |  IR2   |   TF1  |   TF2  |   TF3  |  SNR1  |  SNR2 | SNRDAS1 | SNRDAS2 | Angle to Wall");
      UART_OutString("\r\n--------+-------+--------+--------+--------+--------+--------+-------+---------+---------+--------------+");
      LastHeaderPrint = OS_MsTime();
    }

    UART_OutString("\r\n");
    UART_OutUDec5(OS_MsTime()); UART_OutString(" | ");
    UART_OutUDec5(DistanceDAS1); UART_OutString(" | ");
    UART_OutUDec5(DistanceDAS2); UART_OutString(" | ");
    UART_OutUDec5(Distance1); UART_OutString(" | ");
    UART_OutUDec5(Distance2); UART_OutString(" | ");
    UART_OutUDec5(Distance3); UART_OutString(" | ");
    UART_OutUDec5((uint32_t)SNR1); UART_OutString(" | ");
    UART_OutUDec5((uint32_t)SNR2); UART_OutString(" | ");
    UART_OutUDec5((uint32_t)SNRDAS1);
    UART_OutUDec5((uint32_t)SNRDAS2);
    // UART_OutUDec5((uint32_t)angle_to_wall);

    int can_result = CAN_Send(0, sizeof(Distance1), &Distance1);
    
    tfluna_mail_buffer.Distance1 = Distance1;
    tfluna_mail_buffer.Distance2 = Distance2;
    tfluna_mail_buffer.Distance3 = Distance3;
    // FileDump(Distance,Distance2);
    OS_MailBox_Send(1); // called every 10ms*16 = 160ms
  }
  EndFileDump();
  UART_OutString("done.\n\r>");
  FileName[5] = (FileName[5]+1)&0xF7; // 0 to 7
  Running = 0;             // robot no longer running
  OS_Kill();
}
 //************S2Push*************
// Called when S2 Button pushed, fall of PB21
// Adds another Robot foreground task
// background threads execute once and return
void S2Push(void){
  if(Running==0){
    Running = 1;  // prevents you from starting two test threads
    NumCreated += OS_AddThread(&Robot,128,1);  // test eDisk
  }
}
//--------------end of Task 2-----------------------------
 
//******** Display *************** 
// foreground thread, accepts data from consumer
// displays results on the LCD
// inputs:  none                            
// outputs: none
void Display(void){ 
  uint32_t Distance1;
  uint32_t Distance2;
  uint32_t Distance3;
  uint32_t myId = OS_Id();
  ST7735_Message(0,1,"myId = ",myId);   // top half used for Display
  ST7735_Message(0,2,"Run length = ",(RUNLENGTH)/FS);   // top half used for Display
  while(Running) { 
    TogglePB1();
    int val = OS_MailBox_Recv();  // will block if mail isn't available.
    Distance1 = tfluna_mail_buffer.Distance1;
    Distance2 = tfluna_mail_buffer.Distance2;
    Distance3 = tfluna_mail_buffer.Distance3;
    TogglePB1();
    ST7735_Message(0,3,"Time(ms) =",OS_MsTime());  
    ST7735_Message(0,4,"work  =",FilterWork);
      
    ST7735_Message(0,5,"D1(mm) =",Distance1);
    ST7735_Message(0,6,"D2(mm) =",Distance2);
    ST7735_Message(0,7,"D3(mm) =",Distance3);
    
    TogglePB1();        // toggle PB1
 } 
  OS_Kill();  // done
} 

//--------------end of Task 3-----------------------------

//------------------Task 4--------------------------------
uint32_t Check(uint32_t start, uint32_t end){
  uint32_t sum=0;
  uint32_t *pt; pt = (uint32_t *)start;
  while((uint32_t)pt < end){
    sum += *pt++;
  }
  return sum;
}
//******** Virus Detector *************** 
uint32_t Checksum;             // sum of data stored in ROM
uint32_t ChecksumOriginal;     // sum of data stored in ROM
uint32_t ChecksumErrors;
void VirusDetector(void){ 
  Checks = ChecksumErrors = 0;
  ChecksumOriginal = Check(0,0x20000);
  while(1) { 
    TogglePB20();        // toggle PB20
    Checksum = Check(0,0x20000);
    Checks++;
    if(Checksum !=  ChecksumOriginal){
      ChecksumErrors++; 
    }    
  }
}

//--------------end of Task 4-----------------------------

//------------------Task 5--------------------------------
//******** Interpreter *************** 
void Interpreter(void);    // just a prototype, link to your interpreter
void Lab4(void){}
void DFT(void){}

//--------------end of Task 5-----------------------------


//*******************final user main DEMONTRATE THIS TO TA**********
int realmain(void){     // realmain
  OS_Init();        // initialize, disable interrupts

  Logic_Init();
  DataLost = 0;     // lost data between producer and consumer
  FilterWork = 0;
  
  // initialize communication channels
  OS_MailBox_Init();
  OS_Fifo_Init(256);
  OS_InitSemaphore(&LCDFree, 1);

  // hardware init
  ADC_InitDual(ADC0, 3, 7, ADCVREF_VDDA);

  // attach background tasks
  OS_AddS2Task(&S2Push,1);      // fall of PB21
  OS_AddPA28Task(&PA28Push,1);  // fall of PA28
  OS_AddPeriodicThread(&DAS1,PERIOD/80000,0); // 1 kHz real time sampling of ADC0_3
  OS_AddPeriodicThread(&DAS2,PERIOD/80000,0); // 1 kHz real time sampling of ADC0_7
  OS_AddPeriodicThread(&disk_timerproc,1,0);   // time out routines for disk
  
  // create initial foreground threads
  NumCreated = 0;
  NumCreated += OS_AddThread(&Interpreter,128,1); 
  NumCreated += OS_AddThread(&VirusDetector,128,2);
 
  LPF_Init7(500,7);
  TFLuna1_Init(&Producer1);
  TFLuna1_Format_Standard_mm(); // format in mm
  TFLuna1_Frame_Rate();         // 100 samples/sec
  TFLuna1_SaveSettings();  // save format and rate
  TFLuna1_System_Reset();  // start measurements
  
  TFLuna2_Init(&Producer2);
  TFLuna2_Format_Standard_mm(); // format in mm
  TFLuna2_Frame_Rate();         // 100 samples/sec
  TFLuna2_SaveSettings();  // save format and rate
  TFLuna2_System_Reset();  // start measurements

  TFLuna3_Init(&Producer3);
  TFLuna3_Format_Standard_mm(); // format in mm
  TFLuna3_Frame_Rate();         // 100 samples/sec
  TFLuna3_SaveSettings();  // save format and rate
  TFLuna3_System_Reset();  // start measurements

  if(eFile_Init())              diskError("eFile_Init",0); 
  if(eFile_Format())            diskError("eFile_Format",0); 
  if(eFile_Mount())             diskError("eFile_Mount",0);
  OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
  return 0;            // this never executes
}
//+++++++++++++++++++++++++DEBUGGING CODE++++++++++++++++++++++++
// ONCE YOUR RTOS WORKS YOU CAN COMMENT OUT THE REMAINING CODE
// 

//*******************Trampoline for selecting which main to execute**********
int main(void) {      // main 
  __disable_irq();
  Clock_Init80MHz(0); // no clock out to pin
  LaunchPad_Init();   // LaunchPad_Init must be called once and before other I/O initializations
  realmain();
}