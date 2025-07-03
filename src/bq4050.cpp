
#include "bq4050.h"


void BQ4050::crc8_tab_init(){
  // Function that generates uint8_t array as a lookup table to quickly create a CRC8 for the PEC
  const uint8_t generator = 0x07;
  /* iterate over all uint8_t values 0 - 255 */
  for (int divident = 0; divident < 256; divident++)
  {
    uint8_t currByte = (uint8_t)divident;
    /* calculate the CRC-8 value for current uint8_t */
    for (uint8_t bit = 0; bit < 8; bit++)
    {
      if ((currByte & 0x80) != 0)
      {
        currByte <<= 1;
        currByte ^= generator;
      }
      else
      {
        currByte <<= 1;
      }
    }
    /* store CRC value in lookup table */
    crctable[divident] = currByte;
    if (printResults)
    {
      if (divident % 16 == 0 && divident > 2)
      {
        dbgSerial.println();
      }
      if (currByte < 16)
        dbgSerial.print("0");
      dbgSerial.print(currByte, HEX);
      dbgSerial.print("\t");
    }
  }
  if (printResults)
  {
    dbgSerial.println();
  }
}

uint8_t BQ4050::compute_crc8(uint8_t *bytes, int byteLen){
  // Function to check uint8_t array to be sent out against lookup table to efficiently calculate PEC
  uint8_t crc = 0;
  for (int i = 0; i < byteLen; i++){
    /* XOR-in next input uint8_t */
    uint8_t data = (uint8_t)(bytes[i] ^ crc);
    /* get current CRC value = remainder */
    crc = (uint8_t)(this->crctable[data]);
    if (this->printResults)
    {
      comSerial.print("uint8_t value: ");
      comSerial.print(bytes[i], HEX);
      comSerial.print("\tlookup position: ");
      comSerial.print(data, HEX);
      comSerial.print("\tlookup value: ");
      comSerial.println(crc, HEX);
    }
  }

  return crc;
}


uint16_t BQ4050::rd_reg_word(uint8_t reg){
    this->wire->beginTransmission(this->devAddr);
    this->wire->write(reg);
    this->wire->endTransmission();//send stop signal
    delay(1); // Wait for the device to process the command
    this->wire->requestFrom((uint8_t)this->devAddr, 2);//send start signal
    if (2 == this->wire->available()){
        uint8_t lsb = this->wire->read();
        uint8_t msb = this->wire->read();
        return (msb << 8) | lsb;
    }
    return 0xFFFF;
}


void BQ4050::wd_reg_word(uint8_t reg, uint16_t value){
  this->wire->beginTransmission(this->devAddr);
  this->wire->write(reg);
  this->wire->write(value & 0xFF);
  this->wire->write(value >> 8);
  uint8_t result = this->wire->endTransmission();
  delay(10); // Wait for the device to process the command
  if (result != 0)
  {
    comSerial.print("Write to register 0x");
    comSerial.print(reg, HEX);
    comSerial.println(" failed.");
  }
}

bool BQ4050::rd_mac_block(uint8_t *data, uint8_t arrLen, bool withPEC) {
    uint8_t buf[32+2] = {
        this->devAddr<<1,     
        BLOCK_ACCESS_CMD, 
        (this->devAddr<<1) + 1,
    };

    this->wire->beginTransmission(this->devAddr);
    this->wire->write(BLOCK_ACCESS_CMD);
    this->wire->endTransmission(false); 

    this->wire->requestFrom((uint8_t)this->devAddr, arrLen + 2); // send start signal
    uint32_t start = millis();
    while (this->wire->available() < 1) {
        if (millis() - start > 500){
            dbgSerial.println("SMBus read timeout!");
            return false; 
        }
    }

    // data len, pec not included
    buf[3] = this->wire->read(); // data length
    if (buf[3] > 32 + 2) {
        dbgSerial.print("Block data length exceeds 32 bytes, invalid data length : ");
        dbgSerial.println(buf[3]);
        return false; // avoid buffer overflow
    }


    for (uint8_t i = 0; i < buf[3] - (withPEC ? 0:1 ); i++) {
        if (this->wire->available()) {
            buf[i + 4] = this->wire->read();
        }
        else {
            dbgSerial.println("Block data read error, not enough data available.");
            return false; 
        }
    }

    if(withPEC){
      uint8_t PECcheck = this->compute_crc8(buf, buf[3] + 4); // Calculate PEC for the received data
      if (this->wire->available()) {
          uint8_t pec = this->wire->read();
          if(PECcheck == pec) {
            for (uint8_t i = 0; i < buf[3]; i++) {
                data[i] = buf[i + 4 + 2];
            }
          }
          return (PECcheck == pec); // Compare calculated PEC with received PEC
      }
      dbgSerial.println("Block data read error, no PEC byte received.");
      return false; // If PEC does not match, return false
    }
    else{
      for (uint8_t i = 0; i < buf[3]; i++) {
          data[i] = buf[i + 4 + 2 ];
      }
      return true; 
    }

    return false; 
}


