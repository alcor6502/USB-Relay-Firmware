/*
 * 	   File: cmd_parser.h
 *
 *  Created: Dec 8, 2024
 *   Author: Alfredo Cortellini
 *  License: https://creativecommons.org/licenses/by-nc/4.0/
 *
 */

#ifndef INC_CMD_PROCESS_H_
#define INC_CMD_PROCESS_H_

#include "main.h"

#define CMD_BUF_SIZE 10		// Size of command buffer
#define TX_BUF_SIZE 10 		// Size of the output buffer
#define CMD_START '#'		// Symbol to identify the beginning of a command
#define CMD_END '!'			// Symbol to identify the end of a command
#define TX_DELIM '~'		// Symbol to identify the end of the Tx message
#define TX_DELAY 10			// Delay in msec between receiving the command and the output of the result

// TIM2 CH2 PWM Parameters
#define TRIGGER_TIMER_PSC 20	// Frequency of PWM signal 24 kHz (need to be further divided by duty cycle)
#define TRIGGER_TIMER_ARR 100	// Resolution of the PWM duty Cycle
#define DEBOUNCE_TIME 50		// Debounce time in msec
#define RPM_INTEGRATION_TM 1000	// Integration time to calculate rpm per minute
#define RPM_PULSE_REV 2			// Pulses per revolution

typedef enum {
    RELEASED,
    PRESS_START,
    PRESSED,
	RELEASE_START,
} ButtonCtrl;

typedef struct
{
	uint8_t		cmdBuf[CMD_BUF_SIZE];
	uint8_t		lenCmdBuf;
	uint8_t		txBuf[TX_BUF_SIZE];
	uint8_t		lenTxBuf;
	uint8_t		sendTxBuf;
	uint32_t	sendTxBufStart;
	uint8_t		pulseOn;
	uint32_t	pulseStop;
	ButtonCtrl	buttonState;
	uint32_t	debounceStop;
	uint32_t	rpmPluses;
	uint16_t	rpmMinute;
	uint32_t	rpmNextCalc;
	uint16_t	lastOnState;
} cmd_Parser_Data_Def;



extern uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);

void SetupTIM2();
void TachIncrement();
void UpdateLeds();
void CheckButton();
void CheckDelays();
void CmdParserInit();
void CmdExtractCmd(uint8_t *rxBufUSB, uint32_t *lenRxBufUSB);

#endif /* INC_CMD_PROCESS_H_ */
