// *************OS.c**************
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
//   Priority 2: 1000 Hz periodic event to implement OS_MsTime and sleep using TimerG7
//   Priority 2: SysTick for running the scheduler
//   Priority 3: PendSV for context switch 

// *****************Timers******************
// SysTick for running the scheduler
// Use TimerG0 is used for SDC timeout
// Use TimerG7 is interrupts at 1000Hz to implement OS_MsTime, and sleeping
// Use TimerG8 for background periodic threads
// Use TimerG12 for 32-bit OS_Time, free running (no interrupts)
// Use TimerA0 for PWM outputs to motors
// Use TimerA1 for PWM outputs to motors
// Use TimerG6 for Lab 1 and then for PWM to servo steering

//for debugging 
#define TogglePA8() (GPIOA->DOUTTGL31_0 = (1<<8))
#define TogglePA9() (GPIOA->DOUTTGL31_0 = (1<<9))
#define TogglePA16() (GPIOA->DOUTTGL31_0 = (1<<16))
#define TogglePB4() (GPIOB->DOUTTGL31_0 = (1<<4))
#define TogglePB1() (GPIOB->DOUTTGL31_0 = (1<<1))
#define TogglePB20() (GPIOB->DOUTTGL31_0 = (1<<20))
#define TogglePB22() (GPIOB->DOUTTGL31_0 = (1<<22))
#define TogglePB26() (GPIOB->DOUTTGL31_0 = (1<<26))


void OSDisableInterrupts(void);
void OSEnableInterrupts(void);
long StartCritical(void);
void EndCritical(long);
void SetInitialStack(int i, uint32_t stackSize);
#define  OSCRITICAL_ENTER(sr) { sr=StartCritical(); }
#define  OSCRITICAL_EXIT(sr)  { EndCritical(sr); }

volatile uint32_t TimeMs;
volatile uint32_t TimeMsG8; // in ms
volatile uint32_t TimeMsG7;
volatile uint32_t TimeUs; // in microseconds

#define MAX_PROCESSES 32 // should match whats in heap.c
#define MAXTHREADS 32  // maximum number of threads
#define STACKSIZE 128 // maximum of 32-bit words on the stack 
// (STACKSIZE * NUMTHREADS bytes per stack)

tcb_t tcbs [MAXTHREADS];
int NumThreads; //for allocated  foreground threads
tcb_t *RunPt; // points to the stack pointer
tcb_t *NextThreadPt;
int32_t Stacks[MAXTHREADS][STACKSIZE];  // creates 3 * 400 byte stack (uses 1.2kb of memory)
// Stacks will now be stored on the Heap

/* BACKGROUND PERIODIC THREADS 
- scheduled by TimerG8
*/
typedef struct periodic_task {
  void (*task)(void); // pointer to the background thread
  uint32_t period;  // reload value
  uint32_t timeLeft;  // decrement counter
  uint32_t nextTriggerTime; // absolute time for next run
  enum state Status; // active or free
  int priority;
} periodic_task_t;

#define MAX_PERIODIC_THREADS 64
int NumPeriodic; //for allocated periodic threads

periodic_task_t periodic_threads[MAX_PERIODIC_THREADS];

/* BACKGROUND BUTTON CREATED THREADS
- Scheduled by Group1 IRQ
*/
typedef struct button_task {
  void (*task)(void); // pointer to the background thread
  enum state Status; // active or free
  int priority; 
} button_task_t;

#define MAX_BUTTON_THREADS 128  // arbritrary value, TODO change if needed
int NumButtonThreads; // for allocated button threads
button_task_t s2_button_threads[MAX_BUTTON_THREADS];
button_task_t s1_button_threads[MAX_BUTTON_THREADS];
button_task_t pa28_button_threads[MAX_BUTTON_THREADS];

/* GLOBAL MAILBOX */

typedef struct mailbox {
  uint32_t mail; // shared data
  Sema4_t mail_available; // 0 means invalid data, 1 means valid data. AKA send
  Sema4_t mail_acknowledge; // 0 means consumer has not read, 1 means consumer has read. AKA Ack
} mailbox_t;

mailbox_t mailbox;

/* GLOBAL FIFO */
#define FIFOSIZE 256 // can be any size

typedef struct fifo {
  uint32_t volatile PutI; // put index
  uint32_t volatile GetI; // get index
  uint32_t data[FIFOSIZE];
  Sema4_t current_size; // 0 means FIFO is empty, > 0 means fifo has data
  Sema4_t mutex; // 1 means available, 0 means busy
  Sema4_t room_left;
  uint32_t size;
  uint32_t lost_data;
} fifo_t;

fifo_t fifo;

//Process Control Block(PCB)

typedef enum {
  PROC_FREE = 0,
  PROC_ACTIVE = 1
} proc_status_t;

