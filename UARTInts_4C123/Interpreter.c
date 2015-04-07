/*
Interpreter was written to allow the user to send commands to the LCD, UART, ADC, and OS
*/

#include <stdio.h>
#include <rt_misc.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "UART.h"
#include "tm4c123gh6pm.h"
#include "ADC.h"
#include "ST7735.h"
#include "OS.h"
#include "Lab2.h"
#include "efile.h"
extern int Fset;
extern int Dset;
extern unsigned long ADCarray[102];
extern unsigned long ADCarray2[102];
extern long FFTOutput[64];
extern long Y[102];
extern int StreamToFile; // 0 = uart, 1 = file

/*copied from periodic32bitT0Ints.c*/
#define PF1       (*((volatile uint32_t *)0x40025008))
#define PF2       (*((volatile uint32_t *)0x40025010))
#define PF3       (*((volatile uint32_t *)0x40025020))
#define LEDS      (*((volatile uint32_t *)0x40025038))
#define RED       0x02
#define BLUE      0x04
#define GREEN     0x08
#define WHEELSIZE 8           // must be an integer multiple of 2
                              //    red, yellow,    green, light blue, blue, purple,   white,          dark
const int32_t COLORWHEEL[WHEELSIZE] = {RED, RED+GREEN, GREEN, GREEN+BLUE, BLUE, BLUE+RED, RED+GREEN+BLUE, 0};

//Some User Tasks
void dummy2(void){}
void Lights(void){
  static int i = 0;
  LEDS = COLORWHEEL[i&(WHEELSIZE-1)];
  i = i + 1;
}
uint32_t Buffer[200]; // Buffer to store data
//---------------------OutCRLF---------------------
// Output a CR,LF to UART to go to a new line
// Input: none
// Output: none
void OutLine1(){
  UART_OutChar(CR);
  UART_OutChar(LF);
}

#define JITTERSIZE 64
extern unsigned long MaxJitter;
extern unsigned long JitterHistogram[64];
extern long x[64];

unsigned long time;		 
/*
long FIRFilter(unsigned long data){
	static long XX[102];	// ADC samples
	static long Y[102];	// Convolution
	static unsigned long n=51; // keeps track of samples
	unsigned int i;
	n++;
  if(n == 102) n=51;
	XX[n] = XX[n-51] = (long) data;
	Y[n] = 0;
	for(i = 0; i < 51; i++){ 
		Y[n] += h[n-i]*XX[n-i];
	}
	Y[n-51] = Y[n];
	return Y[n];
}	


void FIRFilter2(unsigned long data){
	static long XX[102];	// ADC samples
	static long Y[102];	// Convolution
	static unsigned long n=51; // keeps track of samples
	unsigned int i;
	n++;
  if(n == 102) n=51;
	XX[n] = XX[n-51] = (long) data;
	Y[n] = 0;
	for(i = 0; i < 51; i++){ 
		Y[n] += h[n-i]*XX[n-i];
	}
	Y[n-51] = Y[n];
	//return Y[n];
}*/

