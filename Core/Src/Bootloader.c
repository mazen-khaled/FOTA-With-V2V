/*
 * Bootloader.c
 *
 *  Created on: Sep 4, 2024
 *      Author: Mazen
 */
// Include the bootloader header file
#include "Bootloader.h"

// Declare static functions for various bootloader operations
static void BL_Get_Version(uint8_t *Host_buffer);
static void BL_Flash_Erase(uint8_t *Host_buffer);
static void BL_Write_Data(uint8_t *Host_buffer);

static uint32_t BL_CRC_verfiy(uint8_t * pdata, uint32_t DataLen, uint32_t HosrCRC); // Verify CRC of received data
static uint8_t Perform_Flash_Erase(uint32_t PageAddress, uint8_t page_Number);      // Perform flash memory erase
static uint8_t BL_Address_Varification(uint32_t Addresss);                          // Verify address validity
static uint8_t FlashMemory_Paylaod_Write(uint16_t * pdata, uint32_t StartAddress, uint8_t Payloadlen); // Write data to flash memory
static void BL_Send_ACK(uint8_t dataLen);                                           // Send acknowledgment to host
static void BL_Send_NACK();                                                         // Send negative acknowledgment to host
void Jump_Application();                                                            // Jump to main application

static void Bootloader_Send_To_Host(uint8_t *Host_buffer, uint32_t datalen);        // Send data to the host

// Buffer for receiving data from the host
static uint8_t Host_buffer[HOSTM_MAX_SIZE];

// Bootloader main function to process host commands
BL_status BL_FeatchHostCommand()
{
    BL_status status = BL_NACK;                            // Initialize status as NACK
    HAL_StatusTypeDef Hal_status = HAL_ERROR;             // Initialize HAL status as error
    uint8_t DataLen = 0;

    // Clear the host buffer
    memset(Host_buffer, 0, HOSTM_MAX_SIZE);

    // Receive the length of the incoming command
    Hal_status = HAL_UART_Receive(&huart2, Host_buffer, 1, HAL_MAX_DELAY);

    // If the first byte is 4, jump to the main application
    if (Host_buffer[0] == 4) {
        Jump_Application();
    }

    // Check if the UART receive operation was successful
    if (Hal_status != HAL_OK) {
        status = BL_NACK; // Set status to NACK on error
    } else {
        DataLen = Host_buffer[0]; // Get the data length

        // Receive the command data
        Hal_status = HAL_UART_Receive(&huart2, &Host_buffer[1], DataLen, HAL_MAX_DELAY);
        DataLen++; // Include the length byte

        if (Hal_status != HAL_OK) {
            status = BL_NACK; // Set status to NACK on error
        } else {
            // Process the received command based on the command byte
            switch (Host_buffer[1]) {
            case CBL_GET_VER_CMD: BL_Get_Version(Host_buffer); break;    // Get version command
            case CBL_GO_TO_ADDR_CMD: BL_SendMessage("jump to address"); break; // Jump to address command
            case CBL_FLASH_ERASE_CMD: BL_Flash_Erase(Host_buffer); break; // Flash erase command
            case CBL_MEM_WRITE_CMD: BL_Write_Data(Host_buffer); break;   // Write data command
            default: status = BL_NACK; // Default case for unsupported commands
            }
        }
    }
    return status;
}

// Send a formatted message over UART
void BL_SendMessage(char *format, ...)
{
    char message[100] = {0};
    va_list args;
    va_start(args, format);
    vsprintf(message, format, args);
    HAL_UART_Transmit(&huart2, (uint8_t *)message, sizeof(message), HAL_MAX_DELAY);
    va_end(args);
}

// Handle the "Get Version" command
static void BL_Get_Version(uint8_t *Host_buffer)
{
    uint8_t Veraion[4] = {CBL_VENDOR_ID, CBL_SW_MAJOR_VERSION, CBL_SW_MINOR_VERSION, CBL_SW_PATCH_VERSION};
    uint16_t Host_Packet_Len = 0;
    uint32_t CRC_valu = 0;

    // Calculate the packet length and extract the CRC value
    Host_Packet_Len = Host_buffer[0] + 1;
    CRC_valu = *(uint32_t *)(Host_buffer + Host_Packet_Len - 4);

    // Verify the CRC
    if (CRC_VERIFING_PASS == BL_CRC_verfiy((uint8_t *)&Host_buffer[0], Host_Packet_Len - 4, CRC_valu)) {
        BL_Send_ACK(4); // Send acknowledgment
        HAL_UART_Transmit(&huart2, (uint8_t *)Veraion, 4, HAL_MAX_DELAY); // Send version data
        Jump_Application(); // Jump to application
    } else {
        BL_Send_NACK(); // Send negative acknowledgment on CRC failure
    }
}