typedef struct process_control_block {
  uint8_t pid;              // Process ID

  proc_status_t status;     // FREE or ACTIVE

  // Memory layout
  uint32_t StartOffset;     // pointer to code (text)
  uint32_t CodeSize;
  uint32_t DataSize;
  uint32_t StackSize;

  void *data;               // pointer to data segment (optional)

  char Name[8];             // process name

  // Thread tracking (simple version)
  uint8_t numThreads;       
} pcb_t;


pcb_t pcbs[MAX_PROCESSES];
 uint8_t CurrentPID; // set to RunPt->pid; somewhere, most likely in scheduler 

// ******** OS_ClearMsTime ************
// sets the system time to zero (solve for Lab 1), and start a periodic interrupt
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void){
  // using timer g7 for this feature
  TogglePB1();
  long sr;
  OSCRITICAL_ENTER(sr);
  TogglePB1();
  TimeMs = 0;
  TimerG7_IntArm(1000, 80, 2);  // 1ms period, priority 2: used for TimeMs
  TogglePB1();
  OSCRITICAL_EXIT(sr);
};


// ******** OS_MsTime ************
// reads the current time in msec (solve for Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// For Labs 2 and beyond, it is ok to make the resolution to match the first call to OS_AddPeriodicThread
uint32_t OS_MsTime(void){
  // put Lab 1 solution here
  //Return TimeMSG8/G7 for check
  return TimeMs;
};

void StartOS(void); // implemented in osasm.s


//------------------------------------------------------------------------------
//  Systick Interrupt Handler
//  SysTick interrupt happens every 2 ms
// used for preemptive foreground thread switch
// ------------------------------------------------------------------------------
void SysTick_Handler(void) { 
   // TogglePB4();
  SCB->ICSR = SCB_ICSR_PENDSVSET_Msk; // cause pendsv exception
                                      // which causes context switch
} // end SysTick_Handler

/*
0 = not available
1 = available
*/
int isThreadAvailable(tcb_t *RunPt) {
  return ((RunPt->sleep_st == 0) && (RunPt->blocked_ptr == 0) && (RunPt->Status == Active));
}

