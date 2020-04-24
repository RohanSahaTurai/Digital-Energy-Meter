/*! @file  Flash.c
 *
 *  @brief Routines for erasing and writing to the Flash.
 *
 *  This contains the functions needed for accessing the internal Flash.
 *
 *  @author Rohan, Jack
 *  @date 2019-08-26
 */

#include "Flash.h"
#include "MK70F12.h"

#define CMD_FLASH_PROGRAM 	0x07u
#define CMD_FLASH_ERASE_SECTOR 	0X09u

// FCCOB registers 0-B represented as a struct
typedef struct {
  uint8_t command;
  uint8_t address23_16;
  uint8_t address15_8;
  uint8_t address7_0;
  uint8_t data0;
  uint8_t data1;
  uint8_t data2;
  uint8_t data3;
  uint8_t data4;
  uint8_t data5;
  uint8_t data6;
  uint8_t data7;
} TFCCOB;

bool Flash_Init()
{
  //Enable the Flash clock gate
  return true;
}
 
/*! @brief Tells the flash what command it should execute, with specific paramaters
 *
 *  @param commonCommandObject is a structure containing the command, address, and data to 
 *  to be writen to the FCCOB registers, which then executes the command
 *  @return bool - TRUE if the registers were accessed successfully
 *  @note Assumes Flash has been initialized.
 */
static bool LaunchCommand(TFCCOB* commonCommandObject)
{
  //NULL check
  if (!commonCommandObject)
    return false;

  //Wait for previous command to complete
  while (!(FTFE_FSTAT & FTFE_FSTAT_CCIF_MASK));

  //Check for access error and protection violations, and clear the errors
  if (FTFE_FSTAT & FTFE_FSTAT_ACCERR_MASK || FTFE_FSTAT & FTFE_FSTAT_FPVIOL_MASK)
    FTFE_FSTAT = 0x30;

  //Store command and parameters in the FCCOB registers to be executed
  FTFE_FCCOB0 = commonCommandObject->command;
  FTFE_FCCOB1 = commonCommandObject->address23_16;
  FTFE_FCCOB2 = commonCommandObject->address15_8;
  FTFE_FCCOB3 = commonCommandObject->address7_0;
  FTFE_FCCOB4 = commonCommandObject->data0;
  FTFE_FCCOB5 = commonCommandObject->data1;
  FTFE_FCCOB6 = commonCommandObject->data2;
  FTFE_FCCOB7 = commonCommandObject->data3;
  FTFE_FCCOB8 = commonCommandObject->data4;
  FTFE_FCCOB9 = commonCommandObject->data5;
  FTFE_FCCOBA = commonCommandObject->data6;
  FTFE_FCCOBB = commonCommandObject->data7;

  // Execute command; write 1 to clear
  FTFE_FSTAT = FTFE_FSTAT_CCIF_MASK;

  // Wait for command to finish executing
  while (!(FTFE_FSTAT & FTFE_FSTAT_CCIF_MASK));

  return true;  
}

/*! @brief Erases a sector of flash memory, allowing it to be written to
 *
 *  @param address tells the function which sector it should clear
 *  @return bool - TRUE if the sector is cleared successfully
 *  @note Assumes Flash has been initialized.
 */
static bool EraseSector(const uint32_t address) 
{
  //Create structure to store command and parameters
  TFCCOB eraseSector;

  eraseSector.command = CMD_FLASH_ERASE_SECTOR;
  eraseSector.address23_16 = (uint8_t) (((uint32_t) address) >> 16);
  eraseSector.address15_8 = (uint8_t) (((uint32_t) address) >> 8);
  eraseSector.address7_0 = (uint8_t) (((uint32_t) address) >> 0);

  return LaunchCommand(&eraseSector);
}

/*! @brief Used to write a phrase to the first 8 bytes of a sector
 *
 *  @param address is the address of the start of the sector to be written to
 *  @param phrase is the 64 bits of data to be written to the sector
 *  @return bool - TRUE if the data was written successfully
 *  @note Assumes Flash has been initialized.
 */
