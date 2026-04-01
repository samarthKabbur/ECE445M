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
#include <stdio.h>
#include <math.h>
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
uint32_t FilterOutput,Distance;
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
//******** DAS *************** 
// background thread, calculates 60Hz notch filter
// runs 1000 times/sec
// samples PA24 Center ADC0_3, calculates Distance
// inputs:  none
// outputs: none
void DAS(void){ 
  uint32_t input;  
  static uint32_t LastTime;      // time at previous ADC sample, 12.5 ns
  uint32_t thisTime;             // time at current ADC sample, 12.5 ns
  uint32_t jitter;               // time between measured and expected, 12.5 ns
  TogglePA8();                   // toggle PA8
  input = ADC0_In();             // channel 3 set when calling ADC0_Init
  TogglePA8();                   // toggle PA8
  thisTime = OS_Time();          // current time, 12.5 ns
  FilterOutput = Filter(input);
  Distance = IRDistance_Convert(FilterOutput,0); // in mm
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
  DataLost = 0;       // new run with no lost data 
  FilterWork = 0;
  Running = 1;
  Jitter3_Init();

  OS_ClearMsTime();    
  OS_Fifo_Init(256);
  OS_Fifo_Init_Specific(64, &tfluna1_fifo);
  OS_Fifo_Init_Specific(64, &tfluna2_fifo);
  OS_Fifo_Init_Specific(64, &tfluna3_fifo);
  NumCreated += OS_AddThread(&Display,128,0); 
  UART_OutString("Robot running...");
  StartFileDump(FileName);

  // while(FilterWork < RUNLENGTH) { 
  while (true) {

    uint32_t data1;      // in mm, from TFLuna
    uint32_t sum1=0;
    for(int t = 0; t < 16; t++){   // collect 16 TFLuna samples
      data1 = OS_Fifo_Get_Specific(&tfluna1_fifo);    // get from producer, mm
      x1[t] = data1;
      sum1 += data1;             // average
    }
    
    // uint32_t data2;      // in mm, from TFLuna
    // uint32_t sum2=0;
    // for(int t = 0; t < 16; t++){   // collect 16 TFLuna samples
    //   data2 = OS_Fifo_Get_Specific(&tfluna2_fifo);    // get from producer, mm
    //   x2[t] = data2;
    //   sum2 += data2;             // average
    // }
    
    uint32_t data3;      // in mm, from TFLuna
    uint32_t sum3=0;
    for(int t = 0; t < 16; t++){   // collect 16 TFLuna samples
      data3 = OS_Fifo_Get_Specific(&tfluna3_fifo);    // get from producer, mm
      x3[t] = data3;
      sum3 += data3;             // average
    }

    Distance1 = sum1>>4;  // in mm
    Distance2 = sum2>>4;  // in mm
    Distance3 = sum3>>4;  // in mm
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
      
    ST7735_Message(0, 8, "SNR =", SNR3);

    
    int angle = (uint32_t)acos(Distance1 / (Distance2)) * 60;
    ST7735_Message(0, 9, "Angle =", angle);
    TogglePB1();        // toggle PB1
 } 
  OS_Kill();  // done
} 

//--------------end of Task 3-----------------------------

//------------------Task 4--------------------------------
// foreground thread that runs without waiting or sleeping
// it executes a virus detector 
uint32_t Check(uint32_t start, uint32_t end){
  uint32_t sum=0;
  uint32_t *pt; pt = (uint32_t *)start;
  while((uint32_t)pt < end){
    sum += *pt++;
  }
  return sum;
}
//******** Virus Detector *************** 
// foreground thread, performs a checksum of all ROM
// never blocks, never sleeps, never dies
// inputs:  none
// outputs: none
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
// UART0 background ISR performs serial input/output
// Two software fifos are used to pass I/O data to foreground
// The interpreter runs as a foreground thread
// The UART0 driver should call OS_Wait(&RxDataAvailable) when foreground tries to receive
// The UART0 ISR should call OS_Signal(&RxDataAvailable) when it receives data from Rx
// Similarly, the transmit channel waits on a semaphore in the foreground
// and the UART0 ISR signals this semaphore (TxRoomLeft) when getting data from fifo

//******** Interpreter *************** 
// Modify your intepreter from Lab 1, adding commands to help debug 
// Interpreter is a foreground thread, accepts input from serial port, outputs to serial port
// inputs:  none
// outputs: none
void Interpreter(void);    // just a prototype, link to your interpreter
// add the following commands, leave other commands, if they make sense
// 1) print performance measures 
//    time-jitter, number of data points lost, number of calculations performed
//    i.e., NumCreated, MaxJitter, DataLost, FilterWork, Checks
      
// 2) print debugging parameters 
//    i.e., Checks, ChecksumErrors

