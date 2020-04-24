/*! @file FIFO.h
 *
 *  @brief Routines to implement a FIFO buffer.
 *
 *  This contains the structure and "methods" for accessing a byte-wide FIFO.
 *
 *  @author PMcL
 *  @date 2015-07-23
 */
/*! @addtogroup FIFO_Module FIFO module documentation
 *  @{
 *
 *  This contains the structure and "methods" for accessing a byte-wide FIFO.
 *
 */

#ifndef FIFO_H
#define FIFO_H

// new types
#include "types.h"
#include "CPU.h"
#include "OS.h"

#define FIFO_SIZE 256		/*!< The size of the FIFO used for storing the receiving and transmitting packets */

/*!
 * @struct TFIFO
 */
typedef struct
{
  uint16_t Start;			/*!< The index of the position of the oldest data in the FIFO */
  uint16_t End; 			/*!< The index of the next available empty position in the FIFO */
  uint8_t Buffer[FIFO_SIZE];	/*!< The actual array of bytes to store the data */
  OS_ECB *NbBytesSemaphore;		/*!< Counter Semaphore that keeps count of the current number of bytes stores */
  OS_ECB *NbBytesAvailableSemaphore;	/*!< /*!< Counter Semaphore that keeps count of the empty spaces available */
} TFIFO;

/*! @brief Initialize the FIFO before first use.
 *
 *  @param fifo A pointer to the FIFO that needs initializing.
 *  @return bool - TRUE if the FIFO was successfully initialised
 */
bool FIFO_Init(TFIFO * const fifo);

/*! @brief Put one character into the FIFO.
 *
 *  @param fifo A pointer to a FIFO struct where data is to be stored.
 *  @param data A byte of data to store in the FIFO buffer.
 *  @return bool - TRUE if data is successfully stored in the FIFO.
 *  @note Assumes that FIFO_Init has been called.
 */
bool FIFO_Put(TFIFO * const fifo, const uint8_t data);

/*! @brief Get one character from the FIFO.
 *
 *  @param fifo A pointer to a FIFO struct with data to be retrieved.
 *  @param dataPtr A pointer to a memory location to place the retrieved byte.
 *  @return bool - TRUE if data is successfully retrieved from the FIFO.
 *  @note Assumes that FIFO_Init has been called.
 */
bool FIFO_Get(TFIFO * const fifo, uint8_t * const dataPtr);

#endif

/*!
 * @}
 */

