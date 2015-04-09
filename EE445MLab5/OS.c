#include "tm4c123gh6pm.h"
#include "PLL.h"
#include <stdint.h>
#include "OS.h"
#include "UART.h"

#define PREEMPTIVE 1

#define NUM_THREADS 9 //Maximum number of threads
#define STACK_SIZE 500  //number of 32-bit words in stack

#define SUCCESS 1
#define FAILURE 0

#define ACTIVE 1
#define INACTIVE 0

#define DUMMY 0	//some function prototypes have arguments that are not used in Lab 2
								//DUMMY is passed to them in their place

#define NVIC_EN0_INT19          0x00080000  // Interrupt 19 enable
#define TIMER_CFG_32_BIT_TIMER  0x00000000  // 32-bit timer configuration
#define TIMER_TAMR_TACDIR       0x00000010  // GPTM Timer A Count Direction
#define TIMER_TAMR_TAMR_PERIOD  0x00000002  // Periodic Timer mode
#define TIMER_CTL_TAEN          0x00000001  // GPTM TimerA Enable
#define TIMER_IMR_TATOIM        0x00000001  // GPTM TimerA Time-Out Interrupt
                                            // Mask
#define TIMER_ICR_TATOCINT      0x00000001  // GPTM TimerA Time-Out Raw
                                            // Interrupt
#define TIMER_TAILR_M           0xFFFFFFFF  // GPTM Timer A Interval Load
                                            // Register
#define TIMER1_TAR_R            (*((volatile uint32_t *)0x40031048))
#define LED (*((volatile uint32_t *)0x4000703C))
#define PE4  (*((volatile unsigned long *)0x40024040))	// added for portf handler

void DisableInterrupts(void);
void EnableInterrupts(void);
void OS_DisableInterrupts(void);
void OS_EnableInterrupts(void);
void StartOS(void);
int32_t StartCritical(void);
void EndCritical(int32_t);

void (*PeriodicTask)(void);   // user function


/******************************TCB Declarations********************************/
struct tcb{
  int *stackPt;  //pointer to stack, valid for threads not running
	struct tcb *nextPt; //linked-list pointer
	struct tcb *prevPt;
  unsigned int id;
  unsigned int sleepState;
  unsigned int priority;
  unsigned int blockedState;
};
typedef struct tcb tcbType;
tcbType tcbs[NUM_THREADS];
int activeThreads[NUM_THREADS];	//Track the active threads
tcbType *RunPt;
int32_t Stacks[NUM_THREADS][STACK_SIZE];
int numThreadsCreated = 0;  //Track number of threads

#define PF4 (*((volatile uint32_t *) 0x40025040))
#define PF0 (*((volatile uint32_t *) 0x40025004))
void (*switchTaskPF0) (void);
void (*switchTaskPF4) (void);
uint32_t static LastPF4;  //Previous value

void SetInitialStack(int i){
  tcbs[i].stackPt = &Stacks[i][STACK_SIZE-16]; //Thread stack pointer
  Stacks[i][STACK_SIZE-1] = 0x01000000; //Thumb bit
  Stacks[i][STACK_SIZE-3] = 0x14141414; //R14
  Stacks[i][STACK_SIZE-4] = 0x12121212; //R12
  Stacks[i][STACK_SIZE-5] = 0x03030303; //R3
  Stacks[i][STACK_SIZE-6] = 0x02020202; //R2
  Stacks[i][STACK_SIZE-7] = 0x01010101; //R1
  Stacks[i][STACK_SIZE-8] = 0x00000000; //R0
  Stacks[i][STACK_SIZE-9] = 1111111111; //R11
  Stacks[i][STACK_SIZE-10] = 0x10101010;  //R10
  Stacks[i][STACK_SIZE-11] = 0x09090909;  //R9
  Stacks[i][STACK_SIZE-12] = 0x08080808;  //R8
  Stacks[i][STACK_SIZE-13] = 0x07070707;  //R7
  Stacks[i][STACK_SIZE-14] = 0x06060606;  //R6
  Stacks[i][STACK_SIZE-15] = 0x05050505;  //R5
  Stacks[i][STACK_SIZE-16] = 0x04040404;  //R4
}

