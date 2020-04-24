/*! @file
 *
 *  @brief Routines for setting up the FlexTimer module (FTM) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the FlexTimer module (FTM).
 *
 *  @author Rohan, Jack
 *  @date 2019-09-13
 */

// Include header files
#include "FTM.h"

#define NCHANNELS 8	/*!<  Number of channels to be used in FTM0 */

//Private Global Variables
static void* UserArguments[NCHANNELS];		/*!< Private global pointer to the user arguments for the call back function */
static void (*UserFunction[NCHANNELS])(void *);	/*!< Private global pointer to the user call back function */

/*! @brief Sets up the FTM before first use.
 *
 *  Enables the FTM as a free running 16-bit counter.
 *  @return bool - TRUE if the FTM was successfully initialized.
 */
bool FTM_Init()
{
  // Enable the clock gate to FTM0 Module
  SIM_SCGC6 |= SIM_SCGC6_FTM0_MASK;

  // Clear the initial value for the FTM counter to 0 in bits [0:15]
  FTM0_CNTIN &= ~FTM_CNTIN_INIT_MASK;

  // Set the module value for the FTM counter to 0xFFFF so that it works as a free timer
  FTM0_MOD |= FTM_MOD_MOD_MASK;

  // Write any value to COUNT bits of FTM0_CNT to update the counter with its initial value
  FTM0_CNT |= FTM_CNT_COUNT_MASK;

  // Select the Fixed Frequency Clock for the module
  FTM0_SC |= FTM_SC_CLKS(2);

  // Initialize the NVIC
  // Vector = 78, IQR = 62
  // NVIC non-IPR = 1, bit-location = 30
  // NVIC IPR = 15, bit-location = [12:15]
  // Clear any pending interrupts on FTM0 by setting bit 30 in NVICICPR2 (NVIC Interrupt Clear Pending Register)
  NVICICPR1 |= (1 << 30);
  // Enable interrupts from PIT module by setting bit 30 in NVICISER2 ((NVIC Interrupt Set Enable Register)
  NVICISER1 |= (1 << 30);

  return true;
}

/*! @brief Sets up a timer channel.
 *
 *  @param aFTMChannel is a structure containing the parameters to be used in setting up the timer channel.
 *    channelNb is the channel number of the FTM to use.
 *    delayCount is the delay count (in module clock periods) for an output compare event.
 *    timerFunction is used to set the timer up as either an input capture or an output compare.
 *    ioType is a union that depends on the setting of the channel as input capture or output compare:
 *      outputAction is the action to take on a successful output compare.
 *      inputDetection is the type of input capture detection.
 *    callbackFunction is a pointer to a user callback function.
 *    callbackArguments is a pointer to the user arguments to use with the user callback function.
 *  @return bool - TRUE if the timer was set up successfully.
 *  @note Assumes the FTM has been initialized.
 */
