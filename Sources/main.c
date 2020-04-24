/* ###################################################################
 **     Filename    : main.c
 **     Project     : Project
 **     Processor   : MK70FN1M0VMJ12
 **     Version     : Driver 01.01
 **     Compiler    : GNU C Compiler
 **     Date/Time   : 2015-07-20, 13:27, # CodeGen: 0
 **     Abstract    :
 **         Main module.
 **         This module contains user's application code.
 **     Settings    :
 **     Contents    :
 **         No public methods
 **
 ** ###################################################################*/
/*!
 ** @file main.c
 ** @version 6.0
 ** @brief
 **         Main module.
 **         This module contains user's application code.
 */
/*!
 **  @addtogroup main_module main module documentation
 **  @{
 */
/* MODULE main */

#include "Cpu.h"
#include "Events.h"
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"
#include "OS.h"     // RTOS Functionality
#include "packet.h" // Packet Module - contains packet initialization, get and put routines
#include "Flash.h"  // Flash Module - contains routines to handle flash memory operations
#include "LEDs.h"   // LED Module - controls turning on and off of LEDs
#include "PIT.h"    // PIT Module - controls the Periodic Timer interrupt module
#include "FTM.h"    // FTM Module - controls the Flexible Timer Module
#include "RTC.h"    // RTC Module - controls the Real Time Clock module
#include "analog.h" // Analog Module - analog functions
#include "Calc.h"   // Calculations - calculations for DEM
#include "HMI.h"    // HMI - Human Machine Interaction
#include "Switch.h"
#include "FixedPoint.h"

/* Function Prototype */
void FTM0Callback (const TFTMChannel* const aFTMChannel);
void HandlePackets();

/***********************************************************************************************************
 * Global Variables and constants
 ************************************************************************************************************/
#define ANALOG_WINDOW_SIZE 16

const uint32_t BAUD_RATE = 115200;              /*! The Baud Rate to be set to communicate with the PC */

const uint32_t MAX_SAMPLE_PERIOD = 1315790;      /*! The sample rate for the analog input in nanoseconds */

const uint8_t PACKET_ACK_MASK = 0x80;           /*! Acknowledgment bit mask */

volatile uint16union_t *NvTariffMode;           /*! Non-volatile Tower Number */
const uint16_t DEFAULT_TARIFF_MODE = 1;         /*! Initial constant Tower Number */

volatile bool TestModeEnabled = false;

uint32_t TimeUsage = 0;

const uint8_t VOLTAGE_CHANNEL_NB = 0;
const uint8_t CURRENT_CHANNEL_NB = 1;

static uint8_t HMITimeoutCounter = 0;

int16_t Voltage_ADC [ANALOG_WINDOW_SIZE];

int16_t Current_ADC [ANALOG_WINDOW_SIZE];

/***********************************************************************************************************
 * Packet Handler Commands
 ************************************************************************************************************/
#define CMD_TESTMODE       0x10    /*!< Command for Flash - Read Byte */
#define CMD_TARIFF         0x11    /*!< Command for Special - Get Startup Values */
#define CMD_TIME1          0x12    /*!< Command for Special - Tower Version */
#define CMD_TIME2          0X13    /*!< Command for Tower Number */
#define CMD_POWER          0x14    /*!< Command for Tower Mode */
#define CMD_ENERGY         0x15    /*!< Command for Flash - Read Byte */
#define CMD_COST           0x16    /*!< Command for Flash - Read Byte */
#define CMD_FREQUENCY      0x17    /*!< Command for Flash - Read Byte */
#define CMD_VOLTAGE_RMS    0x18    /*!< Command for Flash - Read Byte */
#define CMD_CURRENT_RMS    0x19    /*!< Command for Flash - Read Byte */
#define CMD_POWER_FACTOR   0x1A    /*!< Command for Flash - Read Byte */

// ----------------------------------------
// Thread set up
// ----------------------------------------
// Arbitrary thread stack size - big enough for stacking of interrupts and OS use.
#define THREAD_STACK_SIZE 5000
#define NB_ANALOG_CHANNELS 2

/***********************************************************************************************************
 * Thread Stacks
 ************************************************************************************************************/
