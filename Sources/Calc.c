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

#include "Calc.h"

#define THREAD_STACK_SIZE 1000

const int32_t VOLTAGE_RAW_ADC_RATIO_32Q16 = (100<<16);
const int32_t CURRENT_RAW_ADC_RATIO_32Q16 = (1<<16);
const int32_t MAX_ADC_OUTPUT_32Q16 = (1UL<<31)-(1UL<<16);
const int32_t ADC_VOLTAGE_RANGE_32Q16 = 10 << 16;

// Thread prototypes
static void Calc_CalculationThread (void* pData);

// Create Thread Stack
static uint32_t CalculationThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08)));

bool Calc_Init()
{
  OS_ERROR error;

  // Create Semaphores

  // Clear Time usage at start


  // Load the tariffs
  // The tariff rates are stored in 32Q16
  /* Mode | Non-ToU |   Peak  | Shoulder | Off Peak
   *  1   |    0    |  22.235 |   4.400  |  2.109
   *  2   |  1.713  |    0    |    0     |    0
   *  3   |  4.100  |    0    |    0     |    0
   */
  TTariff tariffChartLocal[NB_TARIFF_MODE] =
  {
    { .peakRate = 1457193,
      .shoulderRate = 288350,
      .offPeakRate = 138216
    },
    {
      .peakRate = 112264,
      .shoulderRate = 0,
      .offPeakRate = 0
    },
    {
      .peakRate = 268698,
      .shoulderRate = 0,
      .offPeakRate = 0
    }
  };

  *TariffChart = *tariffChartLocal;

  // Initialize the global variables to be 0
  AveragePowerW   = 0;
  TotalEnergykWh  = 0;
  TotalCostDollars = 0;
  FrequencyTimes10 = 0;
  Vrms = 0;
  Irms = 0;

  // Create threads
  error = OS_ThreadCreate(Calc_CalculationThread,
                          NULL,
                          &CalculationThreadStack[THREAD_STACK_SIZE - 1],
                          CALCULATION_THREAD_PRIORITY);
  if (error)
    PE_DEBUGHALT();

  return true;
}

int32_t Calc_ConvertADCtoVolts (int16_t outputADC)
{
  //convert the ADC output to 32Q16 format
  int32_t output32Q16 = FixedPoint_Convert32Q16(outputADC);

  //Calculate the ratio of the ADC output
  int32_t ratio32Q16 = FixedPoint_Divide(output32Q16, MAX_ADC_OUTPUT_32Q16);

  // Multiply the ratio with the max voltage
  int32_t volts32Q16 = FixedPoint_Multiply(ADC_VOLTAGE_RANGE_32Q16, ratio32Q16);

  return volts32Q16;
}

void Calc_TotalCost (uint32_t energyPerCycleWs)
{
    uint32_t currentRate;

    uint8_t days, hours, minutes, seconds;

    RTC_Get (&days, &hours, &minutes, &seconds);

    static uint32_t accumulatedCentsScaledUp = 0;
    static uint32_t accumulatedCents = 0;

    switch ((uint8) NvTariffMode->l)
    {
      // Tariff Mode 1
      case 1:
        // Peak Period
        if (hours >= 14 && hours <=20)
          currentRate = TariffChart[0].peakRate;
        else if ((hours >= 7 && hours <= 14) || (hours >=20 && hours <= 22))
          currentRate = TariffChart[0].shoulderRate;
        else
          currentRate = TariffChart[0].offPeakRate;
        break;

      // Tariff Mode 2
      case 2:
        currentRate = TariffChart[1].nonToUrate;
        break;

      // Tariff Mode 3
      case 3:
        currentRate = TariffChart[2].nonToUrate;
        break;
    }

    // cents/k = cents/kWh * Ws / 3600
    uint32_t centsPerCycleScaledUp = FixedPoint_Divide(FixedPoint_Multiply(currentRate, energyPerCycleWs), 3600 << 16);

    // accumulate the scaled down cents
    accumulatedCentsScaledUp += centsPerCycleScaledUp;

    if (accumulatedCentsScaledUp >= (1000 << 16))
    {
      accumulatedCents += FixedPoint_Divide(accumulatedCentsScaledUp, 1000 << 16);
      accumulatedCentsScaledUp = 0;
    }

    if (accumulatedCents >= (1000 << 16))
    {
      TotalCostDollars += FixedPoint_Divide(accumulatedCents, 100 << 16);
      accumulatedCents = 0;
    }
}

