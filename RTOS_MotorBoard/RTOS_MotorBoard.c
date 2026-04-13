/* RTOS_MotorBoard.c
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
#include "../RTOS_Labs_common/TFLuna2.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/esp8266.h"
#include "../RTOS_Labs_common/WifiSettings.h"
#include "../RTOS_Labs_common/PWMA0.h"
#include "../RTOS_Labs_common/PWMA1.h"
#include "../RTOS_Labs_common/PWMG6.h"
#include "../inc/SSD1306.h"
#include "../inc/I2C.h"
#include "../RTOS_Labs_common/CAN.h"
#include <stdio.h>
#include <string.h>

typedef enum direction {
  weak_left = 0,
  strong_left,  //1
  straight, //2 
  strong_right, //3
  weak_right  // 4
} direction_t;

typedef enum speed {
  stop = 0,
  slow, // 1
  medium, // 2
  fast  // 3
} speed_t;

typedef struct command {
  direction_t direction;
  speed_t speed;
} command_t;


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
#define PERIOD TIME_1MS      // DAS 1kHz sampling period in system time units;
Sema4_t LCDFree;  // SDC and LCD sharing


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


 //************S2Push*************
// Called when S2 Button pushed, fall of PB21
// Adds another Robot foreground task
// background threads execute once and return
void S2Push(void){
  if(Running==0){
    Running = 1;  // prevents you from starting two test threads
    // NumCreated += OS_AddThread(&Robot,128,1);  // test eDisk
  }
}
//--------------end of Task 2-----------------------------
 
//******** Display *************** 
// foreground thread, accepts data from consumer
// displays results on the LCD
// inputs:  none                            
// outputs: none
void Display(void){ 
        // toggle PB1
 
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

//--------------end of Task 5-----------------------------

//--------------For Wifi-----------------------------

// 1) Robot name (up to 10 chars)
char Name[11] = "Robot17";

// 2) Bump sensors: "00", "01", "10", or "11"
char Bump[3] = "00";

// 3) Steering pulse width (2000–4000)
char Steering[6] = "3000";

// 4) Right motor pulse width (4–9996)
char Right[6] = "1500";

// 5) Left motor pulse width (4–9996)
char Left[6] = "1500";

// 6) SysTickElapsed (ASCII)
char SysTickStr[16] = "0";

// 7) AddThreadElapsed (ASCII)
char AddThreadStr[16] = "0";

// 8) MaxJitter (ASCII)
char JitterStr[16] = "0";

// Buffer for the full GET request
char LOGDATA[256] =
  "GET /php/json/write.php?"
  "name=Robot17&bump=00&steering=3000&right=1500&left=1500&"
  "systick=0&addthread=0&jitter=0 "
  "HTTP/1.0\r\nHOST: embedded.ece.utexas.edu\r\n\r\n";

  char Status[16];
  uint32_t StartTime,EndTime,ElapsedTime;
  extern uint32_t SysTickElapsed;
  extern uint32_t AddThreadElapsed;


bool StoppedFlag = false;

void StartRobot(void) {
  StoppedFlag = false;
}
void StopRobot(void) {
    StoppedFlag = true;
}

void WiFiThread(void){
  char *s;
 
  if(!ESP8266_Connect(true)){
    SSD1306_DrawString(0,16,"No Wifi network",SSD1306_WHITE);
    SSD1306_OutBuffer();
    while(1);
  }

 
  SSD1306_DrawString(0,16,"Wifi connected   ",SSD1306_WHITE);
  SSD1306_OutBuffer();

  for(;;){
      snprintf(SysTickStr, sizeof(SysTickStr), "%u", SysTickElapsed);
      snprintf(AddThreadStr, sizeof(AddThreadStr), "%u", AddThreadElapsed);

    // Build GET request directly from global ASCII strings
    snprintf(LOGDATA, sizeof(LOGDATA),
      "GET /php/json/write.php?"
      "name=%s&bump=%s&steering=%s&right=%s&left=%s&"
      "systick=%s&addthread=%s&jitter=%s "
      "HTTP/1.0\r\nHOST: embedded.ece.utexas.edu\r\n\r\n",
      Name, Bump, Steering, Right, Left,
      SysTickStr, AddThreadStr, JitterStr);

    // Send to server
    if(ESP8266_MakeTCPConnection("embedded.ece.utexas.edu", 80, 0, false)){
      
       ESP8266_StartReceiveSearch("status=");
       int StartTime = OS_MsTime();

      if(ESP8266_Send(LOGDATA)){

        uint32_t timeout = 10000000;
        do{
          s = ESP8266_GetReceiveBuffer();
          timeout--;
        }while((s == 0) && timeout);

        // End timing
        EndTime = OS_MsTime();
        if(StartTime>EndTime){
        ElapsedTime = StartTime - EndTime;
        }
        else{
          ElapsedTime = EndTime - StartTime;
        }

        if(s){
          int i = 0;
          while(((*s) != ' ') && (i < 15)){
            Status[i] = *s;
            s++; 
            i++;
          }
          Status[i] = 0;
         
          if (strstr(Status, "green")) {
            StartRobot();
          }
          else if (strstr(Status, "red")) {
            StopRobot();
          }
          

          if(ElapsedTime > 999999){
          ElapsedTime = 999999;  // prevent overflow on screen
          }
          SSD1306_DrawString(0,44,Status,SSD1306_WHITE); 
          SSD1306_DrawString(0,56,"Time(ms)      ",SSD1306_WHITE); 
          SSD1306_DrawUDec(56,56,ElapsedTime,SSD1306_WHITE);   
          SSD1306_OutBuffer();
        }
      
    }
    ESP8266_CloseTCPConnection();
    }

    OS_Sleep(1000);   // log every 1000 ms
  }
}

int mainWifi(void){
// 1. Initialize OS and hardware
  OS_Init();
  Logic_Init();
  SSD1306_Init(2);
 

  SSD1306_SetCursor(0,0);
  SSD1306_OutString("ECE445M wifi test\n");  

  // 2. Initialize ESP8266
  if(!ESP8266_Init(true, false)){
    while(1);   // no WiFi adapter
  }


  // 3. Add WiFi thread (globals-only version)
  NumCreated += OS_AddThread(&WiFiThread, 128, 2);
  NumCreated += OS_AddThread(&VirusDetector,128,2);


  // 5. Launch OS
  OS_Launch(TIME_2MS);
  return 0;
}

//*******************final user main DEMONTRATE THIS TO TA**********
int realmain(void){     // realmain
  //   OS_Init();
  // Logic_Init();

  // SSD1306_Init(SSD1306_SWITCHCAPVCC);

  // ServoDuty = SERVOINIT;

  // PWMG6_Init(PWMUSEBUSCLK,39,
  //             SERVOPERIOD,
  //             SERVOINIT);

  // // CAN setup
  // OS_CAN_Init(1);
  // OS_InitSemaphore(&CAN_Available,0);

  // // Motor PWM setup
  // PWMA0_Init(PWMUSEBUSCLK,39,
  //             MOTORPERIOD,
  //             2500,7500);
  // PWMA0_Break();

  // PWMA1_Init(PWMUSEBUSCLK,39,
  //             MOTORPERIOD,
  //             2500,7500);
  // PWMA1_Break();

  // // 2. Initialize ESP8266
  // if(!ESP8266_Init(true, false)){
  //   while(1);   // no WiFi adapter
  // }

  // NumCreated = 0;

  // // CAN thread
  // NumCreated += OS_AddThread(&MotorCANThread, 128, 1);
  
  // // 3. Add WiFi thread (globals-only version)
  // NumCreated += OS_AddThread(&WiFiThread, 128, 1);
  // NumCreated += OS_AddThread(&VirusDetector, 128, 2);

  // OS_Launch(TIME_2MS);

  return 0;            // this never executes
}

// ********************* Motors  ******************//