void dummy3(void){}
void Timer1_Init(void){
	long sr;
  sr = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x02; // timer1 instead
	
  TIMER1_CTL_R &= ~TIMER_CTL_TAEN; // 1) disable timer1A during setup
                                   // 2) configure for 32-bit timer mode
  TIMER1_CFG_R = TIMER_CFG_32_BIT_TIMER;
                                   // 3) configure for periodic mode, default down-count settings
  TIMER1_TAMR_R = TIMER_TAMR_TAMR_PERIOD;
  TIMER1_TAILR_R = 0xFFFFFFFF-1;       // 4) reload value
                                   // 5) clear timer1A timeout flag
  TIMER1_ICR_R = TIMER_ICR_TATOCINT;
  TIMER1_IMR_R |= TIMER_IMR_TATOIM;// 6) arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF00FF)|0x00004000; // 7) priority 2
  NVIC_EN0_R = NVIC_EN0_INT21;     // 8) enable interrupt 21 in NVIC
  TIMER1_TAPR_R = 0;
  TIMER1_CTL_R |= TIMER_CTL_TAEN;  // 9) enable timer1A
  EndCritical(sr);
}


/*****************************Public OS Functions******************************/
// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers
// input:  none
// output: none
void OS_Init(void){
	int index;
	
	OS_DisableInterrupts();
	UART_Init();
	PLL_Init(); //Set processor clock
	Timer1_Init();

  /*Configure SysTick to only interrupt foreground threads*/
  NVIC_ST_CTRL_R = 0; //disable SysTick during setup
  NVIC_ST_CURRENT_R = 0;  //any write to current clears it
  NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0x00FFFFFF) | 0xE0000000; //priority 7

	for(index = 0; index < NUM_THREADS; index++){
		activeThreads[index] = 0;		//initialize all threads to inactive
	}
	OS_EnableInterrupts();
}


//******** OS_AddThread ***************
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
int OS_AddThread(void (*task)(void),
  unsigned long stackSize, unsigned long priority){
		int32_t status;
		int index = 0;
		int index2;
		status = StartCritical();

		//First thread created
		if (numThreadsCreated < 1){
			tcbs[index].nextPt = &tcbs[index];
			tcbs[index].prevPt = &tcbs[index];
			RunPt = &tcbs[index];

			tcbs[index].id = index;

			SetInitialStack(index); //Initialize Stack for thread
			Stacks[index][STACK_SIZE-2] = (int32_t)(task);

			numThreadsCreated++; // keeps track of the number of threads created
			activeThreads[index] = ACTIVE;
			tcbs[index].id = index;
			EndCritical(status);

			return SUCCESS;
		}
		else if(numThreadsCreated == NUM_THREADS){
			EndCritical(status);
			return FAILURE;	//maximum number of threads created
		}

		//Find next avaialable free thread
		for(index = 0; index < NUM_THREADS; index++){
			if(activeThreads[index] == INACTIVE){
				break;
			}
		}

		//Update linked list
		if(numThreadsCreated > 1){
			//find first active thread
			for(index2 = 0; index2 < NUM_THREADS; index2++){
				if(activeThreads[index2] == ACTIVE){
					break;
				}
			}
			//insert thread
			tcbs[index].nextPt = tcbs[index2].nextPt;
			tcbs[index].prevPt = &tcbs[index2];
			tcbs[index2].nextPt = &tcbs[index];
			(tcbs[index].nextPt)->prevPt = &tcbs[index];	//update threads prevPt to address of inserted thread
		}
		else{
			//find activeThread
			for(index2 = 0; index2 < NUM_THREADS; index2++){
				if(activeThreads[index2] == ACTIVE){
					break;
				}
			}
			//insert thread
			tcbs[index].nextPt = tcbs[index2].nextPt;
			tcbs[index].prevPt = &tcbs[index2];

			tcbs[index2].nextPt = &tcbs[index];
			tcbs[index2].prevPt = &tcbs[index];
		}

		SetInitialStack(index); //Initialize Stack for thread
		Stacks[index][STACK_SIZE-2] = (int32_t)(task);

		numThreadsCreated++; // keeps track of the number of threads created
		activeThreads[index] = ACTIVE;
		tcbs[index].id = index;
		EndCritical(status);

		return SUCCESS;
}