// TODO: probably want to add to blocked before remover from active 
void Scheduler(void) {

  // priority scheduling from main TCB pool
  int max = 255;
  tcb_t *start = RunPt->next; 
  tcb_t *pt = RunPt->next;
  tcb_t *bestPt = 0;

  do {
    if (isThreadAvailable(pt)) { 
      if (pt->priority < max || bestPt == 0) {
        max = pt->priority; 
        bestPt = pt;
      }
    }
    pt = pt->next; 
  } while(pt != start);

  if (bestPt) {
      RunPt = bestPt;
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

// Arm interrupts on fall of PB21
// interrupts will be enabled in main after all initialization
void EdgeTriggered_Init(void){

  int priority = 1;

  GPIOB->POLARITY31_16 = 0x00000800;     // falling
  GPIOB->CPU_INT.ICLR = 0x00200000;   // clear bit 21
  GPIOB->CPU_INT.IMASK = 0x00200000;  // arm PB21
  NVIC->IP[0] = (NVIC->IP[0]&(~0x0000FF00))|priority<<14;    // set priority (bits 15,14) IRQ 1
  NVIC->ISER[0] = 1 << 1; // Group1 interrupt
  
  IOMUX->SECCFG.PINCM[PA28INDEX] = (uint32_t) 0x00060081; // input, pull up
  // Falling edge on PA28
  GPIOA->POLARITY31_16 = 0x02000000;
  // Clear prior interrupt
  GPIOA->CPU_INT.ICLR = (1U << 28);
  // Arm interrupt
  GPIOA->CPU_INT.IMASK |= (1U << 28);
}


//
//@details  Initialize operating system, disable interrupts until OS_Launch.
//Initialize OS controlled I/O: uart, serial, ADC, systick, LaunchPad I/O and timers.
// Interrupts not yet enabled.
 // @param  none
 // @return none
 //@brief  Initialize OS
//
void OS_Init(void){
  // put Lab 2 (and beyond) solution here
  OSDisableInterrupts();
  OS_ClearMsTime();
  NumThreads = 0;
  NumPeriodic = 0;
  NumButtonThreads = 0;
  CurrentPID = 0;
  // mark all foreground threads as free
  for (int i = 0; i < MAXTHREADS; i++) {
    tcbs[i].Status = Free;
  }

  // mark all background threads as free
  for (int i = 0; i < MAX_PERIODIC_THREADS; i++) {
    periodic_threads[i].Status = Free;
  }
  for (int i = 0; i < MAX_BUTTON_THREADS; i++) {
    s2_button_threads[i].Status = Free;
    s1_button_threads[i].Status = Free;
    pa28_button_threads[i].Status = Free;  
      
    }
  
  //_IntArm(1000, 80, 2);  // 1ms period, priority 2: used for TimeMs

  TimerG8_IntArm(500, 80, 0);  // 1ms period, priority 0: used to run periodic background threads
  TimerG12_Init();
  EdgeTriggered_Init(); // initialize edge triggered button presses

    //comment out for test 1

   UART_Init(1); // hardware priority 1

   ST7735_InitR(INITR_BLACKTAB); //INITR_REDTAB for AdaFruit, INITR_BLACKTAB for SPI HiLetgo ST7735R
  // ST7735_FillScreen(ST7735_BLACK);
   ST7735_SetCursor(0, 0);
  //Enable Interrupts occurs at OS_Launch

  Heap_Init();
}

/* LINKED LIST HELPER FUNCTIONs */
// unlinks a thread from the active circular doubly-linked list
void RemoveFromActive(tcb_t *thread) {
  if (thread->next == thread) {
   // RunPt = 0; 
  } else {
    thread->prev->next = thread->next; 
    thread->next->prev = thread->prev; 
    
    // if (RunPt == thread) {
    //   RunPt = thread->next; //this may be an issue, changes runpt to next pt if removed thread 
    // }
  }
}

// adds thread to the blocked list
//dont quite understand this will ask
void AddToBlocked(Sema4_t *semaPt, tcb_t *thread) {
  thread->next = semaPt->BlockedPt; 
  semaPt->BlockedPt = thread;
}

// removes the highest priority from blocked list
// useful for os signal
tcb_t* RemoveHighestPriorityFromBlocked(Sema4_t *semaPt) {
  // tcb_t *pt = semaPt->BlockedPt;
  // tcb_t *bestPt = pt;
  // tcb_t *prev = 0;
  // tcb_t *bestPrev = 0;
    tcb_t *bestPt = 0;

  int maxPriority = 255; 

  // find highest priority from within blocked list
  // while (pt != 0) {
  //   if (pt->priority < maxPriority) {
  //     maxPriority = pt->priority;
  //     bestPt = pt;
  //     bestPrev = prev;
  //   }
  //   prev = pt;
  //   pt = pt->next;
  // }
  // if (bestPt != 0) {
  //   if (bestPrev == 0) {
  //     semaPt->BlockedPt = bestPt->next;
  //   } else {
  //     bestPrev->next = bestPt->next;
  //   }
  // }
  for (int i = 0; i < NumThreads; i++) {
    if (tcbs[i].Status == Blocked && tcbs[i].blocked_ptr == semaPt) {
      if (tcbs[i].priority < maxPriority) {
        maxPriority = tcbs[i].priority;
        bestPt = &tcbs[i];
      }
    }
  }


  return bestPt;
}

// Inserts a thread back into the active priority-sorted circular list.
void AddToActive(tcb_t *thread) {
  if (RunPt == 0) {  
    thread->next = thread;
    thread->prev = thread;
    RunPt = thread;
  } else {
    // case for >0 active threads
    tcb_t *pt = RunPt;
    do {
      // if (thread->priority < pt->priority) {
      //   break;   
      // }
      // pt = pt->next;
           // TODO: potential bug-
      // we stop at a thread that has higher priority
      // than the thread to be inserted
      // but don't check if the thread behind is of
      // higher priority
      // in which case we need to go behind that one
      if (thread->priority < pt->priority) {
        break; 
      }
        pt = pt->next;
    } while (pt != RunPt);

    // insert the thread
    tcb_t *prevNode = pt->prev; 
    thread->next = pt;
    thread->prev = prevNode;
    prevNode->next = thread;
    pt->prev = thread;
  }
  thread->Status = Active;
}

/* End of linked list helper functions */

// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4_t *semaPt, int32_t value){
  semaPt->Value = value;
  semaPt->BlockedPt = 0; // Initialize the blocked list as empty
}


// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
// ******** OS_Wait ************
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4_t *semaPt){

  // 1. Save the I bit, then disable interrupts
  long sr;
  OSCRITICAL_ENTER(sr);
  
  // 2. Decrement the sema4 counter
  semaPt->Value--;
  
  // 3. If the sema4 counter is less than zero, then this thread will be blocked
  if (semaPt->Value < 0) { // why not = 0

    // 3a. set status of this thread to blocked. Scheduler will remove it from main TCB pool
    RunPt->Status = Blocked;
    RunPt->blocked_ptr = semaPt; 
    
    //OSCRITICAL_EXIT(sr);
    OS_Suspend();

    // 4. restore i bit to its previous value
   // OSCRITICAL_EXIT(sr);
    //return;
  }
  
  OSCRITICAL_EXIT(sr);
}

// ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
// ******** OS_Signal ************
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
// ******** OS_Signal ************
void OS_Signal(Sema4_t *semaPt) {
  // 1. save the i bit, then disable interrupts
  long sr;
  OSCRITICAL_ENTER(sr);
  
  semaPt->Value++;
  
  if (semaPt->Value <= 0) {
    tcb_t *wokenThread = RemoveHighestPriorityFromBlocked(semaPt);
    
    if (wokenThread != 0) {
      wokenThread->blocked_ptr = 0; // Clear the blocked status
      
     // AddToActive(wokenThread);    

     wokenThread->Status = Active;
      
      // get ipsr = 0 means we're not inside an ISR
      // prevents suspending a background thread
      if ((wokenThread->priority < RunPt->priority) && (__get_IPSR() == 0)) {
       // OSCRITICAL_EXIT(sr);
        OS_Suspend(); // preempt the current thread
       // return;
      }
    }
  }
  
  OSCRITICAL_EXIT(sr);
}

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
// ******** OS_bWait ************
// Lab3 block if zero
// input:  pointer to a binary semaphore
// output: none
// ******** OS_bWait ************
// Lab3 block if zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4_t *semaPt) {
  long sr;
  OSCRITICAL_ENTER(sr);
  
  if (semaPt->Value == 0) {
    RunPt->Status = Blocked;
    RunPt->blocked_ptr = semaPt; 

    
   // OSCRITICAL_EXIT(sr);
    OS_Suspend();
    
   // OSCRITICAL_ENTER(sr); 
  }
  else{
  semaPt->Value = 0;
  }
  
  OSCRITICAL_EXIT(sr);
}

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
// ******** OS_bSignal ************
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4_t *semaPt) {
  long sr;
  OSCRITICAL_ENTER(sr);

  tcb_t *wokenThread = RemoveHighestPriorityFromBlocked(semaPt);
  if (wokenThread != 0) {
    wokenThread->blocked_ptr = 0;
    wokenThread->Status = Active;

    if ((wokenThread->priority < RunPt->priority) && (__get_IPSR() == 0)) {
      OS_Suspend();
    }
  } else {
    semaPt->Value = 1;
  }

  OSCRITICAL_EXIT(sr);
}


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

//same as addthread pretty much just with known pid
int OS_AddProcessThread(void(*task)(void), 
   void *data, uint32_t stackSize, uint32_t priority, uint32_t pid){
	   long sr;
   OSCRITICAL_ENTER(sr);
  // find a thread that is free
  int i;
  for (i = 0; i < MAXTHREADS; i++) {
    if (tcbs[i].Status == Free) {
      break;
    }
  }

  if (i == MAXTHREADS) {
    return 0; // fail upon: no thread space available
  }

 //OSCRITICAL_ENTER(sr);
  // init tcb fields
  tcbs[i].id = i;
  tcbs[i].pid = pid; 
  tcbs[i].priority = priority;
  tcbs[i].blocked_ptr = 0;
  tcbs[i].sleep_st = 0;
  tcbs[i].Status = Active;
  NumThreads++;

  // init stack
  // int32_t* stack = AllocateAndSetInitialStack(stackSize, i);
  // if (stack == 0) {
  //   OSCRITICAL_EXIT(sr);
  //   return 1; // failed allocation
  // }
  SetInitialStack(i, stackSize);

  //change R7 to datasegment
  Stacks[i][stackSize-9] = (int32_t)(data);  // R7
  Stacks[i][stackSize - 2] = (int32_t)(task); // sets the PC field on the stack to the starting address of the task

 //OSCRITICAL_ENTER(sr);
  // insert into  priority sorted circular doubly-linked list
if (RunPt == (void*)0) {  
  // first thread in system
  tcbs[i].next = &tcbs[i];
  tcbs[i].prev = &tcbs[i];
  RunPt = &tcbs[i];
} else {

  tcb_t *pt = RunPt;

  // find first node with lower priority ( higher value)
  do {
    if (priority < pt->priority) {
      break;   
    }
    pt = pt->next;
  } while (pt != RunPt);

  // insert before pt and after previous node
  tcb_t *prevNode = pt->prev; 

  tcbs[i].next = pt;
  tcbs[i].prev = prevNode;

  prevNode->next = &tcbs[i];
  pt->prev = &tcbs[i];

//prevNode <---- newNode ----> pt

}

  OSCRITICAL_EXIT(sr);
  return 1;

  
}