uint32_t Duty;
#define MOTORPERIOD 10000 // 200Hz
#define MOTORCHANGE 1000  // 10%
#define MOTORMIN 1000     // 10%
#define MOTORMAX 9000     // 90%
uint32_t ServoDuty; // 2000,2250,2500,2750,3000,3250,3500,3750,4000
#define SERVOMIN 2000      // 1ms
#define SERVOMAX 4000      // 2ms
#define SERVOINIT 3000     // 1.5ms
#define SERVOPERIOD 40000  // 20ms
#define SERVOCHANGE 250    // 0.125ms



void SSD1306_Display(void){
  SSD1306_SetCursor(0,3);
  if(Duty == 0){
    SSD1306_OutString("Motor break         ");
    SSD1306_SetCursor(0,4);
    SSD1306_OutString("                    ");
  }else{
    SSD1306_OutString("Motor Period=       ");
    SSD1306_SetCursor(13,3);SSD1306_OutUDec(MOTORPERIOD);
    SSD1306_SetCursor(0,4);
    SSD1306_OutString("Motor Duty =        ");
    SSD1306_SetCursor(13,4);SSD1306_OutUDec(Duty);
  }
  SSD1306_SetCursor(0,5);
  SSD1306_OutString("Servo Period=       ");
  SSD1306_SetCursor(13,5);SSD1306_OutUDec(SERVOPERIOD);
  SSD1306_SetCursor(0,6);
  SSD1306_OutString("Servo Duty =        ");
  SSD1306_SetCursor(13,6);SSD1306_OutUDec(ServoDuty);
}

