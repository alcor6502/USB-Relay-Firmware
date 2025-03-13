/*
 * 	   File: cmd_parser.c
 *
 *  Created: Dec 8, 2024
 *   Author: Alfredo Cortellini
 *  License: https://creativecommons.org/licenses/by-nc/4.0/
 *
 */
/*
	Command structure:
	"#"	Command prefix
	"!"	Command suffix
	"S"	Switch command - accepted values 1 (on) or 0 (off) Example: - "#S1!" (Switch on the relay)
	"P"	Pulse command - accepted values 1 to 9 (100 msec multiples) - Example: "#P3!" (Switch on the relay for 300 msec)
	"F" Fan control command - accepted values 0 to 100 - Example: "#F90!" (Fan on at 90% PWM)
	"W" PWM control command (B4 or B5 Closed) - accepted values 0 to 100 - Example: "#F90!" (Fan on at 90% PWM)
	"I"	Input line read - Example: "#I!" (Return the status of the input)
	"T"	Tach line read - Example: "#T!" (Return the rotation per minute of the fan)
	"O"	Duty cycle PWM read - Example: "#O!" (Return the PWM value of the output)
	"Z"	Output PWM Frequency - accepted values 10 to 48000 - Example: '#F90!' (Fan on at 90% PWM)
	"A"	Servo mode - accepted values 0 to 200 - Example: "#A75!" servo at 0 Degree

	### How to use on Linux System in Bash:
    	To send a command
    		echo -e '#S1!' > /dev/serial/by-id/usb-IT_Logic_USB_Relay-if00

    	To read a result and store in a variable FAN_TACH:
    	    echo -e '#T!' > /dev/serial/by-id/usb-IT_Logic_USB_Relay-if00
			read -d'~' -t1 FAN_TACH < /dev/serial/by-id/usb-IT_Logic_USB_Relay-if00
			echo $FAN_TACH

	### How to use on Mac OS in Bash:
    	To send a command
    		echo -e '#S1!' > /dev/cu.usbmodem8301

    	To read a result and store in a variable FAN_TACH:
    	    echo -e '#T!' > /dev/cu.usbmodem8301
			read -d'~' -t1 FAN_TACH < /dev/cu.usbmodem8301
			echo $FAN_TACH

	### How to use on Windows in Shell:
	    To send a command
    		echo -e '#S1!' > /dev/cu.usbmodem8301

    	To read a result and store in a variable FAN_TACH:
    	    echo -e '#T!' > /dev/cu.usbmodem8301
			read -d'~' -t1 FAN_TACH < /dev/cu.usbmodem8301
			echo $FAN_TACH

*/

/*
Modified files:

USB_DEVICE/App/usbd_cdc_if.c
	  CmdExtractCmd(&Buf[0], Len);	// Function to extract the commands to the device

USB_DEVICE/App/usbd_desc.c
	*length = 2; // *** Kill the serial Number
*/

#include <cmd_process.h>

static cmd_Parser_Data_Def cmdData;

void SetupTIM2() {							// Manually Configure TIM2 CH2 in PWM Mode
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;		// Enable clock access to TIM2
	TIM2->CR1 &= ~TIM_CR1_CEN;				// Ensure TIM2 is disabled
	TIM2->CR1 &= ~TIM_CR1_DIR;				// Set counting direction
	TIM2->PSC = TRIGGER_TIMER_PSC - 1;		// Set prescaler
	TIM2->ARR = TRIGGER_TIMER_ARR - 1;		// Set Auto Reload Value, sets the PRI
	TIM2->CCR1 = 0;							// Set output compare register for channel 1
	TIM2->CCMR1 &= ~TIM_CCMR1_CC1S_Msk; 	// Clear bits for CC1S
	TIM2->CCMR1 &= ~TIM_CCMR1_OC1M_Msk; 	// Clear bits for OC1M
	TIM2->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2); // Select PWM mode for channel 1 - 110 - PWM mode 1
	TIM2->CCMR1 |= TIM_CCMR1_OC1PE;			// Enable register preload
	TIM2->CCER &= ~TIM_CCER_CC1P;			// Select output polarity to 0 - active high
	TIM2->CCER |= TIM_CCER_CC1E;			// Enable output of channel 1
	TIM2->CR1 |= TIM_CR1_CEN;				// Enable TIM2
}


