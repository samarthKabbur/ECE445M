// *************os.c**************
// ECE445M Labs 1, 2, 3, 4, 5 and 6
// Starter to labs 1,2,3,4,5,6
// high level OS functions
// Students will implement these functions as part of Lab
// Runs on MSPM0
// Jonathan W. Valvano 
// January 10, 2026, valvano@mail.utexas.edu


#include <stdint.h>
#include <stdio.h>
#include <ti/devices/msp/msp.h>
#include "file.h"
#include "../inc/Clock.h"
#include "../inc/LaunchPad.h"
#include "../inc/Timer.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/RTOS_UART.h"
#include "../RTOS_Labs_common/SPI.h"
#include "../RTOS_Labs_common/ST7735_SDC.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/heap.h"

// Hardware interrupt priorities
//   Priority 0: Periodic threads 
//   Priority 1: Input/output interrupts like UART and edge triggered 
//   Priority 2: 1000 Hz periodic event to implement OS_MsTime and sleep using TimerG8
//   Priority 2: SysTick for running the scheduler
//   Priority 3: PendSV for context switch 

// *****************Timers******************
// SysTick for running the scheduler
// Use TimerG0 is used for SDC timeout
// Use TimerG7 for background periodic threads
// Use TimerG8 is interrupts at 1000Hz to implement OS_MsTime, and sleeping
// Use TimerG12 for 32-bit OS_Time, free running (no interrupts)
// Use TimerA0 for PWM outputs to motors
// Use TimerA1 for PWM outputs to motors
// Use TimerG6 for Lab 1 and then for PWM to servo steering

void OSDisableInterrupts(void);
void OSEnableInterrupts(void);
long StartCritical(void);
void EndCritical(long);
void SetInitialStack(int i);
#define  OSCRITICAL_ENTER(sr) { sr=StartCritical(); }
#define  OSCRITICAL_EXIT(sr)  { EndCritical(sr); }

volatile uint32_t TimeMs; // in ms

#define NUMTHREADS 3  // maximum number of threads
#define STACKSIZE 100 // maximum of 32-bit words on the stack (400 bytes per stack)

typedef struct tcb {
  int *sp;  // pointer to stack, valid for threads not running
  struct tcb *next; // linked-list pointer
  struct tcb *prev; // linked-list pointer, useful when many threads exist and for thread deletion
  int id;
  int sleep_st;
  int priority;
  int blocked_st;
} tcb_t;

tcb_t tcbs [NUMTHREADS];
tcb_t *RunPt; // points to the stack pointer
tcb_t *NextThreadPt;
int32_t Stacks[NUMTHREADS][STACKSIZE];  // creates 3 * 400 byte stack (uses 1.2kb of memory)

// ******** OS_ClearMsTime ************
// sets the system time to zero (solve for Lab 1), and start a periodic interrupt
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void){
  // using timer g7 for this feature
  TimeMs = 0;
  TimerG7_IntArm(1000, 80, 2);
};


// ******** OS_MsTime ************
// reads the current time in msec (solve for Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// For Labs 2 and beyond, it is ok to make the resolution to match the first call to OS_AddPeriodicThread
uint32_t OS_MsTime(void){
  // put Lab 1 solution here
  return TimeMs;
};

void StartOS(void); // implemented in osasm.s


//------------------------------------------------------------------------------
//  Systick Interrupt Handler
//  SysTick interrupt happens every 2 ms
// used for preemptive foreground thread switch
// ------------------------------------------------------------------------------
void SysTick_Handler(void) { 
  GPIOB->DOUTTGL31_0 = (1<<22);
  OS_Suspend();
} // end SysTick_Handler

void Scheduler(void) {
  RunPt = RunPt->next;  // go to at least the next thread
  while (RunPt->sleep_st) {
    RunPt = RunPt->next;  // find a thread that isn't sleeping
  }
}

uint32_t OS_LockScheduler(void){
 uint32_t old = SysTick->CTRL;
  SysTick->CTRL= 5;
  return old;
}
void OS_UnLockScheduler(uint32_t previous){
  SysTick->CTRL = previous;
}


//
//@details  Initialize operating system, disable interrupts until OS_Launch.
//Initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers.
// Interrupts not yet enabled.
 // @param  none
 // @return none
 //@brief  Initialize OS
