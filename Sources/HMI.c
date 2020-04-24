/*! @file HMI.c
 *
 *  @brief Human Interface Machine for DEM
 *
 *  This contains the state machine to cycle through time, power, energy and cost state
 *
 *  @author Rohan
 *  @date 2019-11-05
 */

#include "HMI.h"

#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))


TState* CurrentState;

bool HMI_Init(TState* FSMState)
{
  if (!FSMState)
    return false;

  CurrentState = FSMState;

  return true;
 }

void HMI_TimeState(void)
{
  char dataString [25];

  // create a temporary temporary time variable
  uint32_t timeTemp = TimeUsage;

  uint8_t days, hours, minutes, seconds;

  days = (timeTemp - (timeTemp % 86400)) / 86400;

  //Remove days from time
  timeTemp %= 86400;

  //Store hours
  hours = (timeTemp - (timeTemp % 3600)) / 3600;

  //Remove hours from time
  timeTemp %= 3600;

  //Store minutes
  minutes = (timeTemp - (timeTemp % 60)) / 60;

  //Remove minutes from time
  timeTemp %= 60;

  //The remainder is in seconds
  seconds = timeTemp;

  if (days > 99)
    sprintf(dataString, "xx : xx : xx : xx\n");

  else
    sprintf(dataString, "%02d:%02d:%02d:%02d\n", days, hours, minutes, seconds);

  for (uint8_t i = 0; dataString[i] != '\0' ; i++)
    UART_OutChar(dataString[i]);
}

void HMI_PowerState(void)
{
  uint32_t averagePowerW = round(AveragePowerW / 65536.0);

  char dataString [25];

  if (averagePowerW > 999999)
    sprintf(dataString, "PPP.ppp\n");

  else
    sprintf(dataString, "%03d.%03d kW\n", averagePowerW / 1000, averagePowerW % 1000);

  for (uint8_t i = 0; dataString[i] != '\0' ; i++)
    UART_OutChar(dataString[i]);
}

void HMI_EngergyState(void)
{
  float totalEnergykWh = TotalEnergykWh / 65536.0;

  uint16_t wholepart = (uint16_t) totalEnergykWh;
  uint16_t fraction = (totalEnergykWh - (float)wholepart) * 1000;

  char dataString [25];

  if (totalEnergykWh > 999)
    sprintf(dataString, "xxx.xxx\n");

  else
    sprintf(dataString, "%03d.%03d kWh\n", wholepart, fraction);

  for (uint8_t i = 0; dataString[i] != '\0' ; i++)
    UART_OutChar(dataString[i]);;
}

void HMI_CostState(void)
{
  char dataString [25];

  float totalCostDollars = TotalCostDollars / 65536.0;

  uint16_t wholepart = (uint16_t) totalCostDollars;
  uint16_t fraction = (totalCostDollars - (float)wholepart) * 1000;

  if (wholepart > 9999)
    sprintf(dataString, "xxxx.xx\n");

  else
    sprintf(dataString, "$%04d.%02d\n", wholepart, fraction);

  for (uint8_t i = 0; dataString[i] != '\0' ; i++)
    UART_OutChar(dataString[i]);
}


inline void HMI_UpdateState (TState* FSMState, bool resetToDormant)
{
  // if reset to dormant is requested, the current state is set to dormant
  if (resetToDormant)
    CurrentState = FSMState;

  // else, move to the next state
  else
    CurrentState = CurrentState->nextState;
}

inline void HMI_DisplayCurrentState (void)
{
  // Check that the current state is not the dormant state
  // No function exists for the dormant state
  if (CurrentState->stateFunction != NULL)
    CurrentState-> stateFunction();
}

