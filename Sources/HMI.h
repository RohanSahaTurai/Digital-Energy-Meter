/*! @file HMI.h
 *
 *  @brief Human Interface Machine for DEM
 *
 *  This contains the state machine to cycle through time, power, energy and cost state
 *
 *  @author Rohan
 *  @date 2019-11-05
 */

#ifndef SOURCES_HMI_H_
#define SOURCES_HMI_H_

// new types
#include "types.h"
#include "PE_Types.h"
#include "UART.h"
#include <stdio.h>

#define NB_DISPLAY_STATES 5

typedef struct State
{
  void (*stateFunction) (void);
  struct State* nextState;
}TState;


extern uint32_t TimeUsage;
extern uint32_t AveragePowerW;
extern uint32_t TotalEnergykWh;
extern uint32_t TotalCostDollars;

bool HMI_Init();

void HMI_TimeState (void);

void HMI_PowerState (void);

void HMI_EngergyState (void);

void HMI_CostState (void);

void HMI_UpdateState (TState* FSMState, bool resetToDormant);

void HMI_DisplayCurrentState (void);

#endif /* SOURCES_HMI_H_ */