// Handle the "Flash Erase" command
static void BL_Flash_Erase(uint8_t *Host_buffer)
{
    BL_SendMessage("start erasing \r\n");
    uint8_t Erase_status = UNSUCCESSFUL_ERASE;
    uint16_t Host_Packet_Len = 0;
    uint32_t CRC_valu = 0;

    // Calculate packet length and extract CRC value
    Host_Packet_Len = Host_buffer[0] + 1;
    CRC_valu = *(uint32_t *)(Host_buffer + Host_Packet_Len - 4);

    // Verify CRC and perform flash erase
    if (CRC_VERIFING_PASS == BL_CRC_verfiy((uint8_t *)&Host_buffer[0], Host_Packet_Len - 4, CRC_valu)) {
        Erase_status = Perform_Flash_Erase(*((uint32_t *)&Host_buffer[2]), Host_buffer[6]);
    } else {
        BL_Send_NACK(); // Send NACK on CRC failure
    }
}

// Function to handle the process of writing data to flash memory
static void BL_Write_Data(uint8_t *Host_buffer)
{
    // Variables to store address validation, target address, data length, and payload write status
    uint8_t Adress_varfiy = ADDRESS_IS_INVALID;
    uint32_t Address_Host = 0;
    uint8_t DataLen = 0;
    uint8_t payload_status = FLASH_PAYLOAD_WRITE_FAILED;
    uint16_t Host_Packet_Len = 0;
    uint32_t CRC_valu = 0;

    // Extract the packet length from the host buffer
    Host_Packet_Len = Host_buffer[0] + 1;

    // Extract the CRC value from the packet
    CRC_valu = *(uint32_t *)(Host_buffer + Host_Packet_Len - 4);

    // Extract the address and data length from the host buffer
    Address_Host = *((uint32_t *)&Host_buffer[2]);
    DataLen = Host_buffer[6];

    // Verify the validity of the target address
    Adress_varfiy = BL_Address_Varification(Address_Host);

    if (Adress_varfiy == ADDRESS_IS_VALID)
    {
        // Write the payload to flash memory if the address is valid
        payload_status = FlashMemory_Paylaod_Write((uint16_t *)&Host_buffer[7], Address_Host, DataLen);
    }
    else
    {
        // Transmit the invalid address status if the address is not valid
        HAL_UART_Transmit(&huart2, (uint8_t *)&Adress_varfiy, 1, HAL_MAX_DELAY);
    }
}

// Function to verify the validity of a given memory address
static uint8_t BL_Address_Varification(uint32_t Addresss)
{
    uint8_t Adress_varfiy = ADDRESS_IS_INVALID;

    // Check if the address falls within valid flash memory range
    if (Addresss >= FLASH_BASE && Addresss <= STM32F103_FLASH_END)
    {
        Adress_varfiy = ADDRESS_IS_VALID;
    }
    // Check if the address falls within valid SRAM range
    else if (Addresss >= SRAM_BASE && Addresss <= STM32F103_SRAM_END)
    {
        Adress_varfiy = ADDRESS_IS_VALID;
    }
    else
    {
        Adress_varfiy = ADDRESS_IS_INVALID;
    }

    return Adress_varfiy;
}

// Function to verify the CRC of data received from the host
static uint32_t BL_CRC_verfiy(uint8_t *pdata, uint32_t DataLen, uint32_t HostCRC)
{
    uint8_t crc_status = CRC_VERIFING_FAILED;
    uint32_t MCU_CRC = 0;
    uint32_t dataBuffer = 0;

    // Accumulate CRC for the provided data
    for (uint8_t count = 0; count < DataLen; count++)
    {
        dataBuffer = (uint32_t)pdata[count];
        MCU_CRC = HAL_CRC_Accumulate(&hcrc, &dataBuffer, 1);
    }

    // Reset the CRC hardware
    __HAL_CRC_DR_RESET(&hcrc);

    // Compare the calculated CRC with the host-provided CRC
    if (HostCRC == MCU_CRC)
    {
        crc_status = CRC_VERIFING_PASS;
    }
    else
    {
        crc_status = CRC_VERIFING_FAILED;
    }

    return crc_status;
}

// Function to send an acknowledgment (ACK) response
static void BL_Send_ACK(uint8_t dataLen)
{
    uint8_t ACK_value[2] = {0};
    ACK_value[0] = SEND_ACK;
    ACK_value[1] = dataLen;

    // Transmit the ACK response via UART
    HAL_UART_Transmit(&huart2, (uint8_t *)ACK_value, 2, HAL_MAX_DELAY);
}

// Function to send a negative acknowledgment (NACK) response
static void BL_Send_NACK()
{
    uint8_t ACk_value = SEND_NACK;

    // Transmit the NACK response via UART
    HAL_UART_Transmit(&huart2, &ACk_value, sizeof(ACk_value), HAL_MAX_DELAY);
}