uint32_t Array[20] = {2750, 2800, 2850, 2900, 2950, 3000, 3050, 3100, 3150, 3200, 
3250, 3200, 3150, 3100, 3050, 3000, 2950, 2900, 2850, 2800};
uint8_t count = 0;

void RunForward(void){   uint32_t sw2,lasts2;
  uint32_t sw1,lasts1;
  PWMA0_Init(PWMUSEBUSCLK,39,MOTORPERIOD,2500,7500); // 200Hz
  PWMA0_Break(); // high, high, break mode
  PWMA1_Init(PWMUSEBUSCLK,39,MOTORPERIOD,2500,7500); // 200Hz
  PWMA1_Break(); // high, high, break mode
  Duty = MOTORMIN;
  lasts2 = (~(GPIOB->DIN31_0)) & S2;
  while(1){   
    while (StoppedFlag) {
      PWMG6_SetDuty(ServoDuty);
      PWMA0_Forward(0);
      PWMA1_Backward(0); 
    }
    Clock_Delay(1000000); // debounce switch
    sw2 = (~(GPIOB->DIN31_0)) & S2;
    sw1 = GPIOA->DIN31_0 & S1;
    if(sw2 && (lasts2==0)){ // touch s2
      Duty = Duty+MOTORCHANGE;
      if(Duty > MOTORMAX){
        Duty = MOTORMIN;
      }
      SSD1306_Display();
      PWMA0_Backward(Duty);
      PWMA1_Forward(Duty);
    }
     // touch s1
    ServoDuty = Array[count];
    count = (count + 1) % 20;
    SSD1306_Display();
    PWMG6_SetDuty(ServoDuty);
    
    lasts2 = sw2;
    lasts1 = sw1;
  }
}

// scope on PB1 PB4, spins with left motor backward
// PB4 is high , PB1 is Duty (time low)
// scope on PB8 PB9, spins with right motor backward
// PB8 is high , PB9 is Duty (time low)
// PB6 1ms to 2ms pulse high, 20ms period
void RunBackward(void){   uint32_t sw2,lasts2;
uint32_t sw1,lasts1;
  PWMA0_Init(PWMUSEBUSCLK,39,MOTORPERIOD,2500,7500); // 200Hz
  PWMA0_Break(); // high, high, break mode
  PWMA1_Init(PWMUSEBUSCLK,39,MOTORPERIOD,2500,7500); // 200Hz
  PWMA1_Break(); // high, high, break mode
  Duty = MOTORMIN;
  lasts2 = (~(GPIOB->DIN31_0)) & S2;
  while(1){   
    while (StoppedFlag) {
      PWMG6_SetDuty(ServoDuty);
      PWMA0_Forward(0);
      PWMA1_Backward(0);   
    } 
    Clock_Delay(1000000); // debounce switch
    sw1 = GPIOA->DIN31_0 & S1;
    sw2 = (~(GPIOB->DIN31_0)) & S2;
    if(sw2 && (lasts2==0)){ // touch s2
      Duty = Duty+MOTORCHANGE;
      if(Duty > MOTORMAX){
        Duty = MOTORMIN;
      }
      SSD1306_Display();
      PWMA0_Forward(Duty);
      PWMA1_Backward(Duty);   
   }
    ServoDuty = Array[count];
    count = (count + 1) % 20;
    SSD1306_Display();
    PWMG6_SetDuty(ServoDuty);
    
    lasts2 = sw2;
    lasts1 = sw1;
  }

}
int mainMotor(void) {
  //uint32_t  sw2 = (~(GPIOB->DIN31_0)) & S2;
  SSD1306_SetCursor(0,0);
  SSD1306_OutString("ECE445M motor test\n");  
  ServoDuty = SERVOINIT;    // 1.5ms
// period is 20ms
// change is 0.125ms
  PWMG6_Init(PWMUSEBUSCLK,39,SERVOPERIOD,SERVOINIT); // 50Hz, 1.5ms

  SSD1306_Display();
  SSD1306_SetCursor(0,1);
  NumCreated += OS_AddPA28Task(&StopRobot, 0);
  NumCreated += OS_AddPA27Task(&StopRobot, 0);
  NumCreated += OS_AddThread(&RunForward, 256, 1);

  // 5. Launch OS
  OS_Launch(TIME_2MS);

  return 0;
}

