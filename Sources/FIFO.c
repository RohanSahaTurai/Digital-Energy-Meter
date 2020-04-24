/*! @file FIFO.c
 *
 *  @brief Routines to implement a FIFO buffer.
 *
 *  This contains the structure and "methods" for accessing a byte-wide FIFO.
 *
 *  @author Jack, Rohan
 *  @date 2019-08-12
 */

#include "FIFO.h"

/*! @brief Initialize the FIFO before first use.
 *
 *  @param fifo A pointer to the FIFO that needs initializing.
 *  @return bool - TRUE if the FIFO was successfully initialised
 */
bool FIFO_Init(TFIFO * const fifo)
{
  //point the Start and End at index 0
  fifo->Start = fifo->End = 0;

  /* Create semaphores */
  fifo->NbBytesSemaphore = OS_SemaphoreCreate(0);	//initially the fifo is empty

  //initially the number of spaces available is the size of the FIFO
  fifo->NbBytesAvailableSemaphore = OS_SemaphoreCreate(FIFO_SIZE);

  return true;
}

/*! @brief Put one character into the FIFO.
 *
 *  @param fifo A pointer to a FIFO struct where data is to be stored.
 *  @param data A byte of data to store in the FIFO buffer.
 *  @return bool - TRUE if data is successfully stored in the FIFO.
 *  @note Assumes that FIFO_Init has been called.
 */
bool FIFO_Put(TFIFO * const fifo, const uint8_t data)
{
  OS_ERROR error;

  // Check if the FIFO is full
  error = OS_SemaphoreWait(fifo->NbBytesAvailableSemaphore, 0);

  if (error)
    PE_DEBUGHALT();

  /* if space available, proceed */
  OS_DisableInterrupts();

  //check for NULL pointer
  if (!fifo)
  {
    OS_EnableInterrupts();
    return false;
  }

  fifo->Buffer[fifo->End]= data;

  // wrap FIFO if necessary
  if ((fifo->End + 1) == FIFO_SIZE )
 	fifo->End = 0;

  else
    fifo->End++;

  //Increment the current number of bytes stored
  error = OS_SemaphoreSignal(fifo->NbBytesSemaphore);

  if (error)
    PE_DEBUGHALT();

  OS_EnableInterrupts();

  return true;
}

/*! @brief Get one character from the FIFO.
 *
 *  @param fifo A pointer to a FIFO struct with data to be retrieved.
 *  @param dataPtr A pointer to a memory location to place the retrieved byte.
 *  @return bool - TRUE if data is successfully retrieved from the FIFO.
 *  @note Assumes that FIFO_Init has been called.
 */
bool FIFO_Get(TFIFO * const fifo, uint8_t * const dataPtr)
{
  OS_ERROR error;

  // Check if FIFO is empty
  error = OS_SemaphoreWait(fifo->NbBytesSemaphore, 0);

  if (error)
    PE_DEBUGHALT();

  /* if at least one element stored, then proceed */

  OS_DisableInterrupts();

  if (!fifo)
  {
    OS_EnableInterrupts();
    return false;
  }

  //load the FIFO data in the memory address pointed by dataPtr
  *dataPtr = fifo->Buffer[fifo->Start];

  //Wrap Pointer if necessary
  if ((fifo->Start+1)==FIFO_SIZE)
    fifo->Start = 0;

  else
    fifo->Start++;

  // Increment the current number of spaces available
  error = OS_SemaphoreSignal(fifo->NbBytesAvailableSemaphore);

  if (error)
    PE_DEBUGHALT();

  OS_EnableInterrupts();

  return true;
}
