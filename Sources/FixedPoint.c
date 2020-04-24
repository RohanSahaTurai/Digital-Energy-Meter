/*! @file Calc.h
 *
 *  @brief Routines to calculate measurements for the DEM
 *
 *  This contains the tariff rate and functions to calculate measurements for the DEM
 *
 *  @author Rohan
 *  @date 2019-11-05
 */


#include "FixedPoint.h"

inline int32_t FixedPoint_Convert32Q16 (int16_t integer)
{
  return (int32_t) integer << 16;
}

inline int32_t FixedPoint_Multiply (int32_t num1, int32_t num2)
{
  int64_t product64bit = (int64_t)num1 * (int64_t)num2;

  int32_t product32bit = (product64bit >> 16);

  return product32bit;
}

inline uint32_t FixedPoint_MultiplyU (uint32_t num1, int32_t num2)
{
  uint64_t product64bit = (uint64_t)num1 * (uint64_t)num2;

  uint32_t product32bit = (product64bit >> 16);

  return product32bit;
}

inline int32_t FixedPoint_Divide (int32_t dividend, int32_t divisor)
{
  if (divisor == 0)
    PE_DEBUGHALT();

  int32_t quotient = ((int64_t) dividend << 16) / divisor;

  return quotient;
}

inline uint32_t FixedPoint_DivideU (uint32_t dividend, uint32_t divisor)
{
  if (divisor == 0)
    PE_DEBUGHALT();

  uint32_t quotient = ((uint64_t) dividend << 16) / divisor;

  return quotient;
}

int32_t FixedPoint_SquareRoot (int32_t radicand, int32_t initialGuess, uint8_t nIteration)
{
  int32_t xN = initialGuess;

  uint8_t n;

  for (n = 0; n < nIteration; n++)
    xN = (FixedPoint_Divide(radicand, xN) + xN) >> 1;

  return xN;
}