bool BQ4050::wd_df_block(uint16_t cmd, uint8_t *data, uint8_t arrLen) {
    // Prepare the command to write to the device
    // According to manual: block = starting address + DF data block
    // Total bytes = 2 bytes for starting address + arrLen bytes for data
    uint8_t totalBytes = 2 + arrLen;
    
    this->wire->beginTransmission(this->devAddr);
    this->wire->write(BLOCK_ACCESS_CMD);
    this->wire->write(totalBytes); // Total number of bytes (address + data)
    this->wire->write((uint8_t)(cmd & 0xFF)); // Starting address LSB
    this->wire->write((uint8_t)((cmd >> 8) & 0xFF)); // Starting address MSB

    // Write the DF data block
    for (uint8_t i = 0; i < arrLen; i++) {
        this->wire->write(data[i]);
    }

    uint8_t result = this->wire->endTransmission();
    
    if (result != 0) {
        dbgSerial.print("Write to DF block failed with error code: ");
        dbgSerial.print(result);
        switch(result) {
            case 1:
                dbgSerial.println(" (Data too long to fit in transmit buffer)");
                break;
            case 2:
                dbgSerial.print(" (Received NACK on transmit of address 0x");
                dbgSerial.print(this->devAddr, HEX);
                dbgSerial.println(")");
                break;
            case 3:
                dbgSerial.println(" (Received NACK on transmit of data)");
                break;
            case 4:
                dbgSerial.println(" (Other error)");
                break;
            default:
                dbgSerial.println(" (Unknown error)");
        }
        return false; // Return false if transmission was not successful
    }
    
    return true; 
}

bool BQ4050::rd_df_block(uint16_t cmd, uint8_t *data, uint8_t arrLen, bool isString) {
    this->wire->beginTransmission(this->devAddr);
    this->wire->write(BLOCK_ACCESS_CMD);
    this->wire->endTransmission(false); 

    uint8_t reqLen = (isString) ? arrLen + 4 : arrLen + 3; 

    this->wire->requestFrom((uint8_t)this->devAddr, reqLen); // send start signal
    uint32_t start = millis();
    while (this->wire->available() < 1) {
        if (millis() - start > 500){
            dbgSerial.println("SMBus read timeout!");
            return false; 
        }
    }


    uint8_t packlen = this->wire->read(); // Read the first byte as package length

    uint16_t cmdret = this->wire->read();
    cmdret |= (this->wire->read() << 8); // Read the next two bytes as command

    if(cmdret != cmd) {
        dbgSerial.print("Command mismatch, expected: ");
        dbgSerial.print(cmd, HEX);
        dbgSerial.print(", received: ");
        dbgSerial.println(*(uint16_t *)(data + 1), HEX);
        return false; // Command mismatch
    }

    uint8_t strLen = (isString) ? this->wire->read() : 0; // Read the next byte as string length if isString is true
    
    for (uint8_t i = 0; i < arrLen; i++) {
        if (this->wire->available()) {

            data[i] = this->wire->read();
        }
        else {
            dbgSerial.println("Block data read error, not enough data available.");
            return false; 
        }
    }

    return true; 
}


bool BQ4050::wd_mac_cmd(uint16_t cmd){
    //for simplicity, we create an array to hold all the byte we will be sending
    uint8_t byteArr[5] = {
        (uint8_t)(this->devAddr<<1),           // 
        BLOCK_ACCESS_CMD,                   // 
        0x02,                               // 
        (uint8_t)(cmd & 0xFF),       // 
        (uint8_t)((cmd >> 8) & 0xFF) // 
    };
    //This array is then sent to the CRC checker. The CRC includes all bytes sent.
    //The arrSize var is static, so there are always 5 bytes to be sent
    uint8_t PECcheck = this->compute_crc8( byteArr, sizeof(byteArr) );

    this->wire->beginTransmission(this->devAddr);
    //MAC access
    this->wire->write( BLOCK_ACCESS_CMD );
    //number of bytes in to be sent. In when sending a command, this is always 2
    this->wire->write( 0x02 );
    // delay(1);
    //little endian of address
    this->wire->write( cmd&0xFF );
    this->wire->write((cmd>>8)&0xFF );
    //send CRC at the end
    this->wire->write( PECcheck );
    uint8_t ack = this->wire->endTransmission();
    return (ack == 0); // Return true if transmission was successful
}


bool BQ4050::rd_fw_version(uint8_t *data, uint8_t len) {
    // uint8_t data[11 + 2] = {0,}; // 11 bytes for firmware version + 1 byte for data length + 1 byte for PEC
    if (!this->wd_mac_cmd(MAC_CMD_FW_VER)) {
        dbgSerial.println("Write MAC CMD FW failed!");
        return false;
    }
    delay(18);
    len+=2;
    if (!this->rd_mac_block(data, len, true)) {
        dbgSerial.println("Read MAC CMD FW failed!");
        return false;
    }

    if(this->printResults){
      for(uint8_t i = 0; i < len; i++) {
          if (data[i] < 0x10) {
              dbgSerial.print("0");
          }
          dbgSerial.print(data[i], HEX);
          if (i < len - 1) {
              dbgSerial.print(" ");
          }
      }
      dbgSerial.println();
    }

    return true;
}