void CmdParserInit() {
	for (int i = 0; i < CMD_BUF_SIZE; i++){
		cmdData.cmdBuf[i] = 0x00;
	}
	cmdData.lenCmdBuf = 0;
	for (int i = 0; i < TX_BUF_SIZE; i++){
		cmdData.txBuf[i] = 0x00;
	}
	cmdData.lenTxBuf = 0;
	cmdData.sendTxBuf = 0;
	cmdData.sendTxBufStart = 0;
	cmdData.pulseOn = 0;
	cmdData.pulseStop = 0;
	cmdData.buttonState = RELEASED;
	cmdData.debounceStop = 0;
	cmdData.rpmPluses = 0;
	cmdData.rpmMinute = 0;
	cmdData.rpmNextCalc = HAL_GetTick() + RPM_INTEGRATION_TM;
	cmdData.lastOnState = 100;
}


static uint16_t AsciiToNumber(uint8_t *alfa, uint8_t len) {

	uint8_t digit;
	uint32_t number = 0;

	if (alfa == NULL || len < 1) {			// Check for invalid input (NULL or empty string)
		return 0;
	}

	for (int i = 0; i < len; i++) {			// Ensure the character is a valid digit
		digit = alfa[i];
		if (digit< '0' || digit > '9') {
			return 0;
		}
		number = number * 10 + (digit - '0');
	}
	if (number > 65535) {
		number = 65535;
	}
	return (uint16_t)number;
}


static void NumberToAscii(uint32_t number) {
	uint8_t i = 0;
	uint8_t j;
	uint8_t temp;


    if (number == 0) {
    	cmdData.txBuf[i++] = '0';	// Handle zero case
    	cmdData.lenTxBuf = 1;
    }
    else {
		// Extract digits in reverse order
		while (number > 0) {
			cmdData.txBuf[i++] = (number % 10) + '0';  // Convert digit to ASCII
			number /= 10;
		}
		cmdData.lenTxBuf = i;
    }
    // Reverse the array to make it the correct order
    for (j = 0; j < i / 2; j++) {
    	temp = cmdData.txBuf[j];
        cmdData.txBuf[j] = cmdData.txBuf[i - j - 1];
        cmdData.txBuf[i - j - 1] = temp;
    }
	cmdData.txBuf[cmdData.lenTxBuf] = TX_DELIM;
	cmdData.lenTxBuf++;
	cmdData.sendTxBufStart = HAL_GetTick() + TX_DELAY;
	cmdData.sendTxBuf = 1;
}


void TachIncrement() {
	cmdData.rpmPluses++;
}


void UpdateLeds() {				// Update the status led depending on coil
	if (TIM2->CCR1 > 0 ) {		// Check if the PWM value is 1 (On state)
		  HAL_GPIO_WritePin(LED_OFF_GPIO_Port, LED_OFF_Pin,GPIO_PIN_RESET);
		  HAL_GPIO_WritePin(LED_ON_GPIO_Port, LED_ON_Pin,GPIO_PIN_SET);
	}
	else {
		  HAL_GPIO_WritePin(LED_OFF_GPIO_Port, LED_OFF_Pin,GPIO_PIN_SET);
		  HAL_GPIO_WritePin(LED_ON_GPIO_Port, LED_ON_Pin,GPIO_PIN_RESET);
	}
}


