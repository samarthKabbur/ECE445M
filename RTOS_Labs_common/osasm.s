/* ***************************************************************************
; osasm.s: low-level OS commands, written in assembly                       
; derived from uCOS-II                                                     
;****************************************************************************
;OS Lab2/3/4/5/6 
;Students will implement these functions as part of ECE445M Lab
;Starter to labs 1,2,3,4
; Jonathan Valvano
; December 21, 2025
;
;Copyright 2025 by Jonathan W. Valvano, valvano@mail.utexas.edu
;    You may use, edit, run or distribute this file
;    as long as the above copyright notice remains
; THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
; OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
; MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
; VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
; OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
; For more information about my classes, my research, and my books, see
; http://users.ece.utexas.edu/~valvano/
; 
*/
         .data
        .align 2
// Declare global variables here if needed
// with the .space assembly directive

     .text
     .thumb
     .align 2
     .global  RunPt            // currently running thread
     .global  NextThreadPt     // next thread to run, set by schedule
     .global  StartOS
     .global  ContextSwitch2
     .global  PendSV_Handler
     .global  OSDisableInterrupts
     .global  OSEnableInterrupts 
     .global  SVC_Handler
     .global  StartCritical
     .global  EndCritical 

OSDisableInterrupts:
        CPSID   I
        BX      LR

OSEnableInterrupts:
        CPSIE   I
        BX      LR

// *********** StartCritical ************************
//make a copy of previous I bit, disable interrupts
// inputs :  none
// outputs : R0=previous I bit
StartCritical:
        MRS    R0, PRIMASK  // save old status
        CPSID  I           // mask all (except faults)
        BX     LR

// *********** EndCritical ************************
// using the copy of previous I bit, restore I bit to previous value
// inputs :  R0=previous I bit
// outputs : none
EndCritical:
        MSR    PRIMASK, R0
        BX     LR


StartOS:
// put your code here


OSStartHang:
    B       OSStartHang        // Should never get here


    

/* *******************************************************************************************************
;                                         HANDLE PendSV EXCEPTION
;                                     void PendSV_Handler(void)
;
; Note(s) : 1) PendSV is used to cause a context switch.  This is a recommended method for performing
;              context switches with Cortex-M.  This is because the Cortex-M auto-saves half of the
;              processor context on any exception, and restores same on return from exception.  So only
;              saving of R4-R7 is required and fixing up the stack pointers.  Using the PendSV exception
;              this way means that context saving and restoring is identical whether it is initiated from
;              a thread or occurs due to an interrupt or exception.
;
;           2) Pseudo-code is:
;              a) disable interrupts so context switch is atomic
;              b) Push remaining R4-R7 on stack;
;              c) Save the SP in its TCB, 
;              d) Get next thread to run from the OS.c
;              e) Load SP of next thread into SP 
;              f) Pop R4-R7 from new stack;
;              g) enable interrupts 
;              i) Perform exception return which will restore remaining context.
;
;           3) On entry into PendSV handler:
;              a) The following have been saved on the process stack (by processor):
;                 xPSR, PC, LR, R12, R0-R3
;              b) RunPt         points to the tcb of the task to suspend
;              c) NextThreadPt  points to the tcb of the task to resume
;
;           4) Since PendSV is set to lowest priority in the system, we
;              know that it will only be run when no other exception or interrupt is active, and
;              therefore safe to assume that context being switched out was a user thread and not an ISR.
;
;           5) We will assume (hope) the compiler does not use R8-R11
;
;********************************************************************************************************/
.type PendSV_Handler, %function
// Only ISR running at priority 3 guarantees it interrupts only main/foreground code
PendSV_Handler:
// put your code here

   
    
    BX      LR                 // Exception return will restore remaining context   
    

/* *******************************************************************************************************
;                                         HANDLE SVC EXCEPTION
;                                     void SVC_Handler(void)
;
; Note(s) : SVC is a software-triggered exception to make OS kernel calls from user land. 
;           The function ID to call is encoded in the instruction itself, the location of which can be
;           found relative to the return address saved on the stack on exception entry.
;           Function-call paramters in R0..R3 are also auto-saved on stack on exception entry.
;
;********************************************************************************************************/

.type OS_Id, %function
.type OS_Kill, %function
.type OS_Sleep, %function
.type OS_Time, %function
.type OS_AddThread, %function
.type ST7735_Message, %function
        .global    OS_Id
        .global    OS_Kill
        .global    OS_Sleep
        .global    OS_Time
        .global    OS_AddThread
        .global    ST7735_Message
.type SVC_Handler, %function
SVC_Handler:
    PUSH    {R4-R5,LR}

    POP     {R4-R5,PC}
SVCJumpTable:
    .long       OS_Id
    .long       OS_Kill
    .long       OS_Sleep
    .long       OS_Time
    .long       OS_AddThread    
    .long       ST7735_Message


    .end