void Interpreter(){
	char cmd1[30], cmd2[30], cmd3[30];
	char* cd1, *cd2, *cd3;
	uint32_t i,j,k;
	//MaxJitter2 = 5;
	printf("*************EE445M Interpreter**********");
	OutLine1();
	printf("Made by: Max and Dat");
	OutLine1();
	cd1 = cmd1;
	cd2 = cmd2;
	cd3 = cmd3;
	while(1){
		OutLine1();
		printf("Existing Commands: ");
		OutLine1();
	//	printf("ADC_Timer,channelnum,freq");
	//	OutLine1();
	//	printf("ADC_Soft,channelnum,freq");
	//	OutLine1();
		printf("Format");
		OutLine1();
		printf("Directory");
		OutLine1();
		printf("PrintFile");
		OutLine1();
		printf("DeleteFile");
		OutLine1();
		printf("ADC_Print");
		OutLine1();
		printf("FFT_Print");
		OutLine1();
		printf("Filter_Set,0(off) or 1(on)");
		OutLine1();
		printf("Display_Set,0(Volt vs. Freq.) or 1(Volt vs. Time)");
		OutLine1();
		printf("MaxJitter");
		OutLine1();
		printf("JitterHistogram");
		OutLine1();
		printf("LCD_Print,device,line,number,message");
		OutLine1();
		printf("Set_Thread,Task,period,priority");
		OutLine1();
		printf("Current_Period");
		OutLine1();
		printf ("cmd# ");
		scanf("%s", cd1);
		OutLine1();
		printf("Entered: %s", cd1);
		OutLine1();
		cd2 = strtok(cd1, ",");
		printf("Command Entered: %s", cd2);
		OutLine1();

		if(strcmp("Filter_Set", cd2) == 0){
			cd2 = strtok(NULL, ",");
			Fset = atoi(cd2);
		}
		else if(strcmp("Display_Set", cd2) == 0){
			cd2 = strtok(NULL, ",");
			Dset = atoi(cd2);
		}
		else if (strcmp("ADC_Print", cd2) == 0){
			int m;
			for (m=0; m < 102; m++){
				printf ("ADC_Timer Input: %lu", ADCarray[m]);
				OutLine1();
			}
		}
		else if (strcmp("Format", cd2) == 0){
			if(eFile_Format())
				printf("Successful Formatting");
			else
				printf("Formatting Unsuccessful");
			OutLine1();
		}
		else if (strcmp("Directory", cd2) == 0){
		// print the directory
		}
		else if (strcmp("PrintFile", cd2) == 0){
			cd2=strtok(NULL,",");
		// eFile_Print(cd2);
		}
		else if (strcmp("DeleteFile", cd2) == 0){
			cd2=strtok(NULL,",");
			eFile_Delete(cd2);
		}
		/*else if (strcmp("FIR_Print", cd2) == 0){
			int m;
			for (m=0; m < 51; m++){
				printf ("FIR output: %ld", Y[m]);
				OutLine1();
			}
		}*/
		else if (strcmp("FFT_Print", cd2) == 0){
			int m;
			for (m=0; m < 64; m++){
				printf ("FFT output: %ld", FFTOutput[m]);
				OutLine1();
			}
		}
		else if (strcmp("JitterHistogram", cd2) == 0){
			int i;
			for (i = 0; i<64; i++){
				printf("JitterHistogram[%d] = %lu", i, JitterHistogram[i]);
				OutLine1();
			}
		}
			if (strcmp("MaxJitter",cd2) == 0){
			printf("MaxJitter = %lu", MaxJitter);
		}
		else if(strcmp("LCD_Print", cd2) == 0){ //print a line to the LCD
			cd2 = strtok(NULL, ",");
			i = atoi(cd2);
			cd2 = strtok(NULL, ",");
			j = atoi(cd2);
			cd2 = strtok(NULL, ",");
			k = atoi(cd2);
			cd2 = strtok(NULL, ",");
			ST7735_Message(i, j, cd2, k);
		}
		else if(strcmp("Current_Period", cd2)==0){// Returns the current period of a thread
			printf("Current Period: %lu", OS_ReadPeriodicTime());
			OutLine1();
		}
		else if(strcmp("Set_Thread", cd2)==0){ // Initializes a thread to execute
			cd3 = strtok(NULL, ",");
			cd2 = strtok(NULL, ",");
			i = atoi(cd2);
			cd2 = strtok(NULL, ",");
			j = atoi(cd2);
			if(strcmp(cd3,"Lights")==0)
				OS_AddPeriodicThread(&Lights,i,j);
			else if(strcmp(cd3,"dummy")==0)
				OS_AddPeriodicThread(&dummy2,i,j);
			printf("Current Period: %lu", OS_ReadPeriodicTime());
			OutLine1();
		}
	}
}