void CheckButton() {
	if ((HAL_GPIO_ReadPin(MAN_SW_GPIO_Port, MAN_SW_Pin) == GPIO_PIN_SET)  && (cmdData.buttonState == RELEASED)) {
		if (TIM2->CCR1 > 0 ) {					// Check if the PWM value is 1 (On state)
			cmdData.lastOnState = TIM2->CCR1;	// Save the PWM value in case the button is pressed again
			TIM2->CCR1 = 0;						// Switch relay to Off
		}
		else {
			TIM2->CCR1 = cmdData.lastOnState;	// Switch relay to ON
		}
		cmdData.buttonState = PRESS_START;
		cmdData.debounceStop  = HAL_GetTick() + DEBOUNCE_TIME;
	}
	if ((HAL_GPIO_ReadPin(MAN_SW_GPIO_Port, MAN_SW_Pin) == GPIO_PIN_RESET)  && (cmdData.buttonState == PRESSED)) {
		cmdData.buttonState = RELEASE_START;
		cmdData.debounceStop  = HAL_GetTick() + DEBOUNCE_TIME;
	}
}


void CheckDelays() {
	if ((cmdData.buttonState == PRESS_START) && (HAL_GetTick() >= cmdData.debounceStop)){
		cmdData.buttonState = PRESSED;
	}
	if ((cmdData.buttonState == RELEASE_START) && (HAL_GetTick() >= cmdData.debounceStop)){
		cmdData.buttonState = RELEASED;
	}
	if ((cmdData.pulseOn == 1) && (HAL_GetTick() >= cmdData.pulseStop)){
		cmdData.pulseOn = 0;
		TIM2->CCR1 = 0;				// Switch relay to Off
	}
	if ((cmdData.sendTxBuf == 1) && (HAL_GetTick() >= cmdData.sendTxBufStart)){
		cmdData.sendTxBuf = 0;
		CDC_Transmit_FS(cmdData.txBuf, cmdData.lenTxBuf);				// Transmit the string
	}
	if (HAL_GetTick() >= cmdData.rpmNextCalc){
		cmdData.rpmMinute = cmdData.rpmPluses * ((10000 / RPM_INTEGRATION_TM) * (6 / RPM_PULSE_REV));
		cmdData.rpmNextCalc = HAL_GetTick() + RPM_INTEGRATION_TM;
		cmdData.rpmPluses = 0;
	}
}


