/*! @file
 *
 *  @brief Routines for setting up the FlexTimer module (FTM) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the FlexTimer module (FTM).
 *
 *  @author PMcL
 *  @date 2015-09-04
 */
/*! @addtogroup FTM_Module FTM module documentation
 *  @{
 *
 *  This contains the functions for operating the FlexTimer module (FTM).
 *
 */

#ifndef FTM_H
#define FTM_H

// new types
#include "types.h"
// Memory Mappings
#include "MK70F12.h"

/*!
 * @enum TTimerFunction
 */
typedef enum
{
  TIMER_FUNCTION_INPUT_CAPTURE,		/*!< Configures the timer function as input capture */
  TIMER_FUNCTION_OUTPUT_COMPARE		/*!< Configures the timer function as output compare */
} TTimerFunction;

/*!
 * @enum TTimerOutputAction
 */
typedef enum
{
  TIMER_OUTPUT_DISCONNECT,		/*!< Disconnects output when an trigger event occurs */
  TIMER_OUTPUT_TOGGLE,			/*!< Toggles output when a trigger event occurs */
  TIMER_OUTPUT_LOW,			/*!< Output low state when a trigger event occurs */
  TIMER_OUTPUT_HIGH			/*!< Output high state when a trigger event occurs */
} TTimerOutputAction;

/*!
 * @enum TTimerInputDetection
 */
typedef enum
{
  TIMER_INPUT_OFF,			/*!< Disconnects the input */
  TIMER_INPUT_RISING,			/*!< Triggers event on the rising edge of the input */
  TIMER_INPUT_FALLING,			/*!< Triggers event on the falling edge of the output */
  TIMER_INPUT_ANY			/*!< Triggers event on falling or rising edge of the input*/
} TTimerInputDetection;

/*!
 * @struct TFTMChannel
 *
 * @brief aFTMChannel is a structure containing the parameters to be used in setting up the timer channel.
 *
 */
typedef struct
{
  uint8_t channelNb;			/*!< the channel number of the FTM to use */
  uint16_t delayCount;			/*!< the delay count (in module clock periods) for an output compare event */
  TTimerFunction timerFunction;		/*!< used to set the timer up as either an input capture or an output compare*/
  union
  {
    TTimerOutputAction outputAction;		/*!< the action to take on a successful output compare */
    TTimerInputDetection inputDetection;	/*!< Tthe type of input capture detection */
  } ioType;					/*!< a union that depends on the setting of the channel as input capture or output compare */
  void (*callbackFunction)(void*);		/*!< a pointer to a user callback function */
  void *callbackArguments;			/*!< a pointer to the user arguments to use with the user callback function*/
} TFTMChannel;


/*! @brief Sets up the FTM before first use.
 *
 *  Enables the FTM as a free running 16-bit counter.
 *  @return bool - TRUE if the FTM was successfully initialized.
 */
bool FTM_Init();

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
bool FTM_Set(const TFTMChannel* const aFTMChannel);


/*! @brief Starts a timer if set up for output compare.
 *
 *  @param aFTMChannel is a structure containing the parameters to be used in setting up the timer channel.
 *  @return bool - TRUE if the timer was started successfully.
 *  @note Assumes the FTM has been initialized.
 */
bool FTM_StartTimer(const TFTMChannel* const aFTMChannel);


/*! @brief Interrupt service routine for the FTM.
 *
 *  If a timer channel was set up as output compare, then the user callback function will be called.
 *  @note Assumes the FTM has been initialized.
 */
void __attribute__ ((interrupt)) FTM0_ISR(void);

#endif

/*!
 *  @}
 */


