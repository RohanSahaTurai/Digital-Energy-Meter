/*! @file Switch.h
 *
 *  @brief Routines to operate SW1 switch
 *
 *  This contains routine to initialize and use the interrupt driven triggering for the switch.
 *
 *  @author Rohan
 *  @date 2019-11-05
 */

#ifndef SOURCES_SWITCH_H_
#define SOURCES_SWITCH_H_

// new types
#include "types.h"
// RTOS
#include "OS.h"
// Memory Mappings
#include "MK70F12.h"

/*! @brief Initializes the switch before first use.
 *
 *  Sets up the control register for switch and GPIO.
 *  Enables the switch and sets an interrupt every change of level.
 *  @return bool - TRUE if the RTC was successfully initialized.
 */
bool Switch_Init(void (*userFunction)(void*), void* userArguments);

/*! @brief Interrupt service routine for the switch
 *
 *  The switch has been pressed.
 *  The user callback function will be called.
 *  @note Assumes the switch has been initialized.
 */
void __attribute__ ((interrupt)) Switch_ISR (void);

#endif /* SOURCES_SWITCH_H_ */