//
void OS_Init(void){
  // put Lab 2 (and beyond) solution here
  OSDisableInterrupts();
  OS_ClearMsTime();
  //Enable Interrupts occurs at OS_Launch
}

// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4_t *semaPt, int32_t value){
  // put Lab 2 (and beyond) solution here
  // i assume that this isn't a critical section
  // because this function should only be called once
  semaPt->Value = value;
}; 


// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4_t *semaPt){
  long sr;
  // put Lab 2 (and beyond) solution here
  OSCRITICAL_ENTER(sr);
  while (semaPt->Value <= 0) {
    OSCRITICAL_EXIT(sr);
    OS_Suspend();
    OSCRITICAL_ENTER(sr);
  }
  semaPt->Value--;
  OSCRITICAL_EXIT(sr);
}; 

// ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4_t *semaPt){long sr;
  // put Lab 2 (and beyond) solution here
  OSCRITICAL_ENTER(sr);
  semaPt->Value++;
  OSCRITICAL_EXIT(sr);
}; 

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4_t *semaPt){
  // put Lab 2 (and beyond) solution here
  long sr;
  OSCRITICAL_ENTER(sr);
  while (semaPt->Value <= 0) {
    OSCRITICAL_EXIT(sr);
    OS_Suspend();
    OSCRITICAL_ENTER(sr);
  }
  semaPt->Value = 0;
  OSCRITICAL_EXIT(sr);

}; 

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4_t *semaPt){
  // put Lab 2 (and beyond) solution here
  long sr;
  OSCRITICAL_ENTER(sr);
  semaPt->Value = 1;
  OSCRITICAL_EXIT(sr);
}; 



// ******** OS_AddThread *************** 
// add a foreground thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 255 is the lowest
// Priorities are relative to other foreground threads
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
// In Lab 4, the stackSize can be 128, 256, or 512 bytes

int OS_AddProcessThread(void(*task)(void), 
   uint32_t stackSize, uint32_t priority, uint32_t pid){
	   return 0;
   }

int OS_AddThread(void(*task)(void), uint32_t stackSize, uint32_t priority){ 
  long sr;
  static int thread_idx = 0; 
  int i;
  
  if (thread_idx >= NUMTHREADS) {
      return 0; // No more threads available: fail
  }
  
  OSCRITICAL_ENTER(sr);
  
  i = thread_idx;
  thread_idx++;
  
  // init tcb
  tcbs[i].id = i; 
  tcbs[i].priority = priority;
  tcbs[i].blocked_st = 0;
  tcbs[i].sleep_st = 0;

  SetInitialStack(i);  // this func was copied from the book
  Stacks[i][STACKSIZE - 2] = (int32_t)(task); // sets the PC field on the stack to the starting address of the task

  // insert RunPt into the LL
  if (RunPt == (void*)0) {
      tcbs[i].next = &tcbs[i];
      tcbs[i].prev = &tcbs[i];
      RunPt = &tcbs[i]; 
  } else {
      tcb_t *tail = RunPt->prev;
      
      tcbs[i].next = RunPt;
      tcbs[i].prev = tail;
      
      tail->next = &tcbs[i];
      RunPt->prev = &tcbs[i];
  }

  OSCRITICAL_EXIT(sr);
  return 1;
}

void SetInitialStack(int i) {
  tcbs[i].sp = &Stacks[i][STACKSIZE - 12];  // thread stack pointer
  Stacks[i][STACKSIZE-1] = 0x01000000;  // thumb bit
  Stacks[i][STACKSIZE-3] = 0x14141414;  // R14
  Stacks[i][STACKSIZE-4] = 0x12121212;  // R12
  Stacks[i][STACKSIZE-5] = 0x03030303;  // R3
  Stacks[i][STACKSIZE-6] = 0x02020202;  // R2
  Stacks[i][STACKSIZE-7] = 0x01010101;  // R1
  Stacks[i][STACKSIZE-8] = 0x00000000; // R0
  Stacks[i][STACKSIZE-9] = 0x07070707;  // R7
  Stacks[i][STACKSIZE-10] = 0x06060606; // R6
  Stacks[i][STACKSIZE-11] = 0x05050505; // R5
  Stacks[i][STACKSIZE-12] = 0x04040404; // R4 <- thread sp (tcbs[i].sp) starts by pointing here
}