int OS_AddThread(void(*task)(void), uint32_t stackSize, uint32_t priority){ 
  long sr;
   OSCRITICAL_ENTER(sr);
  // find a thread that is free
  int i;
  for (i = 0; i < MAXTHREADS; i++) {
    if (tcbs[i].Status == Free) {
      break;
    }
  }

  if (i == MAXTHREADS) {
    return 0; // fail upon: no thread space available
  }

  // init tcb fields
  tcbs[i].id = i; 
  tcbs[i].pid = CurrentPID; //cant be i cause then changes for each thread in process ; this may cause and issue with
  //background threads since they dont "belong" to a process ie should be 0 but they still call os_addthread
  //was thinking to call os_addthread with os_addprocessthread with pid = 0 but doesnt work for forground
  //threads, so this just sets pid for background threads to current pid for process it interrupts which
  //idk if thats accurate 
  tcbs[i].priority = priority;
  tcbs[i].blocked_ptr = 0;
  tcbs[i].sleep_st = 0;
  tcbs[i].Status = Active;
  NumThreads++;

  // Allocate and init stack
  // int32_t* stack = AllocateAndSetInitialStack(stackSize, i);
  // if (stack == 0) {
  //   OSCRITICAL_EXIT(sr);
  //   return 1; // failed allocation
  // }
  // stack[stackSize - 2] = (int32_t)(task); // sets the PC field on the stack to the starting address of the task
  SetInitialStack(i, stackSize);  // this func was copied from the book
  Stacks[i][stackSize - 2] = (int32_t)(task);

  // insert into  priority sorted circular doubly-linked list
  if (RunPt == (void*)0) {  
    // first thread in system
    tcbs[i].next = &tcbs[i];
    tcbs[i].prev = &tcbs[i];
    RunPt = &tcbs[i];
  } else {

  tcb_t *pt = RunPt;

  // find first node with lower priority ( higher value)
  do {
    if (priority < pt->priority) {
      break;   
    }
    pt = pt->next;
  } while (pt != RunPt);

  // insert before pt and after previous node
  tcb_t *prevNode = pt->prev; 

  tcbs[i].next = pt;
  tcbs[i].prev = prevNode;

  prevNode->next = &tcbs[i];
  pt->prev = &tcbs[i];

  // prevNode <---- newNode ----> pt
  }

  OSCRITICAL_EXIT(sr);
  return 1;
}

void SetInitialStack(int i, uint32_t stackSize) {
  tcbs[i].sp = &Stacks[i][stackSize - 12];  // <-tcb[i].sp;
  Stacks[i][stackSize-1] = 0x01000000;  // thumb bit
  Stacks[i][stackSize-3] = 0x14141414;  // R14
  Stacks[i][stackSize-4] = 0x12121212;  // R12
  Stacks[i][stackSize-5] = 0x03030303;  // R3
  Stacks[i][stackSize-6] = 0x02020202;  // R2
  Stacks[i][stackSize-7] = 0x01010101;  // R1
  Stacks[i][stackSize-8] = 0x00000000; // R0
  Stacks[i][stackSize-9] = 0x07070707;  // R7
  Stacks[i][stackSize-10] = 0x06060606; // R6
  Stacks[i][stackSize-11] = 0x05050505; // R5
  Stacks[i][stackSize-12] = 0x04040404; // R4 <- thread sp (tcbs[i].sp) starts by pointing here
}

int32_t* AllocateAndSetInitialStack(uint32_t stackSize, int i) {
  int32_t* stack = (int32_t*)Heap_Malloc(stackSize * 4); // mul by 4 to convert from words to bytes
  if (stack == 0) return 0; // failed allocation

  tcbs[i].sp = &stack[stackSize - 12];  // <-tcb[i].sp;
  stack[stackSize-1] = 0x01000000;  // thumb bit
  stack[stackSize-3] = 0x14141414;  // R14
  stack[stackSize-4] = 0x12121212;  // R12
  stack[stackSize-5] = 0x03030303;  // R3
  stack[stackSize-6] = 0x02020202;  // R2
  stack[stackSize-7] = 0x01010101;  // R1
  stack[stackSize-8] = 0x00000000; // R0
  stack[stackSize-9] = 0x07070707;  // R7
  stack[stackSize-10] = 0x06060606; // R6
  stack[stackSize-11] = 0x05050505; // R5
  stack[stackSize-12] = 0x04040404; // R4 <- thread sp (tcbs[i].sp) starts by pointing here

  return stack;
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
  // should set all threads in the process to have the same pid
  long sr;
  OSCRITICAL_ENTER(sr);

  // Find free PCB slot
  int i;
  for(i = 0; i < MAX_PROCESSES; i++){
    if(pcbs[i].status == PROC_FREE){
      break;
    }
  }

  if(i == MAX_PROCESSES){
    OSCRITICAL_EXIT(sr);
    return 0; // no process slots available
  }

  // Assign PID (simple: index+1), none should be 0, 0 is reserved for initial threads
  uint8_t pid = i + 1;

 

 // Add main thread with correct PID, i dont know if we actually need to do this
    if(OS_AddProcessThread(text, data, stackSize, priority, pid) == 0){
        OSCRITICAL_EXIT(sr);
        return 0;
    }

    // Initialize PCB
    pcbs[i].pid = pid;
    pcbs[i].status = PROC_ACTIVE;
    pcbs[i].StartOffset = (uint32_t)text;
    pcbs[i].StackSize = stackSize;
    pcbs[i].numThreads = 1;

    OSCRITICAL_EXIT(sr);
    return 1;
}

