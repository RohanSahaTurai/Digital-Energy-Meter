/*! @file  LEDs.c
 *
 *  @brief Routines to access the LEDs on the TWR-K70F120M.
 *
 *  This contains the functions for operating the LEDs.
 *
 *  @author Jack, Rohan
 *  @date 2019-08-26
 */

#include "LEDs.h"

bool LEDs_Init()
{
  // Enable PORT A 
  SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK;  

  // Multiplexing pins to allow LED usage
  PORTA_PCR10 |= PORT_PCR_MUX(1); // Blue
  PORTA_PCR11 |= PORT_PCR_MUX(1); // Orange
  PORTA_PCR28 |= PORT_PCR_MUX(1); // Yellow
  PORTA_PCR29 |= PORT_PCR_MUX(1); // Green

  // PSOR register tells GPIOA that these pins will be set in PDOR
  GPIOA_PSOR |= LED_BLUE | LED_ORANGE | LED_YELLOW | LED_GREEN;

  // PDDR register sets port pins to output
  GPIOA_PDDR |= LED_BLUE | LED_ORANGE | LED_YELLOW | LED_GREEN;

  return true;
}

/*! @brief Sets up the LEDs before first use.
 *
 *  @return bool - TRUE if the LEDs were successfully initialized.
 */
void LEDs_On(const TLED color)
{
  // PCOR register clears output pin, turning LED on
  GPIOA_PCOR = color;
}
 
/*! @brief Turns off an LED.
 *
 *  @param color The color of the LED to turn off.
 *  @note Assumes that LEDs_Init has been called.
 */
void LEDs_Off(const TLED color)
{
  // PSOR register sets output pin, turning LED off
  GPIOA_PSOR |= color;
}

/*! @brief Toggles an LED.
 *
 *  @param color The color of the LED to toggle.
 *  @note Assumes that LEDs_Init has been called.
 */
void LEDs_Toggle(const TLED color)
{
  // PTOR register toggles output pin, changing the state of the LED
  GPIOA_PTOR |= color;
}


