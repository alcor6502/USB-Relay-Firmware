# STM32F042 Firmware for a USB Controlled Relay

## Follow the project on [Hackaday](https://hackaday.io/project/202496-usb-relay-switch)

The software is develop using STMCube IDE
The firmware follows a real-time event-driven model:
- Main Loop: Continuously listens for USB commands & button presses.
- Interrupt Handlers: Detect tachometer pulses for accurate RPM tracking.
- Non-Blocking Execution: Avoids unnecessary delays while handling PWM & I/O control.