// Function to perform a flash erase operation
static uint8_t Perform_Flash_Erase(uint32_t PageAddress, uint8_t page_Number)
{
    FLASH_EraseInitTypeDef pEraseInit;
    HAL_StatusTypeDef Hal_status = HAL_ERROR;
    uint32_t PageError = 0;
    uint8_t PageStatus = INVALID_PAGE_NUMBER;

    // Validate the page number
    if (page_Number > CBL_FLASH_MAX_PAGE_NUMBER)
    {
        PageStatus = INVALID_PAGE_NUMBER;
    }
    else
    {
        PageStatus = VALID_PAGE_NUMBER;

        // Check if the erase request is within bounds or is a mass erase
        if (page_Number <= (CBL_FLASH_MAX_PAGE_NUMBER - 1) || PageAddress == CBL_FLASH_MASS_ERASE)
        {
            if (PageAddress == CBL_FLASH_MASS_ERASE)
            {
                // Set parameters for mass erase (WRONG: Placeholder comment)
                pEraseInit.TypeErase = FLASH_TYPEERASE_PAGES; /* FLASH_TYPEERASE_MASSERASE */
                pEraseInit.Banks = FLASH_BANK_1;
                pEraseInit.PageAddress = 0x8008000;
                pEraseInit.NbPages = 12;
            }
            else
            {
                // Set parameters for page erase
                pEraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
                pEraseInit.Banks = FLASH_BANK_1;
                pEraseInit.PageAddress = PageAddress;
                pEraseInit.NbPages = page_Number;
            }

            // Unlock flash memory and perform the erase operation
            HAL_FLASH_Unlock();
            Hal_status = HAL_FLASHEx_Erase(&pEraseInit, &PageError);
            HAL_FLASH_Lock();

            // Update status based on the erase result
            if (PageError == HAL_SUCCESSFUL_ERASE)
            {
                PageStatus = SUCCESSFUL_ERASE;
            }
            else
            {
                PageStatus = UNSUCCESSFUL_ERASE;
            }
        }
        else
        {
            PageStatus = INVALID_PAGE_NUMBER;
        }
    }

    return PageStatus;
}

// Function to write a payload of data into flash memory
static uint8_t FlashMemory_Paylaod_Write(uint16_t *pdata, uint32_t StartAddress, uint8_t Payloadlen)
{
    uint32_t Address = 0;  // Variable to hold the target address for writing
    uint8_t UpdataAdress = 0;  // Offset increment for the target address
    HAL_StatusTypeDef Hal_status = HAL_ERROR;  // Variable to store the status of flash programming
    uint8_t payload_status = FLASH_PAYLOAD_WRITE_FAILED;  // Default payload write status as failed

    // Unlock the flash memory for write/erase operations
    HAL_FLASH_Unlock();

    // Iterate through the payload data and write each half-word to the flash memory
    for (uint8_t payload_count = 0, UpdataAdress = 0; payload_count < Payloadlen / 2; 
         payload_count++, UpdataAdress += 2)
    {
        // Calculate the target address for the current half-word
        Address = StartAddress + UpdataAdress;

        // Program the current half-word into flash memory
        Hal_status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, Address, pdata[payload_count]);

        // Update the payload status based on the flash programming result
        if (Hal_status != HAL_OK)
        {
            payload_status = FLASH_PAYLOAD_WRITE_FAILED;
        }
        else
        {
            payload_status = FLASH_PAYLOAD_WRITE_PASSED;
        }
    }

    // Lock the flash memory to prevent unintended write/erase operations
    HAL_FLASH_Lock();

    // Return the status of the payload write operation
    return payload_status;
}

// Define the base address of flash sector 2
#define FLASH_SECTOR2_BASE_ADDRESS 0x08008000U

// Define a function pointer type for the application's reset handler
typedef void (*pMainApp)(void);

// Function to jump to the application starting from flash sector 2
void Jump_Application()
{
    // Read the Main Stack Pointer (MSP) value from the vector table in flash sector 2
    uint32_t MSP_Value = *((volatile uint32_t *)FLASH_SECTOR2_BASE_ADDRESS);

    // Read the address of the reset handler from the vector table in flash sector 2
    uint32_t MainAppAdd = *((volatile uint32_t *)(FLASH_SECTOR2_BASE_ADDRESS + 4));

    // Cast the reset handler address to a function pointer
    pMainApp ResetHandler_Address = (pMainApp)MainAppAdd;

    // De-initialize the RCC (Reset and Clock Control) to reset clock settings
    HAL_RCC_DeInit();

    // Set the MSP to the application's stack pointer value
    __set_MSP(MSP_Value);

    // Jump to the application's reset handler to begin execution
    ResetHandler_Address();
}
