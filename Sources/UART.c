/*! @file  UART.c
 *
 *  @brief I/O routines for UART communications on the TWR-K70F120M.
 *
 *  This contains the functions for operating the UART (serial port).
 *
 *  @author Rohan, Jack
 *  @date 2019-08-12
 */


#include "UART.h"

#define THREAD_STACK_SIZE 100

static TFIFO TxFIFO, RxFIFO;	/*!< private global variables for TxFIFO and RxFIFO */

// Thread Prototypes
static void TransmitThread (void *pData);
static void ReceiveThread (void *pData);

// Extern declared thread priorities
extern const uint8_t RECEIVE_THREAD_PRIORITY;
extern const uint8_t TRANSMIT_THREAD_PRIORITY;

// Global Semaphores
OS_ECB *ReceiveSemaphore;
OS_ECB *TransmitSemaphore;

static uint8_t ReceiveData;

// Thread Stack
static uint32_t ReceiveThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned (0x08)));
static uint32_t TransmitThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned (0x08)));

/*! @brief Sets up the UART interface before first use.
 *
 *  @param baudRate The desired baud rate in bits/sec.
 *  @param moduleClk The module clock rate in Hz.
 *  @return bool - TRUE if the UART was successfully initialized.
 */
bool UART_Init(const uint32_t baudRate, const uint32_t moduleClk)
{
  OS_ERROR error;

  //Enable the UART2 clock gate
  SIM_SCGC4 |= SIM_SCGC4_UART2_MASK;

  //Enable PORT E for pin routing
  SIM_SCGC5 |= SIM_SCGC5_PORTE_MASK;

  //Multiplex PORT E for pin 16 and 17 for UART_Tx and UART_Rx
  PORTE_PCR16 |= PORT_PCR_MUX(3);
  PORTE_PCR17 |= PORT_PCR_MUX(3);

  //Transmitter and receiver are turned off before setting baud rate
  UART2_C2 &= ~ (UART_C2_TE_MASK);
  UART2_C2 &= ~ (UART_C2_RE_MASK);

  //Baud rate generation routine
  uint16_t clkBaudRatio;

  uint16union_t localSBR;
  uint16_t localBRFA;

  clkBaudRatio = (moduleClk* 32/(16*baudRate));

  localSBR.l = clkBaudRatio/32;
  localBRFA = clkBaudRatio % 32;

  //Set the Baud Rate
  UART2_BDH = UART_BDH_SBR(localSBR.s.Hi);
  UART2_BDL = UART_BDL_SBR(localSBR.s.Lo);
  UART2_C4 = UART_C4_BRFA(localBRFA);

  // Enable the UART Receive interrupt
  UART2_C2 |= UART_C2_RIE_MASK;

  //Turn on Transmitter and Receiver
  UART2_C2 |=  UART_C2_TE_MASK;
  UART2_C2 |=  UART_C2_RE_MASK;

  // Initialize the NVIC
  // Clear the pending request for the interrupts
  NVICICPR1 = (1 << (49 % 32));
  // Enable interrupts from UART module
  NVICISER1 = (1 << (49 % 32));

  //Initialize the FIFO
  if (!(FIFO_Init(&TxFIFO) && FIFO_Init(&RxFIFO)))
    return false;

  // Create Semaphore
  ReceiveSemaphore = OS_SemaphoreCreate(0);
  TransmitSemaphore = OS_SemaphoreCreate(0);

  // Create Thread
  error = OS_ThreadCreate(ReceiveThread, // 2nd highest priority
			  NULL,
			  &ReceiveThreadStack[THREAD_STACK_SIZE - 1],
			  RECEIVE_THREAD_PRIORITY);

  if (error)
    PE_DEBUGHALT();

  error = OS_ThreadCreate(TransmitThread, // 3rd highest priority
			  NULL,
			  &TransmitThreadStack[THREAD_STACK_SIZE - 1],
			  TRANSMIT_THREAD_PRIORITY);

  if (error)
    PE_DEBUGHALT();

  return true;
}

/*! @brief Get a character from the receive FIFO if it is not empty.
 *
 *  @param dataPtr A pointer to memory to store the retrieved byte.
 *  @return bool - TRUE if the receive FIFO returned a character.
 *  @note Assumes that UART_Init has been called.
 */
bool UART_InChar(uint8_t* const dataPtr)
{
  OS_DisableInterrupts();

  bool success = FIFO_Get(&RxFIFO, dataPtr);

  OS_EnableInterrupts();

  return success;
}

/*! @brief Put a byte in the transmit FIFO if it is not full.
 *
 *  @param data The byte to be placed in the transmit FIFO.
 *  @return bool - TRUE if the data was placed in the transmit FIFO.
 *  @note Assumes that UART_Init has been called.
 */
bool UART_OutChar(const uint8_t data)
{
  OS_DisableInterrupts();

  bool success = FIFO_Put(&TxFIFO, data);

  // Enable the transmitter interrupt
  UART2_C2 |= UART_C2_TIE_MASK;

  OS_EnableInterrupts();

  return success;
}

/*! @brief Thread that receives a packet using UART
 *
 *  @param pData is not used but is needed for the RTOS
 *
 */
static void ReceiveThread (void *pData)
{
  for (;;)
  {
    OS_ERROR error;

    error = OS_SemaphoreWait(ReceiveSemaphore, 0);

    if (error)
      PE_DEBUGHALT();

    FIFO_Put (&RxFIFO, ReceiveData);
  }
}

/*! @brief Thread that sends a packet using UART
 *
 *  @param pData is not used but is needed for the RTOS
 *
 */
static void TransmitThread (void *pData)
{
  for (;;)
  {
    OS_ERROR error;

    error = OS_SemaphoreWait(TransmitSemaphore, 0);

    if (error)
       PE_DEBUGHALT();

    //stores data temporarily pulled from TxFIFO
    uint8_t sendData;

    if  (UART2_S1 & UART_S1_TDRE_MASK)
      // if the FIFO is not empty, transmit the data
      if (FIFO_Get (&TxFIFO, &sendData))
      {
        UART2_D = sendData;

        UART2_C2 |= UART_C2_TIE_MASK;
      }
  }
}


/*! @brief Interrupt service routine for the UART.
 *
 *  @note Assumes the transmit and receive FIFOs have been initialized.
 */
void __attribute__ ((interrupt)) UART_ISR(void)
{
  OS_ISREnter();

  OS_ERROR error;

  // Receive interrupts are always on - just check the flag
  if (UART2_S1 & UART_S1_RDRF_MASK)
  {
    ReceiveData = (uint8_t) UART2_D;

    error = OS_SemaphoreSignal(ReceiveSemaphore);

    if (error)
      PE_DEBUGHALT();
  }

  // Only respond to transmit interrupts if enabled
  if (UART2_C2 & UART_C2_TIE_MASK)
  {
    // if TDRE is set, get data from TxFIFO and put in UART2_D
    if (UART2_S1 & UART_S1_TDRE_MASK)
    {
      // TDRE flag is cleared by reading the status register

      UART2_C2 &= ~UART_C2_TIE_MASK;

      error = OS_SemaphoreSignal(TransmitSemaphore);

      if (error)
        PE_DEBUGHALT();
    }
  }

  OS_ISRExit();

}
