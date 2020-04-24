/*! @file
 *
 *  @brief Routines for controlling the Real Time Clock (RTC) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the real time clock (RTC).
 *
 *  @author Jack, Rohan 
 *  @date 2019-09-14
 */

//Included header files
#include "RTC.h"

/*! @brief Initializes the RTC before first use.
 *
 *  Sets up the control register for the RTC and locks it.
 *  Enables the RTC and sets an interrupt every second.
 *  @return bool - TRUE if the RTC was successfully initialized.
 */
bool RTC_Init()
{
  //Enable clock gate for RTC
  SIM_SCGC6 |= SIM_SCGC6_RTC_MASK;
  
  //Reset SWR Bit
  RTC_CR &= ~RTC_CR_SWR_MASK;

  // Set the time to 0
  RTC_TSR = 0;

  //Configuring load to 18pF
  RTC_CR |= RTC_CR_SC16P_MASK | RTC_CR_SC2P_MASK;

  //Enable the oscillator
  RTC_CR |= RTC_CR_OSCE_MASK;

  // Wait 1000 ms for the oscillator to stable
  for(uint32_t i = 0; i < 0x600000; i++);

  //Lock the control register so writes are ignored
  RTC_LR &= ~RTC_LR_CRL_MASK;

  //Enable the time seconds interrupt
  RTC_IER |= RTC_IER_TSIE_MASK;

  RTC_SR |= RTC_SR_TCE_MASK;

  //Clear current pending interrupts on the RTC
  NVICICPR2 |= NVIC_ICPR_CLRPEND(1 << 3);

  //Enable interrupts
  NVICISER2 |= NVIC_ISER_SETENA(1 << 3);

  // Create Semaphore to be signaled by the ISR
  RTC_Semaphore = OS_SemaphoreCreate(0);

  return true;
}

/*! @brief Sets the value of the real time clock.
 *
 *  @param hours The desired value of the real time clock hours (0-23).
 *  @param minutes The desired value of the real time clock minutes (0-59).
 *  @param seconds The desired value of the real time clock seconds (0-59).
 *  @note Assumes that the RTC module has been initialized and all input parameters are in range.
 */
void RTC_Set(const uint8_t days, const uint8_t hours, const uint8_t minutes, const uint8_t seconds)
{
  //Disable the clock to write to it
  RTC_SR &= ~(RTC_SR_TCE_MASK);

  //A variable to store the desired time in
  uint32_t time = 0;

  //Store the desired time in seconds
  time = (days * 86400) + (hours * 3600) + (minutes * 60) + seconds;

  //Set the time
  RTC_TSR = time;

  //Re-enable the clock to begin counting again
  RTC_SR |= RTC_SR_TCE_MASK;
}

/*! @brief Gets the value of the real time clock.
 *
 *  @param hours The address of a variable to store the real time clock hours.
 *  @param minutes The address of a variable to store the real time clock minutes.
 *  @param seconds The address of a variable to store the real time clock seconds.
 *  @note Assumes that the RTC module has been initialized.
 */
void RTC_Get(uint8_t* days, uint8_t* const hours, uint8_t* const minutes, uint8_t* const seconds)
{
  //Set time to the value of the time seconds register
  uint32_t time = RTC_TSR;

  //Check indefinitely until double read matches
  while (time != RTC_TSR)
    time = RTC_TSR;
  
  //Store days
  *days = (time - (time % 86400)) / 86400;

  //Remove days from time
  time %= 86400;

  //Store hours 
  *hours = (time - (time % 3600)) / 3600;

  //Remove hours from time
  time %= 3600;

  //Store minutes
  *minutes = (time - (time % 60)) / 60;

  //Remove minutes from time
  time %= 60;

  //The remainder is in seconds
  *seconds = time;
}

/*! @brief Interrupt service routine for the RTC.
 *
 *  The RTC has incremented one second.
 *  The user callback function will be called.
 *  @note Assumes the RTC has been initialized.
 */
void __attribute__ ((interrupt)) RTC_ISR(void)
{
  OS_ISREnter();

  OS_ERROR error;

  error = OS_SemaphoreSignal(RTC_Semaphore);

  if (error)
    PE_DEBUGHALT();

  OS_ISRExit();
}
