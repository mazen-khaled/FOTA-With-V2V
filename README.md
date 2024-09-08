# FOTA with V2V Updates for STM32

## Introduction
This project aims to solve the software update challenges in modern vehicles by introducing a Firmware Over-The-Air (FOTA) solution with added Vehicle-to-Vehicle (V2V) update capability. The project uses STM32 microcontrollers and ESP-NOW for wireless communication, ensuring both security and reliability through AES encryption and OTP-based verification.

<p align="center">
  <img src="https://github.com/user-attachments/assets/14ad406b-f1eb-4cad-8a31-470dd5a48f31" alt="Picture3" width="600"/>
</p>

## FOTA Definition and Implementation
FOTA, or Firmware Over-The-Air, allows devices to be updated remotely without physical intervention. In this project, FOTA is implemented using STM32 microcontrollers. The firmware update is securely uploaded, distributed, and verified before being installed on the vehicle’s ECU.

## Steps for FOTA Implementation:
- A new firmware version is encrypted using AES and uploaded to a Firebase server.
- Vehicle 1 downloads the firmware, decrypts it, and updates its systems.
- The updated vehicle can then transmit the firmware to other nearby vehicles via the V2V update system.

  <p align="center">
  <img src="https://github.com/user-attachments/assets/23f40655-1d5f-4c62-a633-85c533ea6ce2" alt="Picture3" width="600"/>
  </p>

## System Components
Developer Upload and Encryption
Firmware updates are encrypted using AES (Advanced Encryption Standard) to ensure the integrity and confidentiality of the update. The encrypted firmware is then uploaded to a Firebase server for distribution.

## Firebase Server
The Firebase platform is used as a cloud storage solution, allowing the vehicles to access the encrypted firmware file securely.

<p align="center">
  <img src="https://github.com/user-attachments/assets/950b1d16-ba63-4017-bd6f-6c4b0d137685" alt="Picture4" width="500"/>
  <img src="https://github.com/user-attachments/assets/4cf8d290-8c4f-499c-a46e-92f3d8402340" alt="Picture3" width="500"/>
</p>

## Vehicle Firmware Decryption
After downloading the firmware from Firebase, the vehicle uses its onboard STM32 microcontroller to decrypt the firmware using a pre-shared key. The decrypted firmware is then used to update the vehicle's systems.

## V2V Update Using ESP-NOW Protocol
Once Vehicle 1 is updated, it can wirelessly transmit the same firmware to nearby vehicles using the ESP-NOW protocol, a lightweight, low-power protocol ideal for short-range, peer-to-peer communication between vehicles.

  <p align="center">
  <img src="https://github.com/user-attachments/assets/79b43e7d-4196-4fff-91a1-73881e88b464" alt="Picture3" width="600"/>
  </p>

## Bootloader in STM32: (My Part)
A bootloader is a small program that runs before the main application code in an embedded system. It is responsible for managing the process of loading and updating firmware. 
In the STM32 microcontroller, the bootloader plays a crucial role in ensuring that firmware updates, like those in our FOTA project, are carried out securely and correctly.

### STM32 Memory Overview
  The STM32 microcontroller's memory architecture includes several key sections, which are important for understanding how the bootloader works:
  
  - Flash Memory:
    - Non-volatile memory used to store the firmware or application code. This is where the bootloader resides, and it's also where new firmware updates are written during the update process.
    - As you can see below that the main memory (Flash memory) is distributed in 128 pages. Each page is of 1 KB, thus making the total memory of 128 KB.
        <p align="center">
        <img src="https://github.com/user-attachments/assets/4f0ad0d8-389a-44ad-8609-a4db4905e644" alt="Picture3" width="600"/>
        </p>

  - System Memory:
    Contains the factory-programmed bootloader, which can be used to program the Flash memory over different communication interfaces (UART, I2C, CAN, etc.).
  - RAM:
    Volatile memory used for temporary data storage while the system is running.
  
  The STM32 Flash memory is divided into sectors, and these sectors can be individually erased and written to. This sector-based memory structure allows the bootloader to erase specific sections of the Flash and write new firmware without affecting other areas of the memory.

