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
#include "OS.h"
#include "efile.h"
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

unsigned long time;		 

void Interpreter(){
	char cmd1[30], cmd2[30];
	char* cd1, *cd2;
	//MaxJitter2 = 5;
	printf("*************EE445M Interpreter**********");
	OutLine1();
	printf("Made by: Max and Dat");
	OutLine1();
	cd1 = cmd1;
	cd2 = cmd2;
	while(1){
		OutLine1();
		printf("Existing Commands: ");
		OutLine1();
		printf("Format");
		OutLine1();
		printf("Directory");
		OutLine1();
		printf("PrintFile,File");
		OutLine1();
		printf("DeleteFile,File");
		OutLine1();
		printf ("cmd# ");
		scanf("%s", cd1);
		OutLine1();
		printf("Entered: %s", cd1);
		OutLine1();
		cd2 = strtok(cd1, ",");
		printf("Command Entered: %s", cd2);
		OutLine1();
		if (strcmp("Format", cd2) == 0){
			if(eFile_Format())
				printf("Successful Formatting");
			else
				printf("Formatting Unsuccessful");
			OutLine1();
		}
		else if (strcmp("Directory", cd2) == 0){
			if(!eFile_Directory(&UART_OutChar)){
				OutLine1();
			}
		}
		else if (strcmp("PrintFile", cd2) == 0){
			cd2=strtok(NULL,",");
			if(!eFile_ROpen(cd2)){
				while(!eFile_ReadNext(cd2)){
					printf(cd2);
				}
				OutLine1();
			}
		}
		else if (strcmp("DeleteFile", cd2) == 0){
			cd2=strtok(NULL,",");
			eFile_Delete(cd2);
		}
	}
}