// ******** OS_AddProcess *************** 
// add a process with foregound thread to the scheduler
// Inputs: pointer to process text (code) segment, entry point at top
//         pointer to process data segment
//         number of bytes allocated for its stack
//         priority (0 is highest)
// Outputs: 1 if successful, 0 if this process can not be added
// This function will be needed for Lab 5
// In Labs 2-4, this function can be ignored
int OS_AddProcess(void *text, void *data, uint32_t stackSize, uint32_t priority){ 
  
  return 0;
}


int OS_LoadProgram(char *name, uint32_t priority){
  
  return 0;
}



// ******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
uint32_t OS_Id(void){
  // put Lab 2 (and beyond) solution here
    return 0; // replace this line with solution
}



uint32_t lcm2(uint32_t n1,uint32_t n2){
  uint32_t n;
  if(n1 > n2){
    n = n1;
  }else{
    n = n2;
  }
  while( ((n % n1) != 0) || ((n % n2) != 0) ){
    n++;
  }
  return n;
}

uint32_t lcm3(uint32_t n1,uint32_t n2,uint32_t n3){
  return lcm2(lcm2(n1,n2),n3);
}
uint32_t lcm4(uint32_t n1,uint32_t n2,uint32_t n3,uint32_t n4){
  return lcm2(lcm2(n1,n2),lcm2(n3,n4));
}
uint32_t lcm5(uint32_t n1,uint32_t n2,uint32_t n3,uint32_t n4,uint32_t n5){
  return lcm2(lcm2(n1,n2),lcm3(n3,n4,n5));
}

//******** OS_AddPeriodicThread *************** 
// Add a background periodic thread
// typically this function receives the highest priority
// Inputs: task is pointer to a void/void background function
//         period in ms
//         priority 0 is the highest, 3 is the lowest
// Priorities are relative to other background periodic threads
// Outputs: 1 if successful, 0 if this thread can not be added
// You are free to select the resolution of period
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In lab 2, this command will be called 0 or 1 times
// In lab 3, this command will be called 0 to 4 times
// In labs 3-7, there will be 0 to 4 background periodic threads, and this priority field 
//           determines the relative priority of these threads
// For Lab 3, it ok to make reasonable limits to reduce the complexity. E.g.,
//  - You can assume there are 0 to 4 background periodic threads
//  - You can assume the priorities are sequential 0,1,2,3,4
//  - You can assume a maximum thread execution time, e.g., 50us
//  - You can assume a slowest period, e.g., 50ms
//  - You can limit possible periods, e.g., 1,2,4,5,10,20,25,50ms
//  - You can assume (E0/T0)+(E1/T1)+(E2/T2)+(E3/T3) is much less than 1 

int OS_AddPeriodicThread(void(*task)(void), 
   uint32_t period, uint32_t priority){
  // put Lab 2 (and beyond) solution here
   
  return 0; // replace this line with solution
}

void TIMG7_IRQHandler(void){
  if((TIMG7->CPU_INT.IIDX) == 1){ // this will acknowledge
    TimeMs++;
  }
}  
void TIMG8_IRQHandler(void){
  if((TIMG8->CPU_INT.IIDX) == 1){ // this will acknowledge
    
 
  }
}





//----------------------------------------------------------------------------
//  Edge triggered Interrupt Handler
//  Rising edge of PA18 (S1) 
//  Falling edge of PB21 (S2)
//  Falling edge of PA27 (bump)
//  Falling edge of PA28 (bump)
//  Falling edge of PA31 (bump)
//----------------------------------------------------------------------------
void GROUP1_IRQHandler(void){
  // write this
  if(GPIOA->CPU_INT.RIS&(1<<18)){ // PA18
    GPIOA->CPU_INT.ICLR = 1<<18;
    
  }
  if(GPIOA->CPU_INT.RIS&(1<<28)){ // PA28
    GPIOA->CPU_INT.ICLR = 1<<28;
   
  }
  if(GPIOB->CPU_INT.RIS&(1<<21)){ // PB21
    GPIOB->CPU_INT.ICLR = 1<<21;
    
  }
}
// ******** OS_AddS1Task *************** 
// add a background task to run whenever the S1 (PA18) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 1 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// Because of the pin conflict with TFLuna, this command will not be called 
int OS_AddS1Task(void(*task)(void), uint32_t priority){
  // put Lab 2 (and beyond) solution here
  
  return 0; // replace this line with solution
};

