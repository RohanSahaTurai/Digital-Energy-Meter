/*! @file FixedPoint.h
 *
 *  @brief Routines to perform 32Q16 arithmetic
 *
 *  This contains functions to perform multiplication, division, conversion and square root
 *  of 32Q16 fixed point numbers
 *
 *  @author Rohan
 *  @date 2019-11-05
 */

#ifndef SOURCES_FIXEDPOINT_H_
#define SOURCES_FIXEDPOINT_H_

#include "types.h"
#include "CPU.h"

/*! @brief Converts a decimal to 32Q16 notation
 *  @param integer - the integer number to be converted
 *
 *  @return int32Q6 - converted number
 */
int32_t FixedPoint_Convert32Q16 (int16_t integer);

/*! @brief Multiplies two signed 32Q16 numbers
 *  @param num1 - one of the multiplicands
 *  @param num2 - one of the two multiplicands
 *
 *  @return int32Q6 - result
 */
int32_t FixedPoint_Multiply (int32_t num1, int32_t num2);

/*! @brief Multiplies two unsigned signed 32Q16 numbers
 *  @param num1 - one of the multiplicands
 *  @param num2 - one of the two multiplicands
 *
 *  @return uint32Q6 - result
 */
uint32_t FixedPoint_MultiplyU (uint32_t num1, int32_t num2);

/*! @brief Divides two signed 32Q16 numbers
 *  @param dividend
 *  @param divisor
 *
 *  @return int32Q6 - converted number
 */
int32_t FixedPoint_Divide (int32_t dividend, int32_t divisor);

/*! @brief Divides two unsigned 32Q16 numbers
 *  @param dividend
 *  @param divisor
 *
 *  @return int32Q6 - converted number
 */
uint32_t FixedPoint_DivideU (uint32_t dividend, uint32_t divisor);

/*! @brief Calculates the square root of a number using Newton's Method
 *  @param radicand - the number whose square root is to be calculated
 *  @param initialGuess - initial estimate
 *  @param nInteration - number of interations to be performed
 *
 *  @return int32Q6 - result
 */
int32_t FixedPoint_SquareRoot (int32_t radicand, int32_t initialGuess, uint8_t nIteration);

#endif /* SOURCES_FIXEDPOINT_H_ */