### Bootloader Process
  When the STM32 microcontroller starts up, the bootloader performs a series of steps:

  - Power-on or Reset: Upon power-on or reset, the bootloader runs before the main application code.
  - Check for Update:
      The bootloader checks whether a new firmware update is available. This can be done by looking at a specific memory location (flag) or receiving a signal over a communication interface (like UART).
  - Communication Setup:
      If an update is available, the bootloader establishes communication with the external source (such as a PC, mobile device, or server) to receive the new firmware.
  - Firmware Verification:
      The bootloader ensures the firmware is valid, often through checksums.
  - Erase Flash Memory:
      Before writing the new firmware, the bootloader erases the appropriate sectors in Flash memory.
  - Write New Firmware:
     Finally, the data can be written into the flash memory. The flash memory can be programmed only 16 bits at a time.
  - Reboot to Main Application:
      Once the firmware update is complete, the bootloader jumps to the main application and the system runs the new firmware.

### STM32 Bootloader Frame for Update
  STM32 bootloaders often use specific frames for communication during the firmware update process. A typical frame includes:
  
  - Start Byte: Marks the beginning of the frame.
  - Command: Defines the action to be taken (e.g., erase, write, read, etc.).
  - Data: The payload, such as a portion of the new firmware, the address to write to, or status information.
  - Checksum/CRC: Used for error detection to ensure data integrity during transmission.
  This frame structure ensures reliable communication between the bootloader and the device updating the firmware (such as a PC or server).
  This frame recived to the controller and stored it in `Host_buffer`.

  <p align="center">
    <img src="https://github.com/user-attachments/assets/c217aac7-574d-4023-9347-2b9d64e39c98" alt="Picture3" width="600"/>
  </p>