// Call these from your interpreter
void Lab4(void){int i;
  UART_OutString("\r\nLab 4 performance data");
  UART_OutString("\r\nFilterWork     = "); UART_OutUDec(FilterWork);
  UART_OutString("\r\nNumCreated     = "); UART_OutUDec(NumCreated);
  UART_OutString("\r\nChecksWork     = "); UART_OutUDec(ChecksWork);
  UART_OutString("\r\nDataLost       = "); UART_OutUDec(DataLost); 
  UART_OutString("\r\nReal-time sampling jitter (cyc)");
  UART_OutString("\r\nTime,  Frequencies");
  for(i=0; i<JitterSize3; i++){
    if(JitterHistogram3[i]){ // skip blanks
      UART_OutString("\r\n "); 
      UART_OutUDec5(i);
      UART_OutUDec5(JitterHistogram3[i]);
    }
  }
  UART_OutString("\r\nMaxJitter3(cyc) = "); UART_OutUDec(MaxJitter3); 
}

void DFT(void){ int i;  int32_t real,imag,mag;
  UART_OutString("\r\nLab 2/3 DFT data");
  UART_OutString("\r\nInput,  Output Real, Output Imaginary, Magnitude");
  for(i=0; i<8; i++){
    real = ReX1[i];
    imag = ImX1[i];    
    mag = sqrt2(real*real+imag*imag);
    UART_OutString("\r\n"); UART_OutUDec(x1[i]); UART_OutChar(' '); UART_OutSDec(real); UART_OutChar(' '); UART_OutSDec(imag);
    UART_OutChar(' '); UART_OutSDec(mag);
  }
}

//--------------end of Task 5-----------------------------


//*******************final user main DEMONTRATE THIS TO TA**********
int realmain(void){     // realmain
  OS_Init();        // initialize, disable interrupts

  Logic_Init();
  DataLost = 0;     // lost data between producer and consumer
  FilterWork = 0;
  Jitter3_Init();
  // initialize communication channels
  OS_MailBox_Init();
  OS_Fifo_Init(256);    // ***note*** 4 is not big enough*****

  // hardware init
  ADC0_Init(3,ADCVREF_VDDA);  // PA24 Center ADC0_3, sampling in DAS() 
  OS_InitSemaphore(&LCDFree, 1);
  // attach background tasks
  OS_AddS2Task(&S2Push,1);      // fall of PB21
  OS_AddPA28Task(&PA28Push,1);  // fall of PA28
  OS_AddPeriodicThread(&DAS,PERIOD/80000,0); // 1 kHz real time sampling of ADC0_3
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

uint32_t M=1;
uint32_t Random32(void){
  M = 1664525*M+1013904223;
  return M;
}
// 0 to 31
uint32_t Random5(void){
  return (Random32()>>27);
}
// 0 to 127
uint32_t Random7(void){
  return (Random32()>>25);
}
// 0 to 255
uint8_t Random8(void){
  return (Random32()>>24);
}

//*****************Test project 1*************************
// This test should run. If ST7735R works, but this fails:
// -  there may be bad solder joints on Sensor board
// -  the MSPM0 might have bad pins (PB0 or PB8)
// -  SDC not seated correctly or damaged
// Write and read test of random access disk blocks
// Warning: this overwrites whatever is on the disk
unsigned char buffer[512];  // don't put on stack
#define MAXBLOCKS 100
void TestDisk(void){  DSTATUS result;  uint32_t block;  int i; uint8_t n;
  // simple test of eDisk
  ST7735_DrawString(0, 1, "eDisk test      ", ST7735_WHITE);
  UART_OutString("\n\rECE445M, Lab 4 eDisk test\n\rTestmain1\n\r");
  result = eDisk_Init(0);  // initialize disk
  if(result) diskError("eDisk_Init",result);
  UART_OutString("Writing blocks\n\r");
  M = 1;    // seed
  for(block = 0; block < MAXBLOCKS; block++){
    for(i=0;i<512;i++){
      buffer[i] = Random8();        
    }
    SetPA8();     // PA8 high for 100 block writes
    if(eDisk_WriteBlock(buffer,block))diskError("eDisk_WriteBlock",block); // save to disk
    //TogglePA16();
    ClrPA8();     
  }  
  UART_OutString("Reading blocks\n\r");
  M = 1;  // reseed, start over to get the same sequence
  for(block = 0; block < MAXBLOCKS; block++){
    SetPA9();     // PA9 high for one block read
    if(eDisk_ReadBlock(buffer,block))diskError("eDisk_ReadBlock",block); // read from disk
    ClrPA9();
    for(i=0;i<512;i++){
      n = Random8(); // same pseudo random sequence
      if(buffer[i] != (0xFF&n)){
        UART_OutString("Read data not correct, block="); UART_OutUDec(block); 
        UART_OutString(", i="); UART_OutUDec(i); 
        UART_OutString(", expected "); UART_OutUDec(0xFF&n); 
        UART_OutString(", read "); UART_OutUDec(buffer[i]);       
        UART_OutString("\n\r");
        OS_Kill();
      }      
    }
  }  
  UART_OutString("Successful test of 100 blocks\n\r");
  ST7735_DrawString(0, 1, "eDisk successful", ST7735_YELLOW);
  Running = 0; // allow launch again
  OS_Kill();
}

void StartTestDisk(void){
  if(Running==0){
    Running = 1;  // prevents you from starting two test threads
    NumCreated += OS_AddThread(&TestDisk,128,1);  // test eDisk
  }
}

int Testmain1(void){   // Testmain1
  OS_Init();           // initialize, disable interrupts
 // OS_AddDevices(1,1,0);  // attach printf to UART0, allow ST7735, not eFile
  Logic_Init();
  Running = 1; 

  // attach background tasks
  OS_AddPeriodicThread(&disk_timerproc,1,0);   // time out routines for disk
  OS_AddS2Task(&StartTestDisk,1);
  OS_AddPA28Task(&StartTestDisk,1);
    
  // create initial foreground threads
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&TestDisk,128,1);  
  NumCreated += OS_AddThread(&VirusDetector,128,3); 
 
  OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
  return 0;            // this never executes
}