static uint32_t RTCThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08)));
static uint32_t PacketReceiveThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08)));  /*! The stack for packet checker thread */

/***********************************************************************************************************
 * Thread Priorities
 *  0 = highest priority
 ************************************************************************************************************/
const uint8_t RECEIVE_THREAD_PRIORITY       = 0;
const uint8_t TRANSMIT_THREAD_PRIORITY      = 1;
const uint8_t CALCULATION_THREAD_PRIORITY   = 2;
const uint8_t RTC_THREAD_PRIORITY           = 3;
const uint8_t PACKETRECEIVE_THREAD_PRIORITY = 4;

/***********************************************************************************************************
 * Global Semaphores
 ************************************************************************************************************/
OS_ECB *RTC_Semaphore;          /*! Binary Semaphore for updating the RTC clock */
OS_ECB *AnalogGetSemaphore;

/***********************************************************************************************************
 * Data Structures and Configurations
 ************************************************************************************************************/
TFTMChannel FTMChannel =
    {
      0,                                // channel number 0
      CPU_MCGFF_CLK_HZ_CONFIG_0,        // delay count of 15s, therefore, clk period = module clk speed * 15
      TIMER_FUNCTION_OUTPUT_COMPARE,    // configure channel 0 as output compare
      TIMER_OUTPUT_LOW,                 // Output Action Low; turn off LED
      (void* ) &FTM0Callback,           // The callback function for channel 0
      &FTMChannel,                      // Send the channel configuration as argument
    };

TState FSMState [NB_DISPLAY_STATES] =
{
  {
    .stateFunction = NULL,    // No function exists for dormant state
    .nextState = &FSMState[1]
  },
  {
    .stateFunction = (void*) &HMI_TimeState,
    .nextState = &FSMState[2]
  },
  {
    .stateFunction = (void*) &HMI_PowerState,
    .nextState = &FSMState[3]
  },
  {
    .stateFunction = (void*) &HMI_EngergyState,
    .nextState = &FSMState[4]
  },
  {
    .stateFunction = (void*) &HMI_CostState,
    .nextState = &FSMState[1]
  }
};


/***********************************************************************************************************
 * Callback functions
 ************************************************************************************************************/
/*! @brief PIT callback function for PIT_ISR
 *
 *  Toggles the Green LED after timeout has occured
 */
void PITCallback (void* arg)
{
  OS_ERROR error;

  static uint8_t NbSamples = 0;

  Analog_Get(VOLTAGE_CHANNEL_NB, Voltage_ADC + NbSamples);

  Analog_Get(CURRENT_CHANNEL_NB, Current_ADC + NbSamples);

  // Increment the window index
  NbSamples++;

  // Wrap index
  if (NbSamples == ANALOG_WINDOW_SIZE)
    NbSamples = 0;

  error = OS_SemaphoreSignal(AnalogGetSemaphore);

  if (error)
    PE_DEBUGHALT();
}

/*! @brief FTM0 callback function for FTM_ISR
 *
 *  @param aFTMChannel is a structure containing the parameters to be used in setting up the timer channel.
 *
 *  Turns off the Blue LED after the timeout has occurred
 */
void FTM0Callback (const TFTMChannel* const aFTMChannel)
{
  HMITimeoutCounter++;

  if (HMITimeoutCounter == 15)
  {
    // Check if the channel is set up for output compare
    if (aFTMChannel->timerFunction == TIMER_FUNCTION_OUTPUT_COMPARE)
      // Do the desired action as requested for the channel
      switch (aFTMChannel->ioType.outputAction)
      {
        // Set the dormant flag to return the HMI to dormant state
        case TIMER_OUTPUT_LOW:
          //Turn off the blue LED
          LEDs_Off(LED_BLUE);
          // reset the HMI to the dormant state
          HMI_UpdateState(FSMState, true);
          break;
      }
    HMITimeoutCounter = 0;
  }

  else
    FTM_StartTimer(&FTMChannel);

}

void SwitchCallback (void *arg)
{
  //Turn on Blue LED
  LEDs_On(LED_BLUE);

  // Move to the next cycle
  HMI_UpdateState (FSMState, false);

  //Start the FTM Timer
  FTM_StartTimer(&FTMChannel);

  HMITimeoutCounter = 0;
}