// Load program from disk and launch process
//  open file for reading
//  read StartOffset,CodeSize,StackSize,DataSize,Name
//  Allocate spaces in RAM for data, stack, and code segments 
//  Reads the object code from file into the code segment
//  Closes the file
//  Add program as a process, create a thread for it, and execute
//    SP => stack segment
//    R7 => data segment
//    PC => code segment (entry point at first location)
// Inputs: name is the name of the file on SDC
//         priority is the thread priority
// Output: 1 success, 0 failure


//may be called in init
int OS_LoadProgram(char *name, uint32_t priority){
 long sr;
    OSCRITICAL_ENTER(sr);

    //  Open the program file
    if(eFile_ROpen(name)){
        OSCRITICAL_EXIT(sr);
        return 0; // fail if file can't open
    }

    //  Read program header, this could be wrong basing off of slides example
    //     .text
    //    .thumb
    //    .align 2
    //    .global ProgramBlock
    // ProgramBlock:
    //    .long Start-ProgramBlock      // offset to start
    //    .long EndProcess-ProgramBlock // size of code segment
    //    .long 128                     // size of stack segment
    //    .long DataSize                // size of data segment
    //    .string "Fuzzy"               // program name
    //    .align 2     
    // Start:

    // EndProcess:
    Program_t prog;
    if(eFileReadNextWord(&prog.StartOffset)) { eFile_RClose(); OSCRITICAL_EXIT(sr); return 0; }
    if(eFileReadNextWord(&prog.CodeSize)) { eFile_RClose(); OSCRITICAL_EXIT(sr); return 0; }
    if(eFileReadNextWord(&prog.StackSize)) { eFile_RClose(); OSCRITICAL_EXIT(sr); return 0; }
    if(eFileReadNextWord(&prog.DataSize)) { eFile_RClose(); OSCRITICAL_EXIT(sr); return 0; }

    
    for(int i = 0; i < 8; i++){
        char letter;
        if(eFile_ReadNext(&letter)){ eFile_RClose(); OSCRITICAL_EXIT(sr); return 0; }
        prog.Name[i] = letter;
    }
    
    //  Allocate code and data segments on heap, stack separate 
    uint8_t *codeSegment = (uint8_t*)Heap_Malloc(prog.CodeSize); // dont think pid would be assigned correctly
    uint8_t *dataSegment = (uint8_t*)Heap_Malloc(prog.DataSize); //would need to search for pid first and not in addprocess?
    if(!codeSegment || !dataSegment){
        eFile_RClose();
        OSCRITICAL_EXIT(sr);
        return 0;
    }
    void *entryPoint = (void *)(codeSegment); // + prog.StartOffset);

    // Read object code into code segment
    for(uint32_t i = 0; i < prog.CodeSize - prog.StartOffset; i++){
        char byte;
        if(eFile_ReadNext(&byte)){  // fail if EOF or read error
            eFile_RClose();
            OSCRITICAL_EXIT(sr);
            return 0;
        }
        codeSegment[i] = (uint8_t)byte;
    }

    // im unsure if we read data into data segment 
    
    
    // void *entryPoint = codeSegment + prog.StartOffset;

    // Close the file
    if(eFile_RClose())  {
      OSCRITICAL_EXIT(sr);
      return 0;
    }
    
    // Add process with main thread
    if(OS_AddProcess(entryPoint, dataSegment, prog.StackSize, priority) == 0){
        OSCRITICAL_EXIT(sr);
        return 0; // failed to create process
    }

    OSCRITICAL_EXIT(sr);
    return 1; 
}



// ******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
uint32_t OS_Id(void){
  // put Lab 2 (and beyond) solution here
  return RunPt->id;
}