bool BQ4050::rd_hw_version(uint8_t *data, uint8_t len) {
    // uint8_t data[2 + 2] = {0,}; // 2 bytes for hardware version + 1 byte for data length + 1 byte for PEC
    if (!this->wd_mac_cmd(MAC_CMD_HW_VER)) {
        dbgSerial.println("Write MAC CMD HW failed!");
        return false;
    }
    delay(18);
    len+=2;
    if (!this->rd_mac_block(data, len, true)) {
        dbgSerial.println("Read MAC CMD HW failed!");
        return false;
    }

    if(this->printResults){
      for(uint8_t i = 0; i < len; i++) {
          if (data[i] < 0x10) {
              dbgSerial.print("0");
          }
          dbgSerial.print(data[i], HEX);
          if (i < len - 1) {
              dbgSerial.print(" ");
          }
      }
      dbgSerial.println();
    }

    return true;
}





bool BQ4050::rd_df_dev_name(uint8_t *data, uint8_t len) {
    if (!wd_mac_cmd(DF_CMD_DEVICE_NAME)) {
        dbgSerial.println("Write df cmd failed!");
        return false;
    }
    delay(18);
    if (!rd_df_block(DF_CMD_DEVICE_NAME, data, len, true)) {
        dbgSerial.println("Read df cmd  failed!");
        return false;
    }

    if(this->printResults){
      for(uint8_t i = 0; i < len; i++) {
          if (data[i] < 0x10) {
              dbgSerial.print("0");
          }
          dbgSerial.print(data[i], HEX);
          if (i < len - 1) {
              dbgSerial.print(" ");
          }
      }
      dbgSerial.println();
    }

    return true; // Return true if data was read successfully and PEC matches
}

bool BQ4050::rd_df_manufacturer_name(uint8_t *data, uint8_t len) {
    if (!wd_mac_cmd(DF_CMD_MANUFACTURER_NAME)) {
        dbgSerial.println("Write df cmd failed!");
        return false;
    }
    delay(18);
    if (!rd_df_block(DF_CMD_MANUFACTURER_NAME, data, len, true)) {
        dbgSerial.println("Read df cmd  failed!");
        return false;
    }

    if(this->printResults){
      for(uint8_t i = 0; i < len; i++) {
          if (data[i] < 0x10) {
              dbgSerial.print("0");
          }
          dbgSerial.print(data[i], HEX);
          if (i < len - 1) {
              dbgSerial.print(" ");
          }
      }
      dbgSerial.println();
    }
    return true; 
}

bool BQ4050::rd_df_da_configuration(uint8_t *data, uint8_t len) {
    if (!wd_mac_cmd(DF_CMD_DA_CONFIGURATION)) {
        dbgSerial.println("Write df cmd failed!");
        return false;
    }
    delay(18);
    if (!rd_df_block(DF_CMD_DA_CONFIGURATION, data, len, false)) {
        dbgSerial.println("Read df cmd  failed!");
        return false;
    }

    if(this->printResults){
      for(uint8_t i = 0; i < len; i++) {
          if (data[i] < 0x10) {
              dbgSerial.print("0");
          }
          dbgSerial.print(data[i], HEX);
          if (i < len - 1) {
              dbgSerial.print(" ");
          }
      }
      dbgSerial.println();
    }

    return true; 
}







bool BQ4050::rd_cell_temp(uint8_t *data, uint8_t len) {
    if (!wd_mac_cmd(MAC_CMD_DA_STATUS2)) {
        dbgSerial.println("Write cell temp failed!");
        return false;
    }
    delay(18);
    len+=2;
    if (!rd_mac_block(data, len, true)) {
        dbgSerial.println("Read cell temp  failed!");
        return false;
    }

    if(this->printResults){
      for(uint8_t i = 0; i < len; i++) {
          if (data[i] < 0x10) {
              dbgSerial.print("0");
          }
          dbgSerial.print(data[i], HEX);
          if (i < len - 1) {
              dbgSerial.print(" ");
          }
      }
      dbgSerial.println();
    }

    return true;
}



bool BQ4050::fet_toggle(){
    if(this->wd_mac_cmd(MAC_CMD_FET_CONTROL)) {
        delay(500); // Wait for the device to process the command
        return true; // Return true if command was sent successfully
    }
    return false; // Return false if there was an error sending the command
}

bool BQ4050::reset(){
    if(this->wd_mac_cmd(MAC_CMD_DEV_RESET)) {
        delay(500); // Wait for the device to reset
        return true; // Return true if reset command was sent successfully
    }
    return false; // Return false if there was an error sending the reset command
}