uint32_t Calc_TotalEnergy (int32_t instPower, uint32_t samplePeriod, bool risingEdgeDetected)
{
  static int32_t summationInstPower = 0;
  uint32_t energyPerCycleWs;

  static uint32_t tempAccumulatedEnergyWs = 0;

  // risingEdgeDetected flag is set on receiving the first sample of the next cycle
  // but samplePeriod stores the value of Ts for the previous cycle
  if (risingEdgeDetected)
  {
    /* Energy (in Ws) = (Sum(instPower) * Ts(in s) */

    // Ts (in nanoseconds) / 10e5 = Ts * 10e-4 seconds
    samplePeriod = samplePeriod / 100000;

    // convert samplePeriod to 32Q16
    int32_t samplePeriod32Q16 = samplePeriod << 16;

    /* Energy (Ws) = (Sum_power * Ts * 10e-4)
     *             =  Sum_power * Ts/10e4
     */
    energyPerCycleWs = FixedPoint_MultiplyU(summationInstPower, FixedPoint_DivideU(samplePeriod32Q16, 10000U << 16));

    if (TestModeEnabled)
        energyPerCycleWs = FixedPoint_MultiplyU(energyPerCycleWs, 3600U<<16);

    // add the total energy Ws in an a temporary variable
    tempAccumulatedEnergyWs += energyPerCycleWs;

    // 1 Wh = 3600 Ws (0.001 kWh = 3600 Ws)
    // We want a precision of 0.001 kWh, therefore every time the accumulated energy is greater than 3600 Ws,
    // we add 0.001 kWh energy to the TotalEnergykWh
    // Avoids overflow and increases maximum amount to be stored
    if (tempAccumulatedEnergyWs >= (3600<<16))
    {
      // convert tempAccumulatedEnergy Ws to Wh (tempEnergyWs / 3600)
      // convert tempAcummulatedEnergy from Wh to kWh (divide by 1000)
      TotalEnergykWh += FixedPoint_Divide( FixedPoint_Divide(tempAccumulatedEnergyWs, (3600<<16)), (1000<<16));

      // reset the accumulatedEnergy
      tempAccumulatedEnergyWs = 0;
    }

    // reset the sum of instantaneous power for the next cycle
    summationInstPower = 0;
  }

  summationInstPower += instPower;

  return energyPerCycleWs;
}

int32_t Calc_AveragePower(int32_t instPower, bool risingEdgeDetected)
{
  static uint8_t counter = 0;

  int32_t averagePower = 0;

  static int32_t sumInstPower = 0;


  if (risingEdgeDetected)
  {
    // average Power = sum(instantaneous Power) / sampleNbPerCycle
    averagePower = FixedPoint_Divide(sumInstPower, (int32_t) counter << 16);

    // Update the Global Variable
    AveragePowerW = averagePower;

    sumInstPower= 0;
    counter = 0;
  }

  // Sum the instantaneous power
  sumInstPower += instPower;

  counter++;

  return averagePower;
}

int32_t Calc_Vrms (int32_t instVoltage, bool risingEdgeDetected)
{
  // Scale down the raw voltage by 10 to avoid overflow
  int32_t instVoltsScaledDown = FixedPoint_Divide(instVoltage, 10<<16);

  // stores the Vrms from the previous cycle
  static uint32_t oldVrms = 1<<16;

  // Iteration number for square root algorithm
  uint8_t interationNb;

  // The number of samples collected in the current cycle
  static uint8_t sampleCount = 0;

  // sum of squared voltage
  static int32_t sumSquaredVolts = 0;

  // if rising edge is detected, it means the start of a new cycle
  if (risingEdgeDetected)
  {
    // Divide the sum of squared voltage by the number of samples in current cycle
    int32_t ratio = FixedPoint_Divide(sumSquaredVolts, sampleCount << 16);

    // Take the square root of the ratio
    // if the previous Vrms is 1 (first cycle), run the iteration 15 times
    if (oldVrms == 1<<16)
      interationNb = 15;
    else
      interationNb = 1;

    //initial guess for the next cycle is the current Vrms
    oldVrms = FixedPoint_SquareRoot (ratio, oldVrms, interationNb);

    // Load the global variable after scalling up by 10
    Vrms = (uint32_t) FixedPoint_Multiply(oldVrms, 10<<16);

    //reset the counter
    sampleCount = 0;
    //reset the summation
    sumSquaredVolts = 0;
  }

  // accumulate the squared voltage after every sample
  sumSquaredVolts += FixedPoint_Multiply(instVoltsScaledDown, instVoltsScaledDown);

  // increment the sampleCount;
  sampleCount++;

  return oldVrms;
}

