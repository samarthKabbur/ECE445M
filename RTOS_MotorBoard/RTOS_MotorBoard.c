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

bool StoppedFlag = false;
bool PreviousFlag = false;

void StopRobot(void) {
    StoppedFlag = true;
}
void StartRobot(void){
 
  StoppedFlag = false;
}




// **** OS must run disk_timerproc();  at 1000Hz, every 1ms *****
uint32_t Running;           // true while robot is running
uint32_t NumCreated;   // number of foreground threads created
extern uint32_t BumpPress;

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
int Crash=0;
void HandleCrash(void){
  Crash = 1;
  
} 

// void PA28Push(void){ // real time task
//   if(ArmCrash){
//     ArmCrash = 0; // debounce
//     NumCreated += OS_AddThread(&HandleCrash,128,1);  // test robot crash
//   }
// } 



 //************S2Push*************
// Called when S2 Button pushed, fall of PB21
// Adds another Robot foreground task
// background threads execute once and return
void S2Push(void){
  if(StoppedFlag==1){
    StartRobot();
    PreviousFlag = false;
  }
}


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
char Name[11] = "George";

// 2) Bump sensors: "00", "01", "10", or "11"
//char Bump[3] = "00";

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
char JitterStr[16] = "373";

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
uint32_t LeftMotor;
uint32_t RightMotor;
uint32_t SteeringNum;
uint32_t Time;

void WiFiThread(void){
  char *s;
  char Bump[3] = "00";
 
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
      snprintf(Steering, sizeof(Steering), "%u", SteeringNum);
      snprintf(Right, sizeof(Right), "%u",RightMotor );
      snprintf(Left, sizeof(Left), "%u",LeftMotor );
      Bump[0] = (BumpPress & 0x02) ? '1' : '0';
      Bump[1] = (BumpPress & 0x01) ? '1' : '0';
      

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
      

      if(ESP8266_Send(LOGDATA)){

        uint32_t timeout = 10000000;
        do{
          s = ESP8266_GetReceiveBuffer();
          timeout--;
        }while((s == 0) && timeout);

       

        if(s){
          int i = 0;
          while(((*s) != ' ') && (i < 15)){
            Status[i] = *s;
            s++; 
            i++;
          }
          Status[i] = 0;

        
          char *value = strchr(Status, '=');

          if (value) {
            value++;   // move past '='
            int greenFlag = (strcmp(value, "Green") == 0) || ((strcmp(value, "green") == 0));
            if (( greenFlag && PreviousFlag == false) || (greenFlag && PreviousFlag == false)) {
              StartRobot();
              Time = OS_MsTime();
              PreviousFlag = true;
            }
           
           if ((strcmp(value, "Red") == 0 || strcmp(value, "red") == 0 ) && PreviousFlag == true) {
               StopRobot();
               PreviousFlag = false;
            }
          }
           
         
            ElapsedTime = (OS_MsTime() - Time)/1000;
          if(ElapsedTime > 999999){
          ElapsedTime = 999999;  // prevent overflow on screen
          }
          SSD1306_DrawString(0,44,Status,SSD1306_WHITE); 
          SSD1306_DrawString(0,56,"Time(s)      ",SSD1306_WHITE); 
          SSD1306_DrawUDec(56,56,ElapsedTime,SSD1306_WHITE);   
          SSD1306_OutBuffer();
        }
      
    }
    ESP8266_CloseTCPConnection();
    }

    OS_Sleep(1000);   // log every second
  
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
  NumCreated += OS_AddThread(&WiFiThread, 512, 1);

  NumCreated += OS_AddThread(&VirusDetector,128,2);


  // 5. Launch OS
  OS_Launch(TIME_2MS);
  return 0;
}




//*******************final user main DEMONTRATE THIS TO TA**********


// ********************* Motors  ******************//


uint32_t Duty;
#define MOTORPERIOD 10000 // 200Hz
#define MOTORCHANGE 1000  // 10%
#define MOTORMIN 1000     // 10%
#define MOTORMAX 9000     // 90%
uint32_t ServoDuty; // 2000,2250,2500,2750,3000,3250,3500,3750,4000
#define SERVOMIN 2000      // 1ms
#define SERVOMAX 4000      // 2ms
#define SERVOINIT 2900     // 1.5ms
#define SERVOPERIOD 40000  // 20ms
#define SERVOCHANGE 250    // 0.125ms



