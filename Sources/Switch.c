/*! @file Switch.c
 *
 *  @brief Routines to operate SW1 switch
 *
 *  This contains routine to initialize and use the interrupt driven triggering for the switch.
 *
 *  @author Rohan
 *  @date 2019-11-05
 */

#include "Switch.h"

static void* UserArguments;
static void (*UserFunction)(void* );

/*! @brief Initializes the switch before first use.
 *
 *  Sets up the control register for switch and GPIO.
 *  Enables the switch and sets an interrupt every change of level.
 *  @return bool - TRUE if the RTC was successfully initialized.
 */
bool Switch_Init(void (*userFunction)(void*), void* userArguments)
{
  // Point the private global UserArguments pointer to the address of the userArguments
  UserArguments = userArguments;

  // Point the private global callback function pointer to the address of the userFunction
  UserFunction = userFunction;

  // Set Clock Gate
  SIM_SCGC5 |= SIM_SCGC5_PORTD_MASK;

  // Set registers for PORTD
  // K70 manual pg. 316
  PORTD_PCR0 |= PORT_PCR_MUX(1);
  PORTD_PCR0 |= PORT_PCR_ISF_MASK;
  PORTD_PCR0 |= PORT_PCR_IRQC(10);
  PORTD_PCR0 |= PORT_PCR_PE_MASK;
  PORTD_PCR0 |= PORT_PCR_PS_MASK;

  // IRQ mod 32
  NVICICPR2 |= (1 << 26); // Clear interrupts
  NVICISER2 |= (1 << 26); // Enable interrupts

  return true;
}

/*! @brief Interrupt service routine for the switch
 *
 *  The switch has been pressed.
 *  The user callback function will be called.
 *  @note Assumes the switch has been initialized.
 */
void __attribute__ ((interrupt)) Switch_ISR (void)
{
  OS_ISREnter();

  // Clear interrupt flag
  PORTD_ISFR |= PORT_ISFR_ISF(0);

  // Disable interrupts
  PORTD_PCR0 &= ~PORT_PCR_IRQC_MASK;

  if(UserFunction)  // Null Check
    // Call the user function
    (*UserFunction) (UserArguments);

  // Enable Interrupt
  PORTD_PCR0 |= PORT_PCR_IRQC(10);

  OS_ISRExit();

}



