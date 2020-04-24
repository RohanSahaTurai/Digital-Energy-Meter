# Digital-Energy-Meter
The firmware of a Digital Energy Meter built on the TWR-ADCDAC-LTC tower module running Kinetis K70 MCU.

• DEM samples real-time current and voltage inputs, and calculates the average power, total energy consumed, total cost, RMS voltage and current, and frequency of the input signals.

• External ADC, which interfaces with the MCU through SPI, samples analog voltage and current. Frequency is tracked and sample rate is adjusted in real-time to maintain 16 samples per second.

• Uses UART to communicate to a PC through a PC User Interface software module. Tower Serial Communication protocol is implemented to interrogate and set different settings in the DEM.

• Real time clock is used to implement variable tariff based on the time of the day. Self-testing mode is available that simulates the measurements in a time accelerated environment. Tariff could also be set via the PC.

• Uses Human Machine Interface to cycle through different states of FSM to display the measured and calculated values in real-time

• Due to the lack of FPU unit, calculations are performed in 32Q16 fixed-point notation for faster processing.

• Uses multithreaded, pre-emptive RTOS that implements thread priorities to achieve real-time measurements and decrease interrupt latency. Thread synchronization and communication is achieved using semaphores, mutex lock, protecting critical sections and appropriate thread priorities.