static bool WritePhrase(const uint32_t address, const uint64union_t phrase)
{
  //Create structure for the command and parameters
  TFCCOB writeSector;

  //Store data big-endian
  writeSector.command = CMD_FLASH_PROGRAM;
  writeSector.address23_16 = (uint8_t) (((uint32_t) address) >> 16);
  writeSector.address15_8  = (uint8_t) (((uint32_t) address) >> 8);
  writeSector.address7_0   = (uint8_t) (((uint32_t) address) >> 0);
  writeSector.data0 = (uint8_t) (((uint32_t) phrase.s.Lo) >> 24);
  writeSector.data1 = (uint8_t) (((uint32_t) phrase.s.Lo) >> 16);
  writeSector.data2 = (uint8_t) (((uint32_t) phrase.s.Lo) >> 8);
  writeSector.data3 = (uint8_t) (((uint32_t) phrase.s.Lo) >> 0);
  writeSector.data4 = (uint8_t) (((uint32_t) phrase.s.Hi) >> 24);
  writeSector.data5 = (uint8_t) (((uint32_t) phrase.s.Hi) >> 16);
  writeSector.data6 = (uint8_t) (((uint32_t) phrase.s.Hi) >> 8);
  writeSector.data7 = (uint8_t) (((uint32_t) phrase.s.Hi) >> 0);

  //Execute the command
  return LaunchCommand(&writeSector);
}

/*! @brief Allows the first 8 bytes of a sector to be modified, by clearing it and then writing back to it
 *
 *  @param address is the address of the sector to be modified
 *  @param phrase is the new set of data to be written to the sector
 *  @return bool - TRUE if the phrase was modified successfully
 *  @note Assumes Flash has been initialized.
 */
static bool ModifyPhrase(const uint32_t address, const uint64union_t phrase)
{
  //Erase the sector to be modified
  if (!EraseSector(address))
    return false;
  
  //Write the new data to the flash
  return WritePhrase(address, phrase);
}

/*! @brief Allocates space for a non-volatile variable in the Flash memory.
 *
 *  @param variable is the address of a pointer to a variable that is to be allocated space in Flash memory.
 *         The pointer will be allocated to a relevant address:
 *         If the variable is a byte, then any address.
 *         If the variable is a half-word, then an even address.
 *         If the variable is a word, then an address divisible by 4.
 *         This allows the resulting variable to be used with the relevant Flash_Write function which assumes a certain memory address.
 *         e.g. a 16-bit variable will be on an even address
 *  @param size The size, in bytes, of the variable that is to be allocated space in the Flash memory. Valid values are 1, 2 and 4.
 *  @return bool - TRUE if the variable was allocated space in the Flash memory.
 *  @note Assumes Flash has been initialized.
 */
bool Flash_AllocateVar(volatile void** variable, const uint8_t size)
{
  // NULL check
  if (!variable)
    return false;

  //Index represents the 8 storage bytes like an array
  static uint8_t memoryIndex = 0x00;

  //Count used to check position
  uint8_t count;

  //Checking for how many bytes must be stored, only accounts for 1, 2 and 4 bytes of data
  switch (size)
  {
    //1 byte of data
    case 1:
      //For loop checks for the first empty space available
      for (count = 0; count < 8; count++)
      {
        //Check for empty space
        if (!(memoryIndex & ((uint8_t) 0x01 << count)))
        {
          //Allocate the address of the variable to the empty space
          *variable = (void *) FLASH_DATA_START + count;

          //Setting the memory space as allocated in our "array"
          memoryIndex |= (0x01 << count);

          return true;
        }
      }

      //If no space available, return false
      return false;

    //2 bytes of data
    case 2:

      for (count = 0; count < 8; count+=2)
      {
        //Check that there are two consecutive empty spaces in flash
        if (!(memoryIndex & ((uint8_t) 0x01 << count)) &&
            !(memoryIndex & ((uint8_t) 0x01 << count + 1)))
        {
          //Allocate the address of the variable to the first empty space
          *variable = (void *) FLASH_DATA_START + count;

          //Setting the memory spaces as allocated in our "array"
          memoryIndex |= (0x01 << count);
          memoryIndex |= (0x01 << count+1);

          return true;
        }
      }

      //If no spaces available, return false
      return false;

    //4 bytes of data
    case 4:

      for (count = 0; count < 8; count+=4)
      {
        //Check that there are 4 consecutive empty spaces in flash
        if (!(memoryIndex & ((uint8_t) 0x01 << count)) &&
            !(memoryIndex & ((uint8_t) 0x01 << count + 1)) &&
            !(memoryIndex & ((uint8_t) 0x01 << count + 2)) &&
            !(memoryIndex & ((uint8_t) 0x01 << count + 3)))
        {
          //Allocate the address of the variable to the first empty space
          *variable = (void *) FLASH_DATA_START + count;

          //Setting the memory spaces as allocated in our array
          memoryIndex |= (0x01 << count);
          memoryIndex |= (0x01 << count+1);
          memoryIndex |= (0x01 << count+2);
          memoryIndex |= (0x01 << count+3);

          return true;
        }
      }

      //If no spaces available, return false
      return false;

    default:
      //If any data not of sizes 1,2 or 4, return false
      return false;
  }
}