int32_t Calc_Irms (int32_t instCurrent, bool risingEdgeDetected)
{
  // Scale up the raw current by 10 to increase precision
  int32_t instCurrentScaledUp = FixedPoint_Multiply(instCurrent, 10<<16);

  // stores the Irms from the previous cycle
  static uint32_t oldIrms = 1<<16;

  // Iteration number for square root algorithm
  uint8_t interationNb;

  // The number of samples collected in the current cycle
  static uint8_t sampleCount = 0;

  // sum of squared currents
  static int32_t sumSquaredCurrents = 0;

  // if rising edge is detected, it means the start of a new cycle
  if (risingEdgeDetected)
  {
    // Divide the sum of squared current by the number of samples in current cycle
    int32_t ratio = FixedPoint_Divide(sumSquaredCurrents, sampleCount << 16);

    // Take the square root of the ratio
    // if the previous Irms is 1 (first cycle), run the iteration 15 times
    if (oldIrms == 1<<16)
      interationNb = 15;
    else
      interationNb = 1;

    //initial guess for the next cycle is the current Irms
    oldIrms = FixedPoint_SquareRoot (ratio, oldIrms, interationNb);

    // Load the global variable with the scaled down value
    Irms = (uint32_t) FixedPoint_Divide(oldIrms, 10<<16);

    //reset the counter
    sampleCount = 0;
    //reset the summation
    sumSquaredCurrents = 0;
  }

  // accumulate the squared voltage after every sample
  sumSquaredCurrents += FixedPoint_Multiply(instCurrentScaledUp, instCurrentScaledUp);

  // increment the sampleCount;
  sampleCount++;

  return oldIrms;
}

void Calc_PowerFactor (int32_t vRMS, int32_t iRMS, int32_t avgPower)
{
  uint32_t vRMSiRMS = FixedPoint_Multiply(Vrms, Irms);

  if (vRMSiRMS != 0)
  {
    int32_t powerFactor = FixedPoint_Divide(AveragePowerW, vRMSiRMS);

    PowerFactor = (uint32_t) powerFactor;
  }
}