// ******** OS_AddS2Task *************** 
// add a background task to run whenever the S2 (PB21) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 1 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed user task will run to completion and return
// This task can not spin block loop sleep or kill
// This task can call issue OS_Signal, it can call OS_AddThread
// This task does not have a Thread ID
// In lab 2, this function will be called 0 or 1 times
// In lab 3, this function will be called will be called 0 or 1 times
// In lab 3, there will be many background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddS2Task(void(*task)(void), uint32_t priority){
  // put Lab 2 (and beyond) solution here
  
  return 0; // replace this line with solution
}

// ******** OS_AddPA28Task *************** 
// add a background task to run whenever the bump (PA28) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 1 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In lab 3, this command will be called 0 or 1 times
// In lab 2, not implemented
// In lab 3, there will be many background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddPA28Task(void(*task)(void), uint32_t priority){
  // put Lab 3 (and beyond) solution here
 
  return 0; // replace this line with solution
};



// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
  // put Lab 2 (and beyond) solution here
  // use int TimeMS global variable to time the sleeping
  long sr;
  OSCRITICAL_ENTER(sr);
  RunPt->sleep_st = sleepTime;  // Run_pt->sleep_st will be decremented with TimG7
  OSCRITICAL_EXIT(sr);
  
} 



// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
  // put Lab 2 (and beyond) solution here
  long sr;
  OSCRITICAL_ENTER(sr);
  tcb_t *pt = RunPt;

  // case for only one thread exists
  if (RunPt == RunPt->next) {

  }

  while (pt->next != RunPt) {
    pt = pt->next;
  }
  pt->next = RunPt->next;

  OSCRITICAL_EXIT(sr);
  OS_Suspend();



  for(;;){};        // can not return (must return in Lab 5 since called from SVC_hander)
}; 



// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void){
  // put Lab 2 (and beyond) solution here
  SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
};
  



// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128,256
void OS_Fifo_Init(uint32_t size){
  // put Lab 2 (and beyond) solution here
  
 
}

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
int OS_Fifo_Put(uint32_t data){
  // put Lab 2 (and beyond) solution here
  return 0; // replace this line with solution
}

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
uint32_t OS_Fifo_Get(void){
  // put Lab 2 (and beyond) solution here
   return 0; // replace this line with solution
}

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
int32_t OS_Fifo_Size(void){
  // put Lab 2 (and beyond) solution here
  return 0; // replace this line with solution
}
// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void){
  // put Lab 2 (and beyond) solution here
  
 
}

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(uint32_t data){
  // put Lab 2 (and beyond) solution here
  
};

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
uint32_t OS_MailBox_Recv(void){
  // put Lab 2 (and beyond) solution here
   return 0; // replace this line with solution
};

// ******** OS_Time ************
// return the system time, counting up 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
uint32_t OS_Time(void){
  // put Lab 2 (and beyond) solution here
    return 0; // replace this line with solution
};

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
uint32_t OS_TimeDifference(uint32_t start, uint32_t stop){
  // put Lab 2 (and beyond) solution here
    return 0; // replace this line with solution
};



// ******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: number of 12.5ns clock cycles for each time slice
//         you may select the units of this parameter
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick
// make PendSV priority 3, and SysTick priority 2
void OS_Launch(uint32_t theTimeSlice){
  // units of theTimeSlice are in bus cycles (12.5 ns)
  // put Lab 2 (and beyond) solution here
  SysTick->CTRL = 0; // disable systick during setup
  SysTick->VAL = 0; // clear val
  SCB->SHP[1] = ((SCB->SHP[1]&(~0xC0000000)) | (3 << 30)); // set priority 3
  SysTick->LOAD = theTimeSlice - 1; // reload value
  SysTick->CTRL = 0x07; // enable, core clock, and arm interrupts
  StartOS();  // start on the first task (implemented in osasm.s)
}