//******** OS_Launch ***************
// start the scheduler, enable interrupts
// Inputs: number of 12.5ns clock cycles for each time slice
//         you may select the units of this parameter
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick
void OS_Launch(uint32_t theTimeSlice){
#if PREEMPTIVE
	NVIC_ST_RELOAD_R = theTimeSlice - 1; //reload value
  NVIC_ST_CTRL_R = 0x00000007;  //Enable, core clock and interrupt arm
#endif
	StartOS();
}

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void){
#if PREEMPTIVE
  NVIC_ST_CURRENT_R = 0;  //Clear counter
#endif
  NVIC_INT_CTRL_R = 0x04000000; //Trigger Systick interrupt
}

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
	int index;
	int32_t status;

	status = StartCritical();

	if(numThreadsCreated == 1){
		//only 1 thread Active, runPt will be unchanged
		//find active thread
		for(index = 0; index < NUM_THREADS; index++){
			if(activeThreads[index] == ACTIVE){
				break;
			}
		}
		activeThreads[index] = INACTIVE;
		--numThreadsCreated;

		return;
	}

	//otherwise linked list must be maintained
	(RunPt->nextPt)->prevPt = (RunPt->prevPt);
	(RunPt->prevPt)->nextPt = (RunPt->nextPt);

	activeThreads[RunPt->id] = 0;
	--numThreadsCreated;

	EndCritical(status);
	OS_Suspend();	//Force context switch

}

//******** OS_Id ***************
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero
unsigned long OS_Id(void){
  return RunPt->id;
}

// ******** OS_Wait ************
// decrement semaphore
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *s) {
  DisableInterrupts();
  while((*s).Value <= 0){
    EnableInterrupts();
    DisableInterrupts();
  }
  (*s).Value = (*s).Value - 1;
  EnableInterrupts();
}

// ******** OS_Signal *********fi***
// increment semaphore
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *s) {
  long status;
  status = StartCritical();
  (*s).Value = (*s).Value + 1;
  EndCritical(status);
}

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *s) {
  DisableInterrupts();
  while((*s).Value == 0){
    EnableInterrupts();
    DisableInterrupts();
  }
  (*s).Value =  0;
  EnableInterrupts();
}

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *s) {
  long status;
  status = StartCritical();
  (*s).Value = 1;
  EndCritical(status);
}

// ******** OS_InitSemaphore ************
// initialize semaphore
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *s, long value){
  int32_t status;
  status = StartCritical();
  (*s).Value = value;
  EndCritical(status);
}

//******** OS_AddSW1Task ***************
// add a background task to run whenever the SW1 (PF4) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal	 OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field
//           determines the relative priority of these four threads
#define PF4 (*((volatile uint32_t *) 0x40025040))
void (*switchTask) (void);
uint32_t static LastPF4;  //Previous value
int OS_AddSW1Task(void(*task)(void), unsigned long priority){
  SYSCTL_RCGCGPIO_R |= 0x20;  //Activate port F
  switchTaskPF4 = task;
  GPIO_PORTF_DIR_R &= ~0x10; //Make PF4 input
  GPIO_PORTF_DEN_R |= 0x10; //Enable digital I/O on PF4
  GPIO_PORTF_IS_R &= ~0x10; //PF4 is edge-sensitive
  GPIO_PORTF_IBE_R |= 0x10; //PF4 is both edges
  GPIO_PORTF_ICR_R = 0x10;  //Clear flag4
  GPIO_PORTF_IM_R |= 0x10;  //Enable interrupt on PF4
  GPIO_PORTF_PUR_R |= 0x10; //PF4 does not have pullup
  LastPF4 = PF4;
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF) | (priority<<21); //3, bits 23-21
  NVIC_EN0_R = NVIC_EN0_INT30; //Enable interrupt 3 in NVIC
  return SUCCESS;
}