//*******************Measurement of context switch time**********
// Run this to measure the time it takes to perform a task switch
// UART0 not needed 
// SYSTICK interrupts, period established by OS_Launch
// first timer not needed
// second timer not needed
// S1 not needed, 
// S2 not needed
// logic analyzer on PB22 for systick interrupt (in your OS)
//                on PA8 to measure context switch time
void ThreadCS(void){    // only thread running
  while(1){
    TogglePA8();        // toggle PA8
  }
}
int TestmainCS(void){       // TestmainCS
  Logic_Init();
  OS_Init();           // initialize, disable interrupts
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&ThreadCS,128,0); 
  OS_Launch(TIME_1MS/10); // 100us, doesn't return, interrupts enabled in here
  return 0;               // this never executes
}


//*****************Test project 2*************************
// Filesystem test. 
// Warning: this reformats the disk, all existing data will be lost
void PrintDirectory(void){ char *name; unsigned long size; 
  unsigned int num;
  unsigned long total;
  num = 0;
  total = 0;
  UART_OutString("\n\r");
  if(eFile_DOpen(""))           diskError("eFile_DOpen",0);
  while(!eFile_DirNext(&name, &size)){
    UART_OutString("Filename = "); UART_OutString(name); UART_OutString("  ");
    UART_OutString("Size (bytes)= "); UART_OutUDec(size); UART_OutString("\n\r");
    total = total+size;
    num++;    
  }
  UART_OutString("Number of Files = "); UART_OutUDec(num); UART_OutString("\n\r");
  UART_OutString("Number of Bytes = "); UART_OutUDec(total); UART_OutString("\n\r");
  if(eFile_DClose())            diskError("eFile_DClose",0);
}
void TestFile(void){   int i; char data; int status;
  UART_OutString("\n\rECE445M Lab 4 eFile test 2\n\r");
  ST7735_DrawString(0, 1, "eFile test 2    ", ST7735_WHITE);
  // simple test of eFile
  if(eFile_Init())              diskError("eFile_Init",0); 
  if(eFile_Format())            diskError("eFile_Format",0); 
  if(eFile_Mount())             diskError("eFile_Mount",0);
  PrintDirectory();
  if(eFile_Create("file1"))     diskError("eFile_Create",0);
  if(eFile_WOpen("file1"))      diskError("eFile_WOpen",0);
  for(i=5; i<=15; i++){
    eFile_WriteString("Testmain2\tabcdefghijklmnopqrstuvwxyz\t");
    eFile_WriteUFix2(OS_MsTime()/10); eFile_Write('\t');
    eFile_WriteUDec(i); eFile_Write('\t');
    eFile_WriteSFix2(i-10); eFile_Write('\t');
    eFile_WriteSDec(i-10); eFile_WriteString("\n\r");
    OS_Sleep(10);
  }
  if(eFile_WClose())            diskError("eFile_WClose",0);
  PrintDirectory();

  if(eFile_ROpen("file1"))      diskError("eFile_ROpen",0);
  do{
    status = eFile_ReadNext(&data);
    if(status == 0) UART_OutChar(data);
  }while(status==0);
  if(eFile_RClose())  diskError("eFile_RClose",0);
  if(eFile_Delete("file1"))     diskError("eFile_Delete",0);
  PrintDirectory();
  if(eFile_Unmount())           diskError("eFile_Unmount",0);
  UART_OutString("Successful test\n\r");
  ST7735_DrawString(0, 1, "eFile successful", ST7735_YELLOW);
  Running=0; // launch again
  OS_Kill();
}

