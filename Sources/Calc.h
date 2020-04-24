/*! @file Calc.h
 *
 *  @brief Routines to calculate measurements for the DEM
 *
 *  This contains the tariff rate and functions to calculate measurements for the DEM
 *  Requires RTOS compatibility to run
 *
 *  @author Rohan
 *  @date 2019-11-05
 */

#ifndef SOURCES_CALC_H_
#define SOURCES_CALC_H_

// new types
#include "types.h"
// fixed-point processing
#include "FixedPoint.h"
// RTOS
#include "OS.h"
// CPU exception handler
#include "CPU.h"
// PIT module to set the new timer
#include "PIT.h"
// Packet module to send packets
#include "packet.h"
// RTC Module to get the time
#include "RTC.h"

#define ANALOG_WINDOW_SIZE 16
#define NB_TARIFF_MODE 3


typedef struct Tariff
{
  uint32_t nonToUrate;
  uint32_t peakRate;
  uint32_t shoulderRate;
  uint32_t offPeakRate;
}TTariff;

TTariff TariffChart[NB_TARIFF_MODE];

extern const uint32_t MAX_SAMPLE_PERIOD;      /*! The sample rate for the analog input in nanoseconds */

extern const uint8_t CALCULATION_THREAD_PRIORITY;   //Extern declared thread priority
extern OS_ECB *AnalogGetSemaphore;                  //Extern declared global semaphore

extern int16_t Voltage_ADC [ANALOG_WINDOW_SIZE];
extern int16_t Current_ADC [ANALOG_WINDOW_SIZE];

extern volatile uint16union_t *NvTariffMode;

extern volatile bool TestModeEnabled;

uint32_t AveragePowerW;
uint32_t TotalEnergykWh;
uint32_t TotalCostDollars;
uint32_t FrequencyTimes10;
uint32_t Vrms;
uint32_t Irms;
uint32_t PowerFactor;

bool Calc_Init();

int32_t Calc_ConvertADCtoVolts (int16_t outputADC);

void Calc_TotalCost (uint32_t energyPerCycle);

int32_t Calc_AveragePower(int32_t instPower, bool risingEdgeDetected);

uint32_t Calc_TotalEnergy (int32_t instPower, uint32_t samplePeriod, bool risingEdgeDetected);

bool Calc_FrequencyTracking (int32_t sample, uint32_t* samplePeriod);

int32_t Calc_Vrms (int32_t instVoltage, bool risingEdgeDetected);

int32_t Calc_Irms (int32_t instCurrent, bool risingEdgeDetected);

#endif /* SOURCES_CALC_H_ */
