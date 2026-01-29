/* MotorTestmain.c
 * Jonathan Valvano
 * November 24, 2022
 * Remove 3.3V J101 jumper to run RTOS sensor board or motor board
 * A two-pin female header is required on the LaunchPad TP10(XDS_VCC) and TP9(!RSTN)


PINCM  Pin   Mode    Timer     Usage
17     PB4   Mode4   TIMA1_C0  Left Motor,  IN2, PWM low for forward, GPIO high for reverse 
13     PB1   Mode4   TIMA1_C1  Left Motor,  IN1, GPIO high for forward, PWM low for reverse  

25     PB8   Mode4   TIMA0_C0  Right Motor, IN2, PWM low for forward, GPIO high for reverse   
26     PB9   Mode4   TIMA0_C1  Right Motor, IN1, GPIO high for forward, PWM low for reverse 

23     PB6   Mode7   TIMG6_C0  Steering Servo PWM high 20ms period,   
 */


#include <ti/devices/msp/msp.h>
#include "../RTOS_Labs_common/LaunchPad.h"
#include "../inc/Clock.h"
#include "../RTOS_Labs_common/PWMA0.h"
#include "../RTOS_Labs_common/PWMA1.h"
#include "../RTOS_Labs_common/PWMG6.h"
//  PA0 is red LED1,   index 0 in IOMUX PINCM table
// PB22 is BLUE LED2,  index 49 in IOMUX PINCM table
// PB26 is RED LED2,   index 56 in IOMUX PINCM table
// PB27 is GREEN LED2, index 57 in IOMUX PINCM table
// PA18 is S2 positive logic switch,  index 39 in IOMUX PINCM table
// PB21 is S3 negative logic switch,  index 48 in IOMUX PINCM table
uint32_t Duty,Period,Change;

// scope on PB1 PB4, run without motors
// PB4 is Duty, PB1 is Period-Duty
int main0(void){   uint32_t sw2,lasts2;
  Clock_Init80MHz(0);
  LaunchPad_Init();
  PWMA1_Init(PWMUSEBUSCLK,39,10000,2500,7500); // 200Hz
  PWMA1_Coast(); // low low, sleep mode
  Duty = 1000;
  Period = 10000;
  Change = 1000;
  lasts2 = (~(GPIOB->DIN31_0)) & S2;
  while(1){    
    Clock_Delay(1000000); // debounce switch
    sw2 = (~(GPIOB->DIN31_0)) & S2;
    if(sw2 && (lasts2==0)){ // touch s2
      Duty = Duty+Change;
      if(Duty >= Period){
        Duty = Change;
      }
      PWMA1_SetDuty(Duty,Period-Duty);
    }
    lasts2 = sw2;
    
  }
}
// scope on PB1 PB4, spins with left motor forward
// PB4 is Duty (time low), PB1 is high
// scope on PB8,PB9, spins with right motor forward
// PB8 is Duty (time low), PB9 is high
int main1(void){   uint32_t sw2,lasts2;
  Clock_Init80MHz(0);
  LaunchPad_Init();
  PWMA0_Init(PWMUSEBUSCLK,39,10000,2500,7500); // 200Hz
  PWMA0_Break(); // high, high, break mode
    PWMA1_Init(PWMUSEBUSCLK,39,10000,2500,7500); // 200Hz
  PWMA1_Break(); // high, high, break mode
  Duty = 1000;
  Period = 10000;
  Change = 1000;
  lasts2 = (~(GPIOB->DIN31_0)) & S2;
  while(1){    
    Clock_Delay(1000000); // debounce switch
    sw2 = (~(GPIOB->DIN31_0)) & S2;
    if(sw2 && (lasts2==0)){ // touch s2
      Duty = Duty+Change;
      if(Duty >= Period){
        Duty = Change;
      }
      PWMA0_Forward(Duty);
      PWMA1_Forward(Duty);
    }
    lasts2 = sw2;
    
  }
}

// scope on PB1 PB4, spins with left motor backward
// PB4 is high , PB1 is Duty (time low)
// scope on PB8 PB9, spins with right motor backward
// PB8 is high , PB9 is Duty (time low)
int main2(void){   uint32_t sw2,lasts2;
  Clock_Init80MHz(0);
  LaunchPad_Init();
  PWMA0_Init(PWMUSEBUSCLK,39,10000,2500,7500); // 200Hz
  PWMA0_Break(); // high, high, break mode
  PWMA1_Init(PWMUSEBUSCLK,39,10000,2500,7500); // 200Hz
  PWMA1_Break(); // high, high, break mode
  Duty = 1000;
  Period = 10000;
  Change = 1000;
  lasts2 = (~(GPIOB->DIN31_0)) & S2;
  while(1){    
    Clock_Delay(1000000); // debounce switch
    sw2 = (~(GPIOB->DIN31_0)) & S2;
    if(sw2 && (lasts2==0)){ // touch s2
      Duty = Duty+Change;
      if(Duty >= Period){
        Duty = Change;
      }
      PWMA0_Backward(Duty);
      PWMA1_Backward(Duty);
    }
    lasts2 = sw2;
    
  }
}




int main(void){   uint32_t sw2,lasts2;
  Clock_Init80MHz(0);
  LaunchPad_Init();
  PWMG6_Init(PWMUSEBUSCLK,39,40000,4000); // 50Hz, 2ms
  Duty = 1000;    // 0.5ms
  Period = 40000; // 20ms
  Change = 1000;  // 0.5ms
  lasts2 = (~(GPIOB->DIN31_0)) & S2;
  while(1){    
    Clock_Delay(1000000); // debounce switch
    sw2 = (~(GPIOB->DIN31_0)) & S2;
    if(sw2 && (lasts2==0)){ // touch s2
      Duty = Duty+Change;
      if(Duty > 5000){
        Duty = Change;
      }
      PWMG6_SetDuty(Duty);
    }
    lasts2 = sw2;
    
  }
}