void GPIO_Init(void){
  SYSCTL_RCGCGPIO_R |= 0x08;        // 1) activate port D
  while((SYSCTL_PRGPIO_R&0x08)==0){};   // allow time for clock to stabilize
                                    // 2) no need to unlock PD3-0
  GPIO_PORTD_AMSEL_R &= ~0x0F;      // 3) disable analog functionality on PD3-0
  GPIO_PORTD_PCTL_R &= ~0x0000FFFF; // 4) GPIO
  GPIO_PORTD_DIR_R |= 0x0F;         // 5) make PD3-0 out
  GPIO_PORTD_AFSEL_R &= ~0x0F;      // 6) regular port function
  GPIO_PORTD_DEN_R |= 0x0F;         // 7) enable digital I/O on PD3-0

		SYSCTL_RCGCGPIO_R |= 0x20;       // activate port F
  while((SYSCTL_PRGPIO_R&0x0020) == 0){};// ready?
  GPIO_PORTF_DIR_R |= 0x0E;        // make PF3-1 output (PF3-1 built-in LEDs)
  GPIO_PORTF_AFSEL_R &= ~0x0E;     // disable alt funct on PF3-1
  GPIO_PORTF_DEN_R |= 0x0E;        // enable digital I/O on PF3-1
                                   // configure PF3-1 as GPIO
  GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFF000F)+0x00000000;
  GPIO_PORTF_AMSEL_R = 0;          // disable analog functionality on PF
}

//Debounce Task for PF4 Switch
void static DebounceTaskPF4(void){
  OS_Sleep(2);  //Foreground sleeping, must run within 50ms
  LastPF4 = PF4;  //Read while it is not bouncing
  GPIO_PORTF_IM_R |= 0x10;  //Enable interrupt on PF4
  OS_Kill();
}

void GPIOPortF_Handler(void){
  if(GPIO_PORTF_MIS_R & 0x00000010){
		PE4 ^= 0x10;
    //PF4 Event
    GPIO_PORTF_IM_R &= ~0x10; //Disarm interrupt on PF4
    GPIO_PORTF_ICR_R = 0x10;  //clear flag4
    if(!OS_AddThread(&DebounceTaskPF4, DUMMY, DUMMY)){
      //Add Thread failed, re-enable interrupt and don't execute user task to avoid
      //cluttering cpu
      GPIO_PORTF_IM_R |= 0x10;  //Enable interrupt on PF4
      return;
    }
    if(LastPF4 & 0x00000010){   //If previous was high this is falling edge
      (*switchTask)();
    }
		PE4 ^= 0x10;
  }
}

// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
unsigned long mail;
Sema4Type mailBoxSendSema4;
Sema4Type mailBoxAckSema4;
void OS_MailBox_Init(void){
  OS_InitSemaphore(&mailBoxSendSema4, 0); //No mail available
  OS_InitSemaphore(&mailBoxAckSema4, 0); //No consumer waiting for data
}

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received
void OS_MailBox_Send(unsigned long data){
  mail = data;
  OS_Signal(&mailBoxSendSema4);
  OS_Wait(&mailBoxAckSema4);
}

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty
unsigned long OS_MailBox_Recv(void){
  unsigned long data;
  OS_Wait(&mailBoxSendSema4);
  data = mail;
  OS_Signal(&mailBoxAckSema4);
  return data;
}

// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128

