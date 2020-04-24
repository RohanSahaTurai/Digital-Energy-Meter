/*! @file
 *
 *  @brief Routines for controlling Periodic Interrupt Timer (PIT) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the periodic interrupt timer (PIT).
 *
 *  @author Jack, Rohan
 *  @date 2019-09-13
 */

// Included header files
#include "PIT.h"

//Private Global Variables
static void* UserArguments;			/*!< Private global pointer to the user arguments of the callback function */
static void (*UserFunction)(void* );		/*!< Private global pointer to the PIT user callback function */
static uint32_t ModuleClk;			/*!< Private global variable to store the value of the module clock */

/*! @brief Sets up the PIT before first use.
 *
 *  Enables the PIT and freezes the timer when debugging.
 *  @param moduleClk The module clock rate in Hz.
 *  @return bool - TRUE if the PIT was successfully initialized.
 *  @note Assumes that moduleClk has a period which can be expressed as an integral number of nanoseconds.
 */
bool PIT_Init(const uint32_t moduleClk, void (*userFunction)(void*), void* userArguments)
{
  // Point the private global UserArguments pointer to the address of the userArguments
  UserArguments = userArguments;

  // Point the private global callback function pointer to the address of the userFunction
  UserFunction = userFunction;

  // Make the moduleClk globally (private) available
  ModuleClk = moduleClk;

  // Enable the clock gating to the PIT module
  SIM_SCGC6 |= SIM_SCGC6_PIT_MASK;

  // Enable the module by clearing the MDIS bit in PIT_MCR
  PIT_MCR &= ~(PIT_MCR_MDIS_MASK);

  // Freeze the timer when debugging by setting the FRZ bit in PIT_MCR
  PIT_MCR |= PIT_MCR_FRZ_MASK;

  // Enable the interrupt for Timer0 by setting the TIE bit in PIT_TCTRL0
  PIT_TCTRL0 |= PIT_TCTRL_TIE_MASK;

  // Initialize the NVIC
  // Vector = 84, IQR = 68
  // NVIC non-IPR = 2, bit-location = 4
  // NVIC IPR = 17, bit-location = [4:7]
  // Clear any pending interrupts on PIT by setting bit 4 in NVICICPR2 (NVIC Interrupt Clear Pending Register)
  NVICICPR2 |= (1 << 4);
  // Enable interrupts from PIT module by setting bit 4 in NVICISER2 ((NVIC Interrupt Set Enable Register)
  NVICISER2 |= (1<<4);

  return true;
}

/*! @brief Sets the value of the desired period of the PIT.
 *
 *  @param period The desired value of the timer period in nanoseconds.
 *  @param restart TRUE if the PIT is disabled, a new value set, and then enabled.
 *                 FALSE if the PIT will use the new value after a trigger event.
 *  @note The function will enable the timer and interrupts for the PIT.
 */
void PIT_Set(const uint32_t period, const bool restart)
{
  // Calculate the value to be loaded in the timer
  uint32_t timerValue = (ModuleClk / (1e9/period)) - 1;

  // Check if the timerValue is less than 0xFFFFFFFF
  if (timerValue <= 0xFFFFFFFF)
  {
    // Load the value in Timer0
    PIT_LDVAL0 = timerValue;

    if (restart)
    {
      // Disable Timer0 to abort the current cycle, if any
      PIT_Enable(false);

      // Enable Timer0 to restart the cycle
      PIT_Enable (true);
    }
  }

}

/*! @brief Enables or disables the PIT.
 *
 *  @param enable - TRUE if the PIT is to be enabled, FALSE if the PIT is to be disabled.
 */
void PIT_Enable(const bool enable)
{
  if (enable)
    // Enable Timer0 by setting the TEN bit in PIT_TCTRL0
    PIT_TCTRL0 |=  PIT_TCTRL_TEN_MASK;

  else
    // Disable Timer0 by clearing the TEN bit in PIT_TCTRL0
    PIT_TCTRL0 &=  ~PIT_TCTRL_TEN_MASK;
}

/*! @brief Interrupt service routine for the PIT.
 *
 *  The periodic interrupt timer has timed out.
 *  The user callback function will be called.
 *  @note Assumes the PIT has been initialized.
 */
void __attribute__ ((interrupt)) PIT_ISR(void)
{
  OS_ISREnter();

  // Check if timeout has occurred
  if (PIT_TFLG0 & PIT_TFLG_TIF_MASK)
  {
    // Clear the interrupt flag once interrupt is handle
    PIT_TFLG0 |= PIT_TFLG_TIF_MASK;

    if(UserFunction)	// Null Check
    	// Call the user function
    	(*UserFunction) (UserArguments);
  }

  OS_ISRExit();
}
