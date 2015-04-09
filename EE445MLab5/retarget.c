/*----------------------------------------------------------------------------
 * Name:    Retarget.c
 * Purpose: 'Retarget' layer for target-dependent low level functions
 * Note(s):
 *----------------------------------------------------------------------------
 * This file is part of the uVision/ARM development tools.
 * This software may only be used under the terms of a valid, current,
 * end user licence from KEIL for a compatible version of KEIL software
 * development tools. Nothing else gives you the right to use this software.
 *
 * This software is supplied "AS IS" without warranties of any kind.
 *
 * Copyright (c) 2011 Keil - An ARM Company. All rights reserved.
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <rt_misc.h>
#include <stdint.h>
#include "UART.h"
#include "tm4c123gh6pm.h"

#pragma import(__use_no_semihosting_swi)



struct __FILE { int handle; /* Add whatever you need here */ };
FILE __stdout;
FILE __stdin;


#include "efile.h"
extern int StreamToFile;
int fputc (int ch, FILE *f) {
 if(StreamToFile){
 if(eFile_Write(ch)){ // close file on error
 eFile_EndRedirectToFile(); // cannot write to file
 return 1; // failure
 }
 return 0; // success writing
 }
 // regular UART output
 UART_OutChar(ch);
 return 0;
}

int fgetc (FILE *f){
 return (UART_InChar());
}

/*
int fputc(int c, FILE *f) {
  UART_OutChar(c);
	return 1;
}


int fgetc(FILE *f) {
	char input;
  input = (UART_InChar());
	UART_OutChar(input);
	return input;
}
*/

int ferror(FILE *f) {
  /* Your implementation of ferror */
  return EOF;
}

/*
void _ttywrch(int c) {
  SER_PutChar(c);
}
*/

void _sys_exit(int return_code) {
label:  goto label;  /* endless loop */
}