bool Calc_FrequencyTracking (int32_t sample, uint32_t* samplePeriod)
{
  static bool firstSampleCaptured = false;  // flag to indicate if the first sample has been captured

  static bool startCounter = false;         // flag to turn on or off the counter
  static bool resetCounter = true;          // flag to reset the counter

  static uint8_t periodSampleCount = 0;     // The number of samples captured for the current cycle

  static int32_t sampleOld;                 // variable to store the last sample from the window

  static uint32_t oldSamplePeriod = 1315790;  // variable to store the sample rate of the current samples
  uint32_t newSamplePeriod;                   // Variable to store the sample rate of the next samples

  static uint32_t zeroCrossing, firstZeroCrossing, secondZeroCrossing;

  bool risingEdgeDetected = false;                // flag to indicate if the sample is the first one from a new cycle

  // return the current sample period
  // the current sample period is the oldSample Period
  *samplePeriod = oldSamplePeriod;

  if (firstSampleCaptured)
  {
    // Start frequency tracking if a positive edge is detected
    // Rising is detected if the old sample value is negative
    // and current sample value is positive or zero
    if (sampleOld < 0 && sample >= 0)
    {
      // start the counter if a positive edge detected
      startCounter = true;

      //Interpolate the first zeroCrossing from left to the right
      // x = (+)a/(+)a-(-)b * 1000 (multiply by 1000 to avoid floating point)
      zeroCrossing = ((int32_t)sample * 1000) / ((int32_t) sample - (int32_t) sampleOld);

      // reset the counter when the rising edge of a new cycle is detected
      resetCounter = !resetCounter;

      // set the flag to indicate beginning of a new cycle
      risingEdgeDetected = true;
    }

    if (startCounter)
    {
      // Counter is reset when rising edge of new cycle is detected
      if (resetCounter)
      {
        // clear the reset flag
        resetCounter = false;

        // Interpolate the second zeroCrossing
        secondZeroCrossing = 1000 - zeroCrossing;

        // T0 = Period_n x Ts
        // frequency = 1 / T0
        // new Ts = T0 / 16 = (Ts / 16) * Period_n
        newSamplePeriod =  (oldSamplePeriod >> 4) * periodSampleCount;

        // add the interpolated parts
        if (firstZeroCrossing <= 1000  && secondZeroCrossing <= 1000)
          newSamplePeriod += ((firstZeroCrossing + secondZeroCrossing) * (oldSamplePeriod >> 16)) / 1000;

         PIT_Set (newSamplePeriod, true);

         // Calculate the frequency
         // freq = 10e9 / (16 * new Ts (in nS))
         // freqTimes10 = freq * 10
         FrequencyTimes10 = 1e9 / (16 * (newSamplePeriod/10));

         oldSamplePeriod = newSamplePeriod;

         //Packet_Put(0, periodSampleCount, 0, 0);

        // reset the counter to zero
        periodSampleCount = 0;
      }

      if (zeroCrossing <= 1000)
        // set the first ZeroCrossing
        firstZeroCrossing = zeroCrossing;

      // Increment the sample count
      periodSampleCount++;
    }
  }

  // Set flag to indicate that first sample has been captured
  // Start frequency tracking from second sample
  firstSampleCaptured = true;

  // store the current sample, which is the old sample for next call
  sampleOld = sample;

  return risingEdgeDetected;
}

static void Calc_CalculationThread (void* pData)
{
  OS_ERROR error;

  static uint8_t sampleNb = 0;

  // Instantaneous voltage, current and power
  int32_t instVoltage, instCurrent, instPower;

  int32_t vRMS, iRMS, avgPower;

  uint32_t samplePeriod;

  uint32_t energyPerCycleWs;

  // flag to indicate if the sample is from a new cycle
  bool risingEdgeDetected;

  for (;;)
  {
    // Wait for analog data to be captured
    error = OS_SemaphoreWait(AnalogGetSemaphore, 0);

    if (error)
      PE_DEBUGHALT();

    /* Convert raw ADC output to voltage and current in 32Q16 notation */
    int32_t volts32Q16 = Calc_ConvertADCtoVolts (Voltage_ADC[sampleNb]);

    instVoltage = FixedPoint_Multiply(volts32Q16, VOLTAGE_RAW_ADC_RATIO_32Q16);

    volts32Q16 = Calc_ConvertADCtoVolts (Current_ADC[sampleNb]);

    instCurrent = FixedPoint_Multiply(volts32Q16, CURRENT_RAW_ADC_RATIO_32Q16);

    // Calculate Instantaneous Power
    instPower = FixedPoint_Multiply(instVoltage, instCurrent);

    risingEdgeDetected = Calc_FrequencyTracking (instVoltage, &samplePeriod);

    avgPower = Calc_AveragePower (instPower, risingEdgeDetected);

    energyPerCycleWs = Calc_TotalEnergy(instPower, samplePeriod, risingEdgeDetected);

    // Calculate cost every cycle
    if (risingEdgeDetected)
      Calc_TotalCost (energyPerCycleWs);

    vRMS = Calc_Vrms (instVoltage, risingEdgeDetected);

    iRMS = Calc_Irms (instCurrent, risingEdgeDetected);

    if (vRMS != 0 && iRMS !=0)
      Calc_PowerFactor (vRMS, iRMS, avgPower);

    // Increment Sample Number
    sampleNb++;

    // Wrap pointer if necessary
    if (sampleNb == ANALOG_WINDOW_SIZE)
      sampleNb = 0;
  }
}