/***********************************************************************************************************
 * Threads
 ************************************************************************************************************/
/*! @brief Thread that looks after interrupts made by RTC
 *
 *  Gets the current time from RTC and sends it to the PC
 *
 *  @param pData Thread Parameter
 */
void RTCThread (void *pData)
{
  OS_ERROR error;

  for (;;)
  {
    // Wait for the semaphore to be signaled by the RTC ISR
    error = OS_SemaphoreWait(RTC_Semaphore, 0);

    if (error)
         PE_DEBUGHALT();

    // Update the HMI display every second
    HMI_DisplayCurrentState();

    // Increment the time of usage
    if (TestModeEnabled)
    {
      TimeUsage += 3600;

      uint8_t days, hours, minutes, seconds;

      RTC_Get (&days, &hours, &minutes, &seconds);

      // Increment the hours by 1 and decrement the seconds by 1
      hours ++;
      seconds--;

      // Set the accelerated time
      RTC_Set(days, hours, minutes, seconds);
    }

    else
      TimeUsage++;

    // Toggle the yellow LED
    LEDs_Toggle(LED_YELLOW);
  }
}


/*! @brief Checks if a valid packet is received, turns on Blue LED and and calls HandlePackets()
 *
 *  @param pData is not used but is required by the OS to create a thread.
 */
static void PacketReceiveThread(void* pData)
{
  for(;;)
  {
    // Check if a valid packet is received
    if (Packet_Get())
      //Respond to the incoming packets from the PC
      HandlePackets();
  }
}

/***********************************************************************************************************
 * Individual Packet Handler Routines
 ************************************************************************************************************/

/*! @brief Responds to the Tower Startup packet
 *
 *  @return bool - TRUE if the packet was handle successfully
 */
bool HandleTestModePacket()
{
  if (Packet_Parameter3 == 0)
    return Packet_Put (CMD_TESTMODE, (uint8_t) TestModeEnabled, 0, 0);

  else if (Packet_Parameter3 == 1)
    if (Packet_Parameter1 == 0 || Packet_Parameter1 == 1)
    {
      TestModeEnabled = (bool) Packet_Parameter1;
      return true;
    }

  return false;
}

/*! @brief Responds to the Tower Number packet
 *
 *  @return bool - TRUE if the the packets were sent successfully
 */
bool HandleTariffPacket()
{
  if (Packet_Parameter3 == 0)
    return Packet_Put (CMD_TARIFF, NvTariffMode->s.Lo, NvTariffMode->s.Hi, 0);

  else if (Packet_Parameter3 == 1)
    if (Packet_Parameter1 == 1 || Packet_Parameter1 == 2 || Packet_Parameter1 == 3)
      return Flash_Write16((uint16_t *) NvTariffMode, (uint16_t)Packet_Parameter1);

  return false;
}

bool HandleTime1Packet()
{
  uint8_t days, hours, minutes, seconds;

  // Get the current time
  RTC_Get(&days, &hours, &minutes, &seconds);

  if (Packet_Parameter3 == 0)
    // send the current time to the PC
    return Packet_Put (CMD_TIME1, seconds, minutes, 0);

  else if (Packet_Parameter3 == 1)
    if (Packet_Parameter1 >= 0 && Packet_Parameter1 <= 59 &&
        Packet_Parameter2 >= 0 && Packet_Parameter2 <= 59)
    {
      // keep the current days and hours
      // Set the new seconds and minutes
      RTC_Set(days, hours, Packet_Parameter2, Packet_Parameter1);

      return true;
    }

  return false;
}

bool HandleTime2Packet()
{
  uint8_t days, hours, minutes, seconds;

  // Get the current time
  RTC_Get(&days, &hours, &minutes, &seconds);

  if (Packet_Parameter3 == 0)
    // send the current time to the PC
    return Packet_Put (CMD_TIME2, hours, days, 0);

  else if (Packet_Parameter3 == 1)
    // As many days as wanted can be set
    if (Packet_Parameter1 >=0 && Packet_Parameter1 <= 23)
    {
      // Keep the current seconds and minutes
      // Set the new days and hours
      RTC_Set(Packet_Parameter2, Packet_Parameter1, minutes, seconds);

      return true;
    }

  return false;
}