//*******************CAN TEST**********
uint32_t CANData;
uint32_t id;
uint32_t failures;
void ReceiveCAN(void)
{
  while (true)
  {
    OS_Wait(&CAN_Available);
    // while (!CAN_Get_ID(&CANData, &id)) { 
    while (!CAN_Get(&CANData)) { 
      OS_Sleep(1000);
    }
    TogglePB4();
    SSD1306_SetCursor(0,0);
    SSD1306_OutString("CAN Received");
    SSD1306_SetCursor(0,1);
    SSD1306_OutUDec(CANData);
  }
}

int TestmainCAN(void)
{
  OS_Init();  
  Logic_Init();
  OS_CAN_Init(1);
  OS_InitSemaphore(&CAN_Available, 0);
  SSD1306_Init(SSD1306_SWITCHCAPVCC);

  NumCreated = 0;
  NumCreated += OS_AddThread(&ReceiveCAN, 128, 1);
  NumCreated += OS_AddThread(&VirusDetector, 128, 2);

  OS_Launch(TIME_2MS);
  return 0;
}

//*************** Can and Motor Test ****************/


#define BASE_SPEED   3000
#define WEAK_DELTA   800
#define STRONG_DELTA 1600
 #define SERVO_CENTER 2900
#define SERVO_LEFT   2450
#define SERVO_RIGHT  3450