static void CmdParse() {
	static uint8_t servoMode;				// Flag to notify TIM2 is in servo mode
	uint16_t asciiValue = 0;				// Contain the converted value of the Ascii number


	if ((servoMode == 1) && (
		(cmdData.cmdBuf[0] == 'S') ||
		(cmdData.cmdBuf[0] == 'P') ||
		(cmdData.cmdBuf[0] == 'F') ||
		(cmdData.cmdBuf[0] == 'W') ||
		(cmdData.cmdBuf[0] == 'Z'))) {			// These commands trigger TIM2 to default values exiting servo mode
		TIM2->CCR1 = 0;							// Disable output
		TIM2->CR1 &= ~TIM_CR1_CEN;				// Ensure TIM2 is disabled
		TIM2->CNT = 0;							// Reset counter
		TIM2->SR = 0;							// Clear status register
		TIM2->PSC = TRIGGER_TIMER_PSC - 1;		// Set prescaler
		TIM2->ARR = TRIGGER_TIMER_ARR - 1;		// Set Auto Reload Value, sets the PRI
		TIM2->CCER &= ~TIM_CCER_CC1P;			// Select output polarity to 0 - active high
		TIM2->CR1 |= TIM_CR1_CEN;				// Enable TIM2
		servoMode = 0;
	}

	if ( cmdData.lenCmdBuf > 1) {	// There are numbers to be converted
		asciiValue = AsciiToNumber(&cmdData.cmdBuf[1], cmdData.lenCmdBuf - 1);
	}

	switch (cmdData.cmdBuf[0]) {

	case 'S':	// *****  "S" Command  *****
		if (asciiValue > 1) {
			asciiValue = 1;
		}
		TIM2->CCER &= ~TIM_CCER_CC1P;			// Select output polarity to 0 - active high
		TIM2->CCR1 = asciiValue * 100;
		break;


	case 'P':	// *****  "P" Command  *****
		if (asciiValue > 36000) {
			asciiValue = 36000;
		}
		if (asciiValue < 1) {
			asciiValue = 1;
		}
		TIM2->CCER &= ~TIM_CCER_CC1P;			// Select output polarity to 0 - active high
		TIM2->CCR1 = 100;		// Switch relay to ON
		cmdData.pulseOn = 1;
		cmdData.pulseStop  = HAL_GetTick() + (asciiValue * 100);	// Set the duration of the pulse
		break;


	case 'F':	// *****  "F" Command  *****
		if (asciiValue > 100) {
			asciiValue = 100;
		}
		TIM2->CCER |= TIM_CCER_CC1P;			// Select output polarity to 1 - active low
		TIM2->CCR1 = asciiValue;
		break;


	case 'W':	// *****  "W" Command  *****
		if (asciiValue > 100) {
			asciiValue = 100;
		}
		TIM2->CCER &= ~TIM_CCER_CC1P;			// Select output polarity to 0 - active high
		TIM2->CCR1 = asciiValue;
		break;


	case 'I':	// *****  "I" Command  *****
		NumberToAscii(HAL_GPIO_ReadPin(TACH_GPIO_Port, TACH_Pin));
		break;


	case 'T':	// *****  "T" Command  *****
		NumberToAscii(cmdData.rpmMinute);
		break;


	case 'O':	// *****  "O" Command  *****
		NumberToAscii((uint32_t)(TIM2->CCR1));
		break;

	case 'Z':	// *****  "Z" Command  *****
		if (asciiValue > 50000) {
			asciiValue = 50000;
		}
		if (asciiValue < 10) {
			asciiValue = 10;
		}
		TIM2->CR1 &= ~TIM_CR1_CEN;				// Ensure TIM2 is disabled
		TIM2->PSC = (480000 / asciiValue) - 1;
		TIM2->CR1 |= TIM_CR1_CEN;				// Enable TIM2
		break;

	case 'A':	// *****  "A" Command  *****
		if (servoMode == 0)  {
			TIM2->CCR1 = 0;							// Disable output
			TIM2->CR1 &= ~TIM_CR1_CEN;				// Ensure TIM2 is disabled
			TIM2->CNT = 0;							// Reset counter
			TIM2->SR = 0;							// Clear status register
			TIM2->PSC = 960 - 1;					// 50 Hz (20ms) frequency
			TIM2->ARR = 1000 -1;					// 1.000 PWM resolution
			TIM2->CCER |= TIM_CCER_CC1P;			// Select output polarity to 1 - active low
			TIM2->CR1 |= TIM_CR1_CEN;				// Enable TIM2
			servoMode = 1;							// Force reset TM2 when exit from servo mode
		}
		if (asciiValue > 200) {
			asciiValue = 200;
		}
		TIM2->CCR1 = asciiValue;			// The servo goes from 0 to 200
		break;

    default:
        break;
	}
}


void CmdExtractCmd(uint8_t *rxBufUSB, uint32_t *lenRxBufUSB) {

	static uint8_t cmdFound;

	for (int i = 0; i < *lenRxBufUSB; i++) {

		if ((rxBufUSB[i] == (CMD_END)) && (cmdFound == 1)) {
			CmdParse();
			cmdFound = 0;
		}
		if (cmdData.lenCmdBuf == CMD_BUF_SIZE) {
			cmdFound = 0;
			cmdData.lenCmdBuf = 0;
		}
		if (cmdFound == 1) {
			cmdData.cmdBuf[cmdData.lenCmdBuf] = rxBufUSB[i];
			cmdData.lenCmdBuf++;
		}
		if (rxBufUSB[i] == (CMD_START)) {
			cmdData.lenCmdBuf = 0;
			cmdFound = 1;
		}
	}
}