/*ignoring size parameter and using predefined value instead for lab 2*/
#define FIFO_SIZE 256
unsigned long fifo[FIFO_SIZE];
Sema4Type mutex;
Sema4Type currentSize;
unsigned long * putPt;
unsigned long * getPt;
int lostData;
void OS_Fifo_Init(unsigned long size){
  OS_InitSemaphore(&mutex, 1);
  OS_InitSemaphore(&currentSize, 0);
  putPt = &fifo[0];
  getPt = &fifo[0];
	lostData = 0;
}

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers
//  this function can not disable or enable interrupts
int OS_Fifo_Put(unsigned long data){
	unsigned long *nextPutPt;
	nextPutPt = putPt + 1;
	if(nextPutPt == &fifo[FIFO_SIZE]){
		nextPutPt = &fifo[0];	//wrap
	}
	if(nextPutPt == getPt){
		lostData++;
		return FAILURE;
	}
	else{
		*(putPt) = data;
		putPt = nextPutPt;
		OS_Signal(&currentSize);
	}
	return SUCCESS;
}

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data
unsigned long OS_Fifo_Get(void){
	int data;
	OS_Wait(&currentSize);
	OS_Wait(&mutex);
	data = *(getPt++);
	if(getPt == &fifo[FIFO_SIZE]){
		getPt = &fifo[0];
	}
	OS_Signal(&mutex);
	return data;
}

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
long OS_Fifo_Size(void){
  return currentSize.Value;
}

void Timer5_Init(void(*task)(void), unsigned long period, unsigned long priority){
	long sr;
	priority &= 0x7; // 3 bits to set priority
	priority = priority << 5; // bits 5-7 set priority in nvic_pri23
  sr = StartCritical();
	SYSCTL_RCGCTIMER_R |= 0X20; // TIMER 5
	PeriodicTask = task;				// user function
	 TIMER5_CTL_R = 0; // 1) disable timer5A during setup
                                   // 2) configure for 32-bit timer mode
  TIMER5_CFG_R = 0;
                                   // 3) configure for periodic mode, default down-count settings
  TIMER5_TAMR_R = 2;
  TIMER5_TAILR_R = period-1;       // 4) reload value
                                   // 5) clear timer5A timeout flag
  TIMER5_ICR_R = 1;
  TIMER5_IMR_R |= 1;								// 6) arm timeout interrupt
  NVIC_PRI23_R = (NVIC_PRI23_R&0xFFFFFF)|priority; // 7) priority set
  NVIC_EN2_R = 0x10000000;     // 8) enable IRQ 92 in NVIC
  TIMER5_TAPR_R = 0;
  TIMER5_CTL_R |= TIMER_CTL_TAEN;  // 9) enable timer5A
  EndCritical(sr);
}

void Timer5A_Handler(void){
	TIMER5_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer5A timeout
  (*PeriodicTask)();                // execute user task
}

//******** OS_AddPeriodicThread ***************
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// You are free to select the time resolution for this function
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal	 OS_AddThread
// This task does not have a Thread ID
// In lab 2, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, this command will be called 0 1 or 2 times
// In lab 3, there will be up to four background threads, and this priority field
//           determines the relative priority of these four threads
int OS_AddPeriodicThread(void(*task)(void), // uses timer 5
 unsigned long period, unsigned long priority){
	 Timer5_Init(task, period, priority);
	 return 1;
 }

void OS_ClearPeriodicTime(void){
	TIMER5_TAR_R = 0;
	return;
}

unsigned long OS_ReadPeriodicTime(void){
	return TIMER5_TAR_R;
}


void Timer1A_Handler(void){
  TIMER1_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer1A timeout
  (*PeriodicTask)();                // execute user task
}

// ******** OS_Time ************
// return the system time
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as
//   this function and OS_TimeDifference have the same resolution and precision
unsigned long OS_Time(void){
	return TIMER1_TAR_R;
}

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as
//   this function and OS_Time have the same resolution and precision
unsigned long OS_TimeDifference(unsigned long start, unsigned long stop){
	return start - stop;
}

// ******** OS_ClearMsTime ************
// sets the system time to zero (from Lab 1)
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void){
	TIMER1_TAR_R = 0;
}

// ******** OS_MsTime ************
// reads the current time in msec (from Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// It is ok to make the resolution to match the first call to OS_AddPeriodicThread
unsigned long OS_MsTime(void){
	return TIMER1_TAR_R / TIME_1MS;
}


// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(unsigned long sleepTime){
	RunPt->sleepState = sleepTime + 1;
}