bool FTM_Set(const TFTMChannel* const aFTMChannel)
{

  // validate channel number
  if (aFTMChannel->channelNb >= NCHANNELS)
    return false;

  // Set the timer function (IC/OC)
  if (aFTMChannel->timerFunction == TIMER_FUNCTION_INPUT_CAPTURE)
    // MSnB:MSnA = 00
    FTM0_CnSC(aFTMChannel->channelNb) &= ~(FTM_CnSC_MSB_MASK | FTM_CnSC_MSA_MASK);	// clear MSnB and MSnA

  else if (aFTMChannel->timerFunction == TIMER_FUNCTION_OUTPUT_COMPARE)
    {
      // MSnB:MSnA = 01
      FTM0_CnSC(aFTMChannel->channelNb) &= ~FTM_CnSC_MSB_MASK;	// clear MSnB
      FTM0_CnSC(aFTMChannel->channelNb) |= FTM_CnSC_MSA_MASK;	// set MSnA
    }

  // return false if timerFunction is none of the above
  else
    return false;

  // Select the Edge for Input Capturing
  switch (aFTMChannel->ioType.inputDetection)
  {
    case TIMER_INPUT_RISING:
      // ELSnB:ELSnA = 01
      FTM0_CnSC(aFTMChannel->channelNb) &= ~FTM_CnSC_ELSB_MASK;	// clear ELSnB
      FTM0_CnSC(aFTMChannel->channelNb) |= FTM_CnSC_ELSA_MASK;	// set ELSnA
      break;

    case TIMER_INPUT_FALLING:
      // ELSnB:ELSnA = 10
      FTM0_CnSC(aFTMChannel->channelNb) |= FTM_CnSC_ELSB_MASK;	// set ELSnB
      FTM0_CnSC(aFTMChannel->channelNb) &= ~FTM_CnSC_ELSA_MASK;	// clear ELSnA
      break;

    case TIMER_INPUT_ANY:
      // ELSnB:ELSnA = 11
      FTM0_CnSC(aFTMChannel->channelNb) |= (FTM_CnSC_ELSB_MASK | FTM_CnSC_ELSA_MASK);	// set ELSnB and ELSnA
      break;

    default:	// TIMER_INPUT_OFF
      // ELSnB:ELSnA = 00
      FTM0_CnSC(aFTMChannel->channelNb) &= ~(FTM_CnSC_ELSB_MASK | FTM_CnSC_ELSA_MASK);	// clear ELSnB and ELSnA
  }

  // Point the private global UserArguments pointer to the address of the callbackArguments
  UserArguments [aFTMChannel->channelNb] = aFTMChannel-> callbackArguments;

  // Point the private global callback function pointer to the address of the callbackFunction
  UserFunction [aFTMChannel->channelNb] = aFTMChannel-> callbackFunction;

  return true;
}

/*! @brief Starts a timer if set up for output compare.
 *
 *  @param aFTMChannel is a structure containing the parameters to be used in setting up the timer channel.
 *  @return bool - TRUE if the timer was started successfully.
 *  @note Assumes the FTM has been initialized.
 */
bool FTM_StartTimer(const TFTMChannel* const aFTMChannel)
{
  // Validate channel
  if (aFTMChannel->channelNb < NCHANNELS)
    // Check if the channel is setup for output compare
    if (aFTMChannel->timerFunction == TIMER_FUNCTION_OUTPUT_COMPARE)
      {
        // Clear any pending channel flag by clearing the CHF bit
        FTM0_CnSC(aFTMChannel->channelNb) &= ~FTM_CnSC_CHF_MASK;

        // Enable interrupt for the channel by setting the CHIE bit
        FTM0_CnSC(aFTMChannel->channelNb) |= FTM_CnSC_CHIE_MASK;

        // Load the value to be matched for an interrupt in CnV register
        // Record the current value of the timer and add the delay count to the current value
        FTM0_CnV(aFTMChannel->channelNb) = FTM0_CNT + aFTMChannel->delayCount;

        return true;
      }

  // return false if channel number is invalid and function is not output compare
  return false;
}

/*! @brief Interrupt service routine for the FTM.
 *
 *  If a timer channel was set up as output compare, then the user callback function will be called.
 *  @note Assumes the FTM has been initialized.
 */
void __attribute__ ((interrupt)) FTM0_ISR(void)
{
  uint8_t channelNb;	// stores the channel number for which an event has occurred

  for (channelNb = 0; channelNb < NCHANNELS; channelNb++)
  {
    // Check if the interrupt is enabled and flag is set for the channel
    if ((FTM0_CnSC(channelNb) & FTM_CnSC_CHF_MASK) &&
        (FTM0_CnSC(channelNb) & FTM_CnSC_CHIE_MASK))
    {
      // Clear the flag as interrupt is being handled
      FTM0_CnSC(channelNb) &= ~FTM_CnSC_CHF_MASK;

      // Disable the interrupt for the channel
      FTM0_CnSC(channelNb) &= ~FTM_CnSC_CHIE_MASK;

      // Check if a call back function exists for the channel
      if (UserFunction[channelNb])
        // execute the callback function
        (*UserFunction[channelNb])(UserArguments[channelNb]);
    }
  }
}