//*******************CAN TEST**********
uint32_t CANData;
uint32_t id;
uint32_t failures;


//*************** Can and Motor Test ****************/

//max duty is 10000
//steering from 2000 to 4000
#define BASE_SPEED   3000
#define WEAK_DELTA   800
#define STRONG_DELTA 1600
#define SERVO_WEAK_LEFT 2650
#define SERVO_WEAK_RIGHT 3250
 #define SERVO_CENTER 2900
#define SERVO_STRONG_LEFT   2450
#define SERVO_STRONG_RIGHT  3450
//  A0 is right
//    A1 is left
int IncreaseSpeed = 1500;

void ExecuteCommand(command_t cmd){
  //PWMG6_SetDuty(cmd.steering);
     if(Crash>0 && Crash < 5){
    cmd.speed = 6500; //slow edit this 
    PWMG6_SetDuty(SERVO_CENTER);
    PWMA0_Forward(cmd.speed); // want this to go backwards
    PWMA1_Backward(cmd.speed);
    RightMotor = cmd.speed;
    LeftMotor = cmd.speed;
    SteeringNum = SERVO_CENTER;
    
    
     
     Crash++;
      }
      
      
  else{

    BumpPress = 0;
  PWMG6_SetDuty(cmd.steering);
  if(cmd.speed <30){
    PWMA0_Backward(0);
    PWMA1_Forward(0);
  }
  
  int minus = cmd.speed-cmd.differentials + IncreaseSpeed;
  int plus = cmd.speed+cmd.differentials + IncreaseSpeed;
  if(minus<0){
    minus = 0;
    
  }
  if(plus<0){
    plus = 0;
  }
  if(minus>9999){
    minus = 9999;
  }
  if(plus>9999){
    plus = 9999;
  }
  
  PWMA0_Backward(minus);
  PWMA1_Forward(plus);
  RightMotor = minus;
  LeftMotor = plus;
  SteeringNum = cmd.steering;
 

   }
   
   

}

command_t cmd;
void MotorCANThread(void)
{
  
  uint32_t id;
  while(1){
    if(StoppedFlag){
    
      PWMA0_Backward(0);
      PWMA1_Forward(0);
      
    }
    else{
    OS_Wait(&CAN_Available);
    

    while(!CAN_GetCommand(&cmd)){
      OS_Sleep(1);
    }
    
    
    
    ExecuteCommand(cmd);
    

  }
  }
}

int mainCAN_Motor(void){
  OS_Init();
  Logic_Init();

  SSD1306_Init(SSD1306_SWITCHCAPVCC);
  StoppedFlag  = true;
  
  
  if(!ESP8266_Init(true, false)){
    while(1);   // no WiFi adapter
  }

  ServoDuty = SERVOINIT;

  PWMG6_Init(PWMUSEBUSCLK,39,
              SERVOPERIOD,
           SERVOINIT);

    // 2. Initialize ESP8266
  if(!ESP8266_Init(true, false)){
    while(1);   // no WiFi adapter
  }

  // CAN setup
  OS_CAN_Init(1);
  OS_InitSemaphore(&CAN_Available,0);



  NumCreated = 0;
   //OS_AddPA28Task(&PA28Push,1);  // fall of PA28
  NumCreated += OS_AddPA28Task(&HandleCrash, 0);
  NumCreated += OS_AddPA27Task(&HandleCrash, 0);
  NumCreated +=
      OS_AddThread(&MotorCANThread,
                   128,1);

  NumCreated +=
      OS_AddThread(&WiFiThread,
                   512,1);

  NumCreated +=
      OS_AddThread(&VirusDetector,
                   128,2);

  OS_AddS2Task(&S2Push,1);      // fall of PB21

  OS_Launch(TIME_2MS);

  return 0;

  
}




//*******************Trampoline for selecting which main to execute**********
int main(void) { 			// main 
  __disable_irq();
  Clock_Init80MHz(0); // no clock out to pin
  LaunchPad_Init();   // LaunchPad_Init must be called once and before other I/O initializations
  mainCAN_Motor();
}