/*! @brief Writes a 32-bit number to Flash.
 *
 *  @param address The address of the data.
 *  @param data The 32-bit data to write.
 *  @return bool - TRUE if Flash was written successfully, FALSE if address is not aligned to a 4-byte boundary or if there is a programming error.
 *  @note Assumes Flash has been initialized.
 */
bool Flash_Write32(volatile uint32_t* const address, const uint32_t data)
{
  // Check if the address is null
  if (!address)
    return false;

  //Copying the existing phrase in a temporary phrase buffer
  uint64union_t phraseBuffer;

  uint32_t addressBuffer = (uint32_t) address;

  //If at address 0x00080004
  if (addressBuffer % 8 == 4)
  {
    addressBuffer -= 4;

    //Store the new data in the high 4 bytes, and the already existing flash data in the low 4 bytes
    phraseBuffer.s.Lo = _FW(addressBuffer);
    phraseBuffer.s.Hi = data;
  }

  else if (addressBuffer % 8 == 0) //At address 0x00080000
  {
    //Store the new data in the low 4 bytes, and the already existing flash data in the high 4 bytes
    phraseBuffer.s.Hi = _FW(addressBuffer + 4);
    phraseBuffer.s.Lo = data;
  }

  else			// If offset is anything other than 0 or 4, return false
    return false;


  //With new phrase, now erase flash and re-write phrase to flash
  return ModifyPhrase(addressBuffer, phraseBuffer);
}

/*! @brief Writes a 16-bit number to Flash.
 *
 *  @param address The address of the data.
 *  @param data The 16-bit data to write.
 *  @return bool - TRUE if Flash was written successfully, FALSE if address is not aligned to a 2-byte boundary or if there is a programming error.
 *  @note Assumes Flash has been initialized.
 */
bool Flash_Write16(volatile uint16_t* const address, const uint16_t data)
{
  // Check if the address is null
  if (!address)
    return false;

  uint32union_t wordBuffer;

  uint32_t addressBuffer = (uint32_t) address;

  //At address' 0x00080002 || 0x00080006
  if (addressBuffer % 4 == 2)
  {
    addressBuffer -= 2;

    //Store the new data in the high 2 bytes, and the already existing flash data in the low 2 bytes
    wordBuffer.s.Lo = _FH(addressBuffer);
    wordBuffer.s.Hi = data;
  }

  else if (addressBuffer % 4 == 0)  //At address' 0x00080000 || 0x00080004
  {
    //Store the new data in the low 2 bytes, and the already existing flash data in the high 2 bytes
    wordBuffer.s.Hi = _FH(addressBuffer + 2);
    wordBuffer.s.Lo = data;
  }

  else
    return false;	// If address is not at offset 0, 2, 4, 6 and 8, return false

  //Take new 32 bits of data and call write32 to continue copying data
  return Flash_Write32((uint32_t*) addressBuffer, wordBuffer.l);
}

/*! @brief Writes an 8-bit number to Flash.
 *
 *  @param address The address of the data.
 *  @param data The 8-bit data to write.
 *  @return bool - TRUE if Flash was written successfully, FALSE if there is a programming error.
 *  @note Assumes Flash has been initialized.
 */
bool Flash_Write8(volatile uint8_t* const address, const uint8_t data)
{
  // Check if the address is null
  if(!address)
    return false;

  uint16union_t halfWordBuffer;

  uint32_t addressBuffer = (uint32_t) address;

  //At address' 0x00080001 || 0x00080003 || 0x00080005 || 0x00080007
  if (addressBuffer % 2)
  {
    addressBuffer -= 1;

    //Store the new data in the high byte, and the already existing flash data in the low byte
    halfWordBuffer.s.Lo = _FB(addressBuffer);
    halfWordBuffer.s.Hi = data;
  }
  else  //At address' 0x00080000 || 0x00080002 || 0x00080004 || 0x00080006
  {
    //Store the new data in the low byte, and the already existing flash data in the high byte
    halfWordBuffer.s.Hi = _FB(addressBuffer + 1);
    halfWordBuffer.s.Lo = data;
  }

  //Take new 16 bits of data and call write16 to continue copying data
  return Flash_Write16((uint16_t*) addressBuffer, halfWordBuffer.l);
}

/*! @brief Erases the entire Flash sector.
 *
 *  @return bool - TRUE if the Flash "data" sector was erased successfully.
 *  @note Assumes Flash has been initialized.
 */
bool Flash_Erase(void)
{
  //Erase block 2 sector 0 at address 0x00080000
  return EraseSector(FLASH_DATA_START);
}