### Bootloader Functions
  The STM32 bootloader provides several key functions that allow interaction with the microcontroller's Flash memory for firmware updates:
    <p align="center">
      <img src="https://github.com/user-attachments/assets/9ccc912d-20c3-478e-8202-b9fa0e9e76c1" alt="Picture3" width="600"/>
      <img src="https://github.com/user-attachments/assets/a884f152-c3be-439b-a716-a44fcfc14f2e" alt="Picture3" width="600"/>
    </p>

  Communication Protocol (UART):
  The STM32 bootloader communicates with ESP32 over the UART protocol. It allows the bootloader to receive firmware updates from a remote server or a local device.
 
  1) Erase/Write Flash Memory:
  - Erase: The bootloader can erase specific sectors of Flash memory to make room for the new firmware. It erases in units of sectors or pages, depending on the memory structure.

    ```c
      static void BL_Flash_Erase(uint8_t *Host_buffer){
      	BL_SendMessage("start earsing \r\n");
      	uint8_t Erase_status = UNSUCCESSFUL_ERASE;
      	uint16_t Host_Packet_Len=0;
      	uint32_t CRC_valu=0;
      	Host_Packet_Len =  Host_buffer[0]+1;
      	CRC_valu = *(uint32_t*)(Host_buffer+Host_Packet_Len -4);
      	if(CRC_VERIFING_PASS == BL_CRC_verfiy((uint8_t*)&Host_buffer[0],Host_Packet_Len-4,CRC_valu)){
      
      		Erase_status = Perform_Flash_Erase(*((uint32_t*)&Host_buffer[2]),Host_buffer[6]);
      	//	BL_Send_ACK(1);
      	//	HAL_UART_Transmit(&huart2,(uint8_t*)&Erase_status,1,HAL_MAX_DELAY);
      
      	}
      	else{
      		BL_Send_NACK();
      	}
      }
    
  - Write: Once a memory sector is erased, the bootloader writes the new firmware into Flash memory. It ensures that the data is written correctly by using checksums or CRCs to verify the integrity of each memory block.
    
    ```c
    static void BL_Write_Data(uint8_t *Host_buffer){
    	//BL_SendMessage(" i enter write data function \r\n");
    	uint8_t Adress_varfiy=ADDRESS_IS_INVALID;
    	uint32_t Address_Host=0;
    	uint8_t DataLen=0;
    	uint8_t payload_status =FLASH_PAYLOAD_WRITE_FAILED;
    	uint16_t Host_Packet_Len=0;
    	uint32_t CRC_valu=0;
    	Host_Packet_Len =  Host_buffer[0]+1;
    	CRC_valu = *(uint32_t*)(Host_buffer+Host_Packet_Len -4);
      BL_SendMessage(" : 1 \r\n");
      //BL_Send_ACK(1);
      //BL_SendMessage(" : 2 \r\n");
      Address_Host = *((uint32_t*)&Host_buffer[2]);
      DataLen = Host_buffer[6];
      Adress_varfiy = BL_Address_Varification(Address_Host);
      if(Adress_varfiy == ADDRESS_IS_VALID){
        //flash
        payload_status = FlashMemory_Paylaod_Write((uint16_t*)&Host_buffer[7],Address_Host,DataLen);
      }
      else{
        HAL_UART_Transmit(&huart2,(uint8_t*)&Adress_varfiy,1,HAL_MAX_DELAY);
      }
    }
    
  2) Get Bootloader Version:
  - The bootloader can return its version number, allowing developers to ensure that they are working with the correct version for a given microcontroller.

    ```c
    static void BL_Get_Version(uint8_t *Host_buffer){
    	uint8_t Veraion[4]={CBL_VENDOR_ID,CBL_SW_MAJOR_VERSION,CBL_SW_MINOR_VERSION,CBL_SW_PATCH_VERSION};
    	uint16_t Host_Packet_Len=0;
    	uint32_t CRC_valu=0;
    	Host_Packet_Len =  Host_buffer[0]+1;
    	CRC_valu = *(uint32_t*)(Host_buffer+Host_Packet_Len -4);
    	if(CRC_VERIFING_PASS == BL_CRC_verfiy((uint8_t*)&Host_buffer[0],Host_Packet_Len-4,CRC_valu)){
    		BL_Send_ACK(4);
    		HAL_UART_Transmit(&huart2,(uint8_t*)Veraion,4,HAL_MAX_DELAY);
    		Jump_Application();
    	}
    	else{
    		BL_Send_NACK();
    	}
    }

### Bootloader Communication Process
  When the STM32 bootloader receives a new firmware image over UART, it follows this process:
  
  1) Establish Communication: The bootloader starts listening for communication on the UART port. Once it receives a command to start an update, it acknowledges and waits for the new firmware.
  2) Frame Reception: The firmware is sent in frames (packets of data), each containing a portion of the new firmware along with addressing information and a checksum/CRC for error checking.
  3) Erase Flash: Upon receiving the initial frames, the bootloader erases the necessary sectors in the Flash memory.
  4) Write Flash: The bootloader writes the received data to the Flash memory in blocks, ensuring each block is written correctly by verifying it after each write operation.
  5) Verification and Completion: After all firmware blocks have been written, the bootloader verifies the integrity of the entire firmware image. If everything is correct, it marks the update as successful and reboots the microcontroller to run the new application code.

## PCB Design with Altium (My Part)
We designed the custom PCB required for this project using Altium Designer. The PCB integrates the STM32 microcontroller, ESP module for V2V communication, and a LCD . The design focuses on reliability, ease of manufacturing, and proper signal routing to ensure optimal performance during the update process.

  <p align="center">
    <img src="https://github.com/user-attachments/assets/ae2e7b05-1568-4d33-9c06-d7c5210153eb" alt="Picture3" width="440"/>
    <img src="https://github.com/user-attachments/assets/24e85ad9-b6db-4b16-8bc7-aa200023d1c7" alt="Picture3" width="560"/>
  </p>