void StartFileTest(void){
  if(Running==0){
    Running = 1;  // prevents you from starting two test threads
    NumCreated += OS_AddThread(&TestFile,128,1);  // test eFile
  }
}

int Testmain2(void){   // Testmain2 
  OS_Init();           // initialize, disable interrupts
  Logic_Init();
  Running = 1; 

  // attach background tasks
  OS_AddPeriodicThread(&disk_timerproc,1,0);   // time out routines for disk
  OS_AddS2Task(&StartFileTest,1);
  OS_AddPA28Task(&StartFileTest,1);
    
  // create initial foreground threads
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&TestFile,128,1);  
  NumCreated += OS_AddThread(&VirusDetector,128,3); 
 
  OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
  return 0;            // this never executes
}

void PrintFile3(char *pt){int status; char data; 
  OS_bWait(&LCDFree);  
  eFile_ROpen(pt);
  do{
    status = eFile_ReadNext(&data);
    if(status == 0) UART_OutChar(data);
  }while(status==0);
  eFile_RClose();
  OS_bSignal(&LCDFree);
}
void Dump3(uint32_t run,int32_t data){
  SetPA8();
  OS_bWait(&LCDFree);
  eFile_WriteString("Testmain3\tabcdefghijklmnopqrstuvwxyz\t");
  eFile_WriteUFix2(OS_MsTime()/10); eFile_Write('\t');
  eFile_WriteUDec(run); eFile_Write('\t');
  eFile_WriteSFix2(data); eFile_Write('\t');
  eFile_WriteSDec(data); eFile_WriteString("\n\r");
  OS_bSignal(&LCDFree);
  ClrPA8();
}
//*****************Test project 3*************************
// Filesystem stream test. 
// Warning: this reformats the disk, all existing data will be lost
uint32_t Run3=0;
void TestFile3(void){   int i; char data; 
  UART_OutString("\n\rECE445M Lab 4 eFile test 3\n\r");
  ST7735_Message(0,1,"eFile test 3", Run3);
  // test of eFile

  PrintDirectory();
  OS_bWait(&LCDFree);
  eFile_Create(FileName);
  eFile_WOpen(FileName);
  OS_bSignal(&LCDFree);
  for(i=-5; i<=5; i++){
    Dump3(Run3,i);
    Run3++;
    OS_Sleep(10);
  }
  OS_bWait(&LCDFree);
  eFile_WClose();
  OS_bSignal(&LCDFree);
  PrintDirectory();
  PrintFile3(FileName);
  UART_OutString("Successful test 3, Run3="); UART_OutUDec(Run3);
  UART_OutString("\n\r");
  ST7735_Message(0,2,"Run3 =",Run3);  

  Running = 0; // allowed to launch again
  FileName[5] = (FileName[5]+1)&0xF7; // 0 to 7
 
  OS_Kill();
}
void Chaos3(void){
  ST7735_Message(1,0,"Chaos",3); 
  while(1){
    for(int l=1; l<5; l++){
      ST7735_Message(1,l,"n =",Random8());  
    }
    OS_Sleep(100);
  }
}
void StartFileTest3(void){
  if(Running==0){
    Running = 1;  // prevents you from starting two test threads
    NumCreated += OS_AddThread(&TestFile3,128,1);  // test eFile
  }
}

int Testmain3(void){   // Testmain3 
  OS_Init();           // initialize, disable interrupts
  Logic_Init();
  Running = 1; 
  OS_InitSemaphore(&LCDFree, 1);

  // attach background tasks
  OS_AddPeriodicThread(&disk_timerproc,1,0);   // time out routines for disk
  OS_AddS2Task(&StartFileTest3,1);
  OS_AddPA28Task(&StartFileTest3,1);
    
  // create initial foreground threads
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&TestFile3,128,1);  
  NumCreated += OS_AddThread(&Chaos3,128,1);  
  NumCreated += OS_AddThread(&VirusDetector,128,3); 

  if(eFile_Init())              diskError("eFile_Init",0); 
  int error = eFile_Format();
  if(error == 2)            diskError("eFile_Format",2); 
  if(error == 1)            diskError("eFile_Format",0);
  if(eFile_Mount())             diskError("eFile_Mount",0);

  OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
  return 0;            // this never executes
}


//*******************Trampoline for selecting which main to execute**********
int main(void) {      // main 
  __disable_irq();
  Clock_Init80MHz(0); // no clock out to pin
  LaunchPad_Init();   // LaunchPad_Init must be called once and before other I/O initializations
  realmain();
}