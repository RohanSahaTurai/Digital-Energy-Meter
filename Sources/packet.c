/*! @file  packet.c
 *
 *  @brief Routines to implement packet encoding and decoding for the serial port.
 *
 *  This contains the functions for implementing the "Tower to PC Protocol" 5-byte packets.
 *
 *  @author Rohan, Jack
 *  @date 2019-08-12
 */

// Packet Module - contains packet initialization, get and put routines
#include "packet.h"

#define PACKET_BUFFER_SIZE 5	/*!<  How many bytes can be held by the packet buffer */

//Packet Structure Variable
TPacket Packet;

//Global Semaphore for packet_put
static OS_ECB *PacketPutSemaphore;

/*! @brief Initializes the packets by calling the initialization routines of the supporting software modules.
 *
 *  @param baudRate The desired baud rate in bits/sec.
 *  @param moduleClk The module clock rate in Hz.
 *  @return bool - TRUE if the packet module was successfully initialized.
 */
bool Packet_Init(const uint32_t baudRate, const uint32_t moduleClk)
{
  PacketPutSemaphore = OS_SemaphoreCreate(1);

  return UART_Init (baudRate, moduleClk);
}

/*! @brief Attempts to get a packet from the received data.
 *
 *  @return bool - TRUE if a valid packet was received.
 */
bool Packet_Get(void)
{
  // state represents how many bytes are currently stored in the packet buffer
  static uint8_t state = 0;

  // packet buffer is an array to hold 5 bytes to be returned to main
  static uint8_t packetBuffer [PACKET_BUFFER_SIZE];

  // Variable for the checksum to be calculated
  uint8_t checksum;

  // stores the status if the current packet is valid
  bool validPacket = false;

  //Finite State Machine to build valid packet
  while(!validPacket)
    {
      validPacket = false;

      // Switch case decides where the next byte should go in the packet buffer, determined by the state
      switch (state)
      {
	// If there are no bytes currently in the buffer, place the byte in the first position
	case 0:

	  if (!UART_InChar(packetBuffer)) // If there are no bytes in the RxFIFO to be received, return to HandlePackets
	    return false;

	  // If the byte is stored successfully in the buffer, proceed to state 1
	  state = 1;
	  break;

	case 1:

	  if (!UART_InChar(packetBuffer + 1))
	    return false;

	  state = 2;
	  break;

	case 2:

	  if (!UART_InChar(packetBuffer + 2))
	    return false;

	  state = 3;
	  break;

	case 3:

	  if (!UART_InChar(packetBuffer + 3))
	    return false;

	  state = 4;
	  break;

	case 4:

	  if (!UART_InChar(packetBuffer + 4))
	    return false;

	  state = 5;
	  break;

	case 5:
	  //Make checksum = 0 at the start, because 0^x=x
	  checksum = 0;

	  //index for the for-loop
	  uint8_t index;

	  //Calculate the checksum of the first four bytes in the packet buffer
	  for (index = 0; index < 4; index ++)
	  {
	    checksum ^= packetBuffer[index];
	  }

	  //Check if the checksum is equal to the fifth byte
	  if (checksum == packetBuffer[4])
	  {
	    //if checksum matches, return to state = 0
	    state = 0;

	    //valid packet is received as the checksum matches
	    validPacket = true;
	  }

	  //The checksum doesn't match
	  else
	  {
	    //slide the last three bytes to the left
	    for (index = 0; index < 4; index++)
	    {
	      packetBuffer[index] = packetBuffer[index+1];
	    }

	    //Return to state 4 to retrieve one byte from the RxFIFO
	    state = 4;
	  }
	  break;

	default:

	   state = 0;
	   break;

      }	// switch statement ends
    }  // while loop ends

  //populate the global variables with the valid packet bytes
  Packet_Command 	= packetBuffer[0];
  Packet_Parameter1 	= packetBuffer[1];
  Packet_Parameter2 	= packetBuffer[2];
  Packet_Parameter3 	= packetBuffer[3];
  Packet_Checksum 	= packetBuffer[4];

  return true;
}

/*! @brief Builds a packet and places it in the transmit FIFO buffer.
 *
 *  @return bool - TRUE if a valid packet was sent.
 */
bool Packet_Put(const uint8_t command, const uint8_t parameter1, const uint8_t parameter2, const uint8_t parameter3)
{
  OS_ERROR error;

  error = OS_SemaphoreWait(PacketPutSemaphore, 0);

  if (error)
    PE_DEBUGHALT();

  uint8_t checksum;

  // Calculate checksum by XOR-ing the first four packet bytes
  checksum = command ^ parameter1 ^ parameter2 ^ parameter3;

  uint8_t packetBuffer [PACKET_BUFFER_SIZE];

  packetBuffer[0] = command;
  packetBuffer[1] = parameter1;
  packetBuffer[2] = parameter2;
  packetBuffer[3] = parameter3;
  packetBuffer[4] = checksum;

  uint8_t index; //index for the for-loop

  for (index = 0; index < PACKET_BUFFER_SIZE; index ++)
    if (!(UART_OutChar(packetBuffer[index])))
    {
      //OS_EnableInterrupts();
       error = OS_SemaphoreSignal(PacketPutSemaphore);

       if (error)
	 PE_DEBUGHALT();

       return false;
    }

  error = OS_SemaphoreSignal(PacketPutSemaphore);

  if (error)
    PE_DEBUGHALT();

  return true;
}