uint32_t OS_PId(void) {
  return RunPt->pid;
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
  long sr;

  // find an available thread
  int i;
  for (i = 0; i < MAX_PERIODIC_THREADS; i++) {
    if (periodic_threads[i].Status == Free) {
      break;
    }
  }

  if (i == MAX_PERIODIC_THREADS) {
    return 0; // fail upon no space available
  }
 
  int j = i;
  OSCRITICAL_ENTER(sr);
// Shift higher priority elements up
  while (j > 0 &&
       periodic_threads[j-1].Status == Active &&
       periodic_threads[j-1].priority > priority) {

    periodic_threads[j] = periodic_threads[j-1];
    j--;
  }

  // init bg thread
  periodic_threads[j].task = task;
  periodic_threads[j].period = period;
  periodic_threads[j].timeLeft = period;
  periodic_threads[j].priority = priority;
  periodic_threads[j].Status = Active;
  periodic_threads[j].nextTriggerTime = OS_MsTime() + period;
  NumPeriodic++;

  OSCRITICAL_EXIT(sr);
  return 1;
}

/* TIMG8 locks or unlocks periodic threads */
void TIMG8_IRQHandler(void){
  //TogglePB20();
  if((TIMG8->CPU_INT.IIDX) == 1){ // this will acknowledge
    TimeMsG8++;
  //  TogglePB20();
    for (int i = 0; i < NumPeriodic; i++) {
      if (periodic_threads[i].Status == Active) {
        if (periodic_threads[i].timeLeft == 0) {
          (*periodic_threads[i].task)();  // run the bg thread when time has run out
          periodic_threads[i].timeLeft = periodic_threads[i].period - 1;  // reload counter
        } else {
          periodic_threads[i].timeLeft--; // decrement
        }
      }
    }
   // TogglePB20();
  }
}