void ExecuteCommand(command_t cmd, int id){

  uint32_t leftDuty;
  uint32_t rightDuty;
  uint32_t ServoDuty;

  switch(cmd.direction){

    case weak_left:
      leftDuty  = BASE_SPEED - WEAK_DELTA;
      rightDuty = BASE_SPEED + WEAK_DELTA;
      // PWMA0_Backward(leftDuty);
      // PWMA1_Forward(rightDuty);
        ServoDuty = SERVO_CENTER;
      PWMA0_Backward(rightDuty);
      PWMA1_Forward(leftDuty);
       PWMG6_SetDuty(ServoDuty);
      break;

    case strong_left:
      leftDuty  = BASE_SPEED - STRONG_DELTA;
      rightDuty = BASE_SPEED + STRONG_DELTA;
      ServoDuty = SERVO_LEFT;
      // PWMA0_Backward(leftDuty);
      // PWMA1_Forward(rightDuty);
       PWMA0_Backward(rightDuty);
      PWMA1_Forward(leftDuty);
      PWMG6_SetDuty(ServoDuty);
      break;

    case straight:
      leftDuty  = BASE_SPEED;
      rightDuty = BASE_SPEED;
      // PWMA0_Backward(leftDuty);
      // PWMA1_Forward(rightDuty);
      ServoDuty = SERVO_CENTER;
      PWMA0_Backward(rightDuty);
      PWMA1_Forward(leftDuty);
      PWMG6_SetDuty(ServoDuty);

      switch (cmd.speed)
      {
        case stop:
          PWMA0_Backward(0);
          PWMA1_Forward(0);
          break;
        case slow:
          PWMA0_Backward(BASE_SPEED - WEAK_DELTA);
          PWMA1_Forward(BASE_SPEED - WEAK_DELTA);
          break;
        case medium:
          PWMA0_Backward(BASE_SPEED);
          PWMA1_Forward(BASE_SPEED);
          break;
        case fast:
          PWMA0_Backward(BASE_SPEED + WEAK_DELTA);
          PWMA1_Forward(BASE_SPEED + WEAK_DELTA);
          break;
      }
      break;

    case strong_right:
      leftDuty  = BASE_SPEED + STRONG_DELTA;
      rightDuty = BASE_SPEED - STRONG_DELTA;
      // PWMA0_Backward(leftDuty);
      // PWMA1_Forward(rightDuty);
      ServoDuty = SERVO_RIGHT;
      PWMA0_Backward(rightDuty);
      PWMA1_Forward(leftDuty);
      PWMG6_SetDuty(ServoDuty);
      break;

    case weak_right:
      leftDuty  = BASE_SPEED + WEAK_DELTA;
      rightDuty = BASE_SPEED - WEAK_DELTA;
      // PWMA0_Backward(leftDuty);
      // PWMA1_Forward(rightDuty);
       ServoDuty = SERVO_CENTER;
       PWMA0_Backward(rightDuty);
      PWMA1_Forward(leftDuty);
       PWMG6_SetDuty(ServoDuty);
      break;

    default:
     leftDuty  = BASE_SPEED;
      rightDuty = BASE_SPEED;
      // PWMA0_Backward(leftDuty);
      // PWMA1_Forward(rightDuty);
      PWMA0_Backward(rightDuty);
      PWMA1_Forward(leftDuty);
      // PWMA0_Forward(0);
      // PWMA1_Backward(0);
      break;
  }

 SSD1306_SetCursor(0,2);
SSD1306_OutString("CMD: ");

switch(cmd.direction){

  case weak_left:
    SSD1306_OutString("weak_left   ");
    break;

  case strong_left:
    SSD1306_OutString("strong_left ");
    break;

  case straight:
    SSD1306_OutString("straight    ");
    break;

  case strong_right:
    SSD1306_OutString("strong_right");
    break;

  case weak_right:
    SSD1306_OutString("weak_right  ");
    break;

  default:
    SSD1306_OutString("straight    ");
    break;
}

}


void MotorCANThread(void)
{
  command_t cmd = { straight, stop };
  uint32_t id;
  while(1){
    if (!StoppedFlag)
    {
      OS_Wait(&CAN_Available);
      
      while(!CAN_Get_ID(&CANData, &id)){
        OS_Sleep(1);
      }
      if (id == 0)
      {
        cmd.direction = (direction_t)CANData;
      }
      else if (id == 1)
      {
        cmd.speed = (speed_t)CANData;
      }      

      ExecuteCommand(cmd, id);
    }

  }
}

int mainCAN_Motor(void){
  OS_Init();
  Logic_Init();

  SSD1306_Init(SSD1306_SWITCHCAPVCC);

  ServoDuty = SERVOINIT;

  PWMG6_Init(PWMUSEBUSCLK,39,
              SERVOPERIOD,
              SERVOINIT);

  // CAN setup
  OS_CAN_Init(1);
  OS_InitSemaphore(&CAN_Available,0);

  // Motor PWM setup
  PWMA0_Init(PWMUSEBUSCLK,39,
              MOTORPERIOD,
              2500,7500);
  PWMA0_Break();

  PWMA1_Init(PWMUSEBUSCLK,39,
              MOTORPERIOD,
              2500,7500);
  PWMA1_Break();

  NumCreated = 0;

  NumCreated +=
      OS_AddThread(&MotorCANThread,
                   128,1);

  NumCreated +=
      OS_AddThread(&WiFiThread,
                   128,1);

  NumCreated +=
      OS_AddThread(&VirusDetector,
                   128,2);

  OS_Launch(TIME_2MS);

  return 0;



  
}




//*******************Trampoline for selecting which main to execute**********
int main(void) { 			// main 
  __disable_irq();
  Clock_Init80MHz(0); // no clock out to pin
  LaunchPad_Init();   // LaunchPad_Init must be called once and before other I/O initializations
  
  
  //mainWifi();
  //mainMotor();
  mainCAN_Motor();
}