bool HandlePowerPacket()
{
  uint16union_t powerUnion;

  powerUnion.l = (uint16_t) (AveragePowerW >> 16);

  return Packet_Put (CMD_POWER, powerUnion.s.Lo, powerUnion.s.Hi, 0);
}

bool HandleEnergyPacket()
{
  uint16union_t energyWhunion;

  energyWhunion.l = (uint16_t) ((TotalEnergykWh >> 16) * 1000);

  return Packet_Put (CMD_ENERGY, energyWhunion.s.Lo, energyWhunion.s.Hi, 0);
}

bool HandleCostPacket()
{
  float totalCostDollars = TotalCostDollars / 65536.0;

  uint16_t wholepart = (uint16_t) totalCostDollars;
  uint16_t fraction = (totalCostDollars - (float)wholepart) * 1000;

  return Packet_Put (CMD_COST, fraction, wholepart, 0);
}

bool HandleFrequencyPacket()
{
  uint16union_t frequencyTimes10;
  frequencyTimes10.l = (uint16) FrequencyTimes10;

  return Packet_Put (CMD_FREQUENCY, frequencyTimes10.s.Lo, frequencyTimes10.s.Hi, 0);
}

bool HandleVoltagePacket()
{
  uint16union_t vRMSunion;
  vRMSunion.l = (uint16_t) (Vrms >> 16);

  return Packet_Put (CMD_VOLTAGE_RMS, vRMSunion.s.Lo, vRMSunion.s.Hi, 0);
}

bool HandleCurrentPacket()
{
  uint16union_t iRMSunion;

  //multiply Irms global by (1000<<16) to convert to mA
  //bit shift right by 16 bits to convert to decimal
  iRMSunion.l = (uint16_t) (FixedPoint_Multiply(Irms, 1000 << 16) >> 16);

  return Packet_Put(CMD_CURRENT_RMS, iRMSunion.s.Lo, iRMSunion.s.Hi, 0);
}

bool HandlePowerFactorPacket()
{
  uint16union_t pfunion;

  //multiply PowerFactor global by (1000<<16) to scale up (given in specification)
  //bit shift right by 16 bits to convert to decimal
  pfunion.l = (uint16_t) (FixedPoint_Multiply(PowerFactor, 1000 << 16) >> 16);

  return Packet_Put (CMD_POWER_FACTOR, pfunion.s.Lo, pfunion.s.Hi, 0);
}


/***********************************************************************************************************
 * Handle Packets
 ************************************************************************************************************/

/*! @brief Responds to packets sent from PC
 *
 *  @note  Assumes that TowerInit has been called successfully
 */
void HandlePackets()
{
  //stores the success status of HandlePackets() subroutines
  bool success = false;

 //Stores the value of the ACK bit in the Command Parameter
  bool ACK = false;

  //Checks if ACK is requested
  if (Packet_Command & PACKET_ACK_MASK)
    ACK = true;

  //Stores the command parameter excluding the ACK bit
  uint8_t command = Packet_Command & (~PACKET_ACK_MASK);

  //Call HandlePacket Subroutines
  switch (command)
  {
    //Handle the Tower Startup Packet
    case CMD_TESTMODE:
      success = HandleTestModePacket();
      break;

    //Handle the Special - Tower Version Packet
    case CMD_TARIFF:
      success = HandleTariffPacket();
      break;

    //Handle the Tower Number packet
    case CMD_TIME1:
      success = HandleTime1Packet();
      break;

    //Handle the Tower Mode packet
    case CMD_TIME2:
      success = HandleTime2Packet();
      break;

    case CMD_POWER:
      success = HandlePowerPacket();
      break;

    case CMD_ENERGY:
      success = HandleEnergyPacket();
      break;

    case CMD_COST:
      success = HandleCostPacket();
      break;

    case CMD_FREQUENCY:
      success = HandleFrequencyPacket();
      break;

    case CMD_VOLTAGE_RMS:
      success = HandleVoltagePacket();
      break;

    case CMD_CURRENT_RMS:
      success = HandleCurrentPacket();
      break;

    case CMD_POWER_FACTOR:
      success = HandlePowerFactorPacket();
      break;
    }

    //Handle Acknowledgement, if requested
    if (ACK == true)
    {
      //left shift the success bit by 7 and append it to the sent command
      command |= (success << 7);

      //Transmit Acknowledgement Packet
      Packet_Put(command, Packet_Parameter1, Packet_Parameter2, Packet_Parameter3);
    }
}

/***********************************************************************************************************
 * TowerInit
 ************************************************************************************************************/

/*! @brief Initializes Tower by initializing other modules and sending startup packets.
 *
 *  @param pData is not used but is required by the OS to create a thread.
 *  @note This thread deletes itself after running for the first time.
 */
static bool TowerInit (void)
{

   // Disable Interrupts
  __DI();

  // Initialize the Flash Module
  if (!Flash_Init())
    PE_DEBUGHALT();

  // Allocate 16-bits memory in the Flash for Tariff Mode
  if (!Flash_AllocateVar((void*) &NvTariffMode, sizeof(*NvTariffMode)))
    PE_DEBUGHALT();

  //Check if the flash is erased or reprogrammed
  if (NvTariffMode->l == 0xFFFF)
    //Write the initial tower number to the non-volatile memory
    if (!Flash_Write16((uint16_t *) NvTariffMode, DEFAULT_TARIFF_MODE))
      PE_DEBUGHALT();

  //Initialize the Packet module, which in turn initializes the UART module
  if (!Packet_Init(BAUD_RATE, CPU_BUS_CLK_HZ))
    PE_DEBUGHALT();

  // Initialize the LEDs
  if (!LEDs_Init())
    PE_DEBUGHALT();

  // Initialize the PIT module
  if (!PIT_Init(CPU_BUS_CLK_HZ, &PITCallback, NULL))
    PE_DEBUGHALT();

  // Analog
  if (!Analog_Init(CPU_BUS_CLK_HZ))
    PE_DEBUGHALT();

  // Create the semaphore for the GET_ANALOG_SEMAPHORE
  AnalogGetSemaphore = OS_SemaphoreCreate(0);

  // Start the PIT timer with a period of 10ms (10e6 ns)
  PIT_Set(MAX_SAMPLE_PERIOD, true);

  // Initialize the FTM Module
  if (!FTM_Init())
    PE_DEBUGHALT();

  // Set up timer channel 0 as output compare
  if (!FTM_Set(&FTMChannel))
    PE_DEBUGHALT();

  // Initialize the RTC Module
  if (!RTC_Init())
    PE_DEBUGHALT();

  // Initialize the calculation threads
  if (!Calc_Init())
    PE_DEBUGHALT();

  if (!HMI_Init(FSMState))
    PE_DEBUGHALT();

  if (!Switch_Init(&SwitchCallback, NULL))
    PE_DEBUGHALT();

  // Turn on the orange LED if all the initializations are completed successfully
  LEDs_On(LED_ORANGE);

  // Enable Interrupts
  __EI();

  return true;
}

/***********************************************************************************************************
 * Main Routine
 ************************************************************************************************************/
/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  /* Write your local variable definition here */

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/
  OS_ERROR error;

  // Initialise low-level clocks etc using Processor Expert code
  PE_low_level_init();

  // Initialize the RTOS
  OS_Init(CPU_CORE_CLK_HZ, false);

  // TowerInit
  if (!TowerInit())
    PE_DEBUGHALT();

  error = OS_ThreadCreate(RTCThread,
                          NULL,
                          &RTCThreadStack[THREAD_STACK_SIZE - 1],
                          RTC_THREAD_PRIORITY);
  if (error)
    PE_DEBUGHALT();

  // PacketReceiveThread; Lowest Priority
 error = OS_ThreadCreate(PacketReceiveThread,
                         NULL,
                         &PacketReceiveThreadStack[THREAD_STACK_SIZE - 1],
                         PACKETRECEIVE_THREAD_PRIORITY);
 if (error)
   PE_DEBUGHALT();



  // Start multithreading - never returns!
  OS_Start();

  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
  #ifdef PEX_RTOS_START
    PEX_RTOS_START();                  /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;){}
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

/* END main */
/*!
** @}
*/
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.5 [05.21]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