/* 
TIMG7 handles the millisecond clock 
and decrements sleeping threads
*/
void TIMG7_IRQHandler(void){
  //TogglePB1();
  if((TIMG7->CPU_INT.IIDX) == 1){ // this will acknowledge
   // TogglePB1();
    TimeMs++; // increment the millisecond clock
    //TogglePB1();
    // decrement any sleeping threads once every ms
    for (int i = 0; i < NumThreads; i++) {
      if ((tcbs[i].sleep_st > 0) && (tcbs[i].Status == Active)) {
        tcbs[i].sleep_st--;
      }
    }
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
    // check all available button threads
    for (int i = 0; i < MAX_BUTTON_THREADS; i++) {
      if (s1_button_threads[i].Status == Active) {
        (*s1_button_threads[i].task)(); // run the background thread
      }
    }
  }
  
  if(GPIOA->CPU_INT.RIS&(1<<28)){ // PA28
    GPIOA->CPU_INT.ICLR = 1<<28;
    for (int i = 0; i < MAX_BUTTON_THREADS ; i++) {
      if (pa28_button_threads[i].Status == Active) {
        (*pa28_button_threads[i].task)(); // run the background thread
      }
    }
  }

  if(GPIOB->CPU_INT.RIS&(1<<21)){ // PB21
    GPIOB->CPU_INT.ICLR = 1<<21;  // this acknowledges interrupt
    // check all available button threads
    for (int i = 0; i < NumButtonThreads; i++) {
      if (s2_button_threads[i].Status == Active) {
        (*s2_button_threads[i].task)(); // run the background thread
      }
    }
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
  long sr;

  // find an available thread
  int i;
  for (i = 0; i < MAX_BUTTON_THREADS; i++) {
    if (s1_button_threads[i].Status == Free) {
      break;
    }
  }

  if (i == MAX_BUTTON_THREADS) {
    return 0; // failed to add task
  }

  OSCRITICAL_ENTER(sr);
  // init background thread
  s1_button_threads[i].task = task;
  s1_button_threads[i].priority = priority;
  s1_button_threads[i].Status = Active;
  // can kill a button thread by deactivating its task
    // and marking the thread as free

  OSCRITICAL_EXIT(sr);
  return 1; // successfully added task
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
  long sr;

  // find an available thread
  int i;
  for (i = 0; i < MAX_BUTTON_THREADS; i++) {
    if (s2_button_threads[i].Status == Free) {
      break;
    }
  }

  if (i == MAX_BUTTON_THREADS) {
    return 0; // failed to add task
  }
  int j = i;
  OSCRITICAL_ENTER(sr);
  // Shift higher priority elements up
  while (j > 0 &&
         s2_button_threads[j-1].Status == Active &&
         s2_button_threads[j-1].priority > priority) {
          
    s2_button_threads[j] = s2_button_threads[j-1];
    j--;
  }

  // init background thread
  s2_button_threads[i].task = task;
  s2_button_threads[i].priority = priority;
  s2_button_threads[i].Status = Active;
   NumButtonThreads++;
  // can kill a button thread by deactivating its task
    // and marking the thread as free

  OSCRITICAL_EXIT(sr);
  return 1; // successfully added task
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

  //need to implement priority here 
  long sr;

  int i;
  for (i = 0; i < MAX_BUTTON_THREADS; i++) {
    if (pa28_button_threads[i].Status == Free) {
      break;
    }
  }

  if (i == MAX_BUTTON_THREADS) {
    return 0;
  }

  int j = i;
  OSCRITICAL_ENTER(sr);

  while (j > 0 &&
         pa28_button_threads[j-1].Status == Active &&
         pa28_button_threads[j-1].priority > priority) {
          
    pa28_button_threads[j] = pa28_button_threads[j-1];
    j--;
  }
  
  // init background thread
  pa28_button_threads[i].task = task;
  pa28_button_threads[i].priority = priority;
  pa28_button_threads[i].Status = Active;
  NumButtonThreads++;
  OSCRITICAL_EXIT(sr);
  
  return 1; // successfully added task
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
  RunPt->sleep_st = sleepTime;  // Run_pt->sleep_st will be decremented with TimG8 every ms
  OSCRITICAL_EXIT(sr);
  OS_Suspend();
} 



// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
//shorter os kill
void OS_Kill(void){
  long sr;
  OSCRITICAL_ENTER(sr);

  RunPt->Status = Free;
  NumThreads--;
  RemoveFromActive(RunPt);

  OSCRITICAL_EXIT(sr);

  OS_Suspend();

  while(1){};
}



// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void){
  SysTick->VAL = 0; // reset counter
  SCB->ICSR = SCB_ICSR_PENDSVSET_Msk; // trigger SysTick
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
  fifo.GetI = 0; // empty
  fifo.PutI = 0; // empty
  fifo.lost_data = 0; // no data lost yet
  if(size >FIFOSIZE){
    return;
  }
  OS_InitSemaphore(&fifo.current_size, 0);
  OS_InitSemaphore(&fifo.room_left, size);
  OS_InitSemaphore(&fifo.mutex, 1);
  fifo.size = size;
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
  long sr;
  OSCRITICAL_ENTER(sr);
  uint32_t newPutI = (fifo.PutI+1)&(fifo.size-1);
  if (newPutI == fifo.GetI){ // FIFO Full 
    fifo.lost_data++;
    return 0;
    
  } else {
    fifo.data[(fifo.PutI & (fifo.size - 1))] = data; // put in FIFO
    fifo.PutI = newPutI;
  }
 OSCRITICAL_EXIT(sr);
 OS_Signal(&fifo.current_size);
  return 1; 
}

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
uint32_t OS_Fifo_Get(void){long sr;
  
  OS_Wait(&fifo.current_size);// block if empty
  OS_Wait(&fifo.mutex); // block if busy
  OSCRITICAL_ENTER(sr);
  uint32_t data;
  if (fifo.PutI == fifo.GetI){ // FIFO is empty, this should never run cause we would block if empty
    return 0;
  } else {
    data  = fifo.data[(fifo.GetI & (fifo.size - 1))];
    fifo.GetI = (fifo.GetI+1) & (fifo.size - 1);
  }
  OSCRITICAL_EXIT(sr);
  OS_Signal(&fifo.mutex);
  OS_Signal(&fifo.room_left);
  return data;
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
 return fifo.PutI - fifo.GetI;// replace this line with solution
  //return 0;
}
// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void){
  // put Lab 2 (and beyond) solution here
  OS_InitSemaphore(&mailbox.mail_available, 0);
  OS_InitSemaphore(&mailbox.mail_acknowledge, 0);
}

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(uint32_t data){
  // put Lab 2 (and beyond) solution here
  mailbox.mail = data;
  OS_Signal(&mailbox.mail_available); // signal that mail is available
  OS_Wait(&mailbox.mail_acknowledge); // don't send again until the consumer gets the mail
};

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
uint32_t OS_MailBox_Recv(void){
  // put Lab 2 (and beyond) solution here
  uint32_t data;
  OS_Wait(&mailbox.mail_available);
  data = mailbox.mail;
  OS_Signal(&mailbox.mail_acknowledge);
  return data;
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
  return TIMG12->COUNTERREGS.CTR;

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
  long sr;
  OSCRITICAL_ENTER(sr)
  uint32_t result = 0;
    if (stop >= start) {
      result = stop - start;
    } else {
      result = start - stop;
    }
    OSCRITICAL_EXIT(sr);
    return result;
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
  //uncomment after test 1
  int systick_priority = 2;
  int pendsv_priority = 3;

  NVIC_SetPriority(PendSV_IRQn, pendsv_priority); // set pendsv priority 3

  SysTick->CTRL = 0x00; // disable systick during setup
  SysTick->LOAD = theTimeSlice - 1; // reload value
  SCB->SHP[1] = (SCB->SHP[1]&(~0xC0000000)) | systick_priority<<30; // set systick priority 2
  SysTick->VAL = 0; // clear count, cause reload
  SysTick->CTRL = 0x07; // enable systick irq and systick timer
  StartOS();  // start on the first task (implemented in osasm.s)
}