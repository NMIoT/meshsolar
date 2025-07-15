
#include "bq4050.h"
#include "logger.h"


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
  }
  return crc;
}

bool BQ4050::read_reg_word(bq4050_reg_t *reg){
    this->wire->beginTransmission(this->devAddr);
    this->wire->write(reg->addr); // Register address
    this->wire->endTransmission();//send stop signal
    this->wire->requestFrom((uint8_t)this->devAddr, (uint8_t)2);//send start signal
    if (2 == this->wire->available()){
        uint8_t lsb = this->wire->read();
        uint8_t msb = this->wire->read();
        reg->value = (msb << 8) | lsb;
        return true;
    }
    return false; // If we don't get 2 bytes, return false
}

bool BQ4050::write_reg_word(bq4050_reg_t reg){
  this->wire->beginTransmission(this->devAddr);
  this->wire->write(reg.addr); // Register address
  this->wire->write(reg.value & 0xFF);
  this->wire->write(reg.value >> 8);
  uint8_t result = this->wire->endTransmission();
  if (result != 0){
    LOG_W("Write to register 0x%02X failed.", reg.addr);
  }
  return (result == 0); // Return true if transmission was successful
}

bool BQ4050::_wd_mac_cmd(uint16_t cmd){
    //for simplicity, we create an array to hold all the byte we will be sending
    uint8_t byteArr[5] = {
        (uint8_t)(this->devAddr<<1),    
        BLOCK_ACCESS_CMD,             
        0x02,                         
        (uint8_t)(cmd & 0xFF),        
        (uint8_t)((cmd >> 8) & 0xFF)  
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



bool BQ4050::_rd_mac_block(bq4050_block_t *block){
    // Blockdata [38] bytes: [0x16,0x44,0x17,     0x22,0x71,0x00,0x57,0x0e,0x58,0x0e,0x58,
    //                                            0x0e,0x55,0x0e, 0x5a,0x39, 0x51,0x39,0xfc,
    //                                            0xff,0xfb,0xff,0xfb,0xff,0xfe,0xff,0xff,
    //                                            0xff,0xfe,0xff,0xfe,0xff,0xff,0xff,0xfa,
    //                                            0x00,0x00,0x00]

    static uint8_t buf[32+4+2];
    memset(buf, 0, sizeof(buf));                // Clear the buffer to avoid garbage data

    buf[0] = (uint8_t)(this->devAddr<<1);       // Device address shifted left by 1
    buf[1] = (uint8_t)(BLOCK_ACCESS_CMD);       // Command for block access
    buf[2] = (uint8_t)((this->devAddr<<1) + 1); // Read command, shifted left by 1 and incremented by 1

    this->wire->beginTransmission(this->devAddr);
    this->wire->write(BLOCK_ACCESS_CMD);
    this->wire->endTransmission(false); 

    this->wire->requestFrom((uint8_t)this->devAddr, (uint8_t)(block->len + 2 + 1 + 1)); // request max 32 bytes first

    // data len, pec not included in this len
    buf[3] = this->wire->read(); // data length
    LOG_D("Block data length: %d Bytes", buf[3]);

    for (uint8_t i = 0; i < buf[3]; i++) {
        if (this->wire->available()){
            buf[i + 4] = this->wire->read();
        }
        else {
            LOG_E("Block data read error, not enough data available, expected %d bytes but got %d bytes", buf[3], i + 1);
            return false; 
        }
    }


    // Check if the command in the block matches the expected command
    // buf[4] and buf[5] is the command which is returned by the device
    if(*(uint16_t *)(buf + 4) != block->cmd){
        LOG_E("Command received: 0x%04X, expected: 0x%04X", *(uint16_t *)(buf + 4), block->cmd);
        return false; // If the command does not match, return false
    }

    uint8_t PECcheck = this->compute_crc8(buf, buf[3] + 4); // Calculate PEC for the received data, not including the command and PEC byte itself
    if (this->wire->available()) {
        uint8_t pec = this->wire->read();
        if(PECcheck == pec) {
            block->pvalue = buf + 4 + 2; // Assign the data to the block pointer, jump buf[4] and buf[5] which are the command bytes
            block->len    = buf[3] - 2;  // Set the length of the block
        }
        return (PECcheck == pec); // Compare calculated PEC with received PEC
    }
    LOG_E("Block data read error, no PEC byte received.");
    return false; // If PEC does not match, return false
}



bool BQ4050::_rd_df_block(bq4050_block_t *block) {
    uint8_t reqLen = (block->type == STRING) ? block->len + 4 : block->len + 3; 
    static uint8_t buf[32];
    memset(buf, 0, sizeof(buf)); // Clear the buffer to avoid garbage data

    this->wire->beginTransmission(this->devAddr);
    this->wire->write(BLOCK_ACCESS_CMD);
    this->wire->endTransmission(false); 
    this->wire->requestFrom((uint8_t)this->devAddr, reqLen); // send start signal

    buf[0] = this->wire->read(); // Read the first byte as package length
    buf[1] = this->wire->read();
    buf[2] = this->wire->read(); // Read the next two bytes as command

    if(*(uint16_t*)(buf + 1) != block->cmd) {
        LOG_E("Command mismatch! Expected: 0x%04X, Received: 0x%04X", block->cmd, *(uint16_t*)(buf + 1));
        return false;
    }

    for (uint8_t i = 0; i < block->len; i++) {
        if (this->wire->available()) {
            buf[i + 3] = this->wire->read(); // Read the data bytes
        }
        else {
            LOG_E("Block data read error, not enough data available.");
            return false; 
        }
    }
    block->pvalue = (block->type == STRING) ? buf + 4 : buf + 3; // buf[3] is the data length byte if block is STRING，or buf[3] is the first data byte if block is NUMBER
    return true; 
}

bool BQ4050::read_mac_block(bq4050_block_t *block) {
    if (!this->_wd_mac_cmd(block->cmd)) {
        LOG_E("Write MAC CMD [0x%04X] failed!", block->cmd);
        return false;
    }
    if (!this->_rd_mac_block(block)) {
        LOG_E("Read MAC CMD  [0x%04X] failed!", block->cmd);
        return false;
    }
    return true;
}

bool BQ4050::write_dataflash_block(bq4050_block_t block) {
    // Prepare the command to write to the device
    // According to manual: block = starting address + DF data block
    // Total bytes = 2 bytes for starting address + arrLen bytes for data
    uint8_t totalBytes = 2 + block.len;
    
    this->wire->beginTransmission(this->devAddr);
    this->wire->write(BLOCK_ACCESS_CMD);
    this->wire->write(totalBytes); // Total number of bytes (address + data)
    this->wire->write((uint8_t)(block.cmd & 0xFF)); // Starting address LSB
    this->wire->write((uint8_t)((block.cmd >> 8) & 0xFF)); // Starting address MSB

    // Write the DF data block
    for (uint8_t i = 0; i < block.len; i++) {
        this->wire->write(block.pvalue[i]);
    }

    uint8_t result = this->wire->endTransmission();
    
    if (result != 0) {
        LOG_W("Write to DF block failed with error code: %d", result);
        switch(result) {
            case 1:
                LOG_W(" (Data too long to fit in transmit buffer)");
                break;
            case 2:
                LOG_W(" (Received NACK on transmit of address 0x%02X)", this->devAddr);
                break;
            case 3:
                LOG_W(" (Received NACK on transmit of data)");
                break;
            case 4:
                LOG_W(" (SMBus error)");
                break;
            default:
                LOG_W(" (Unknown error)");
        }
        return false; // Return false if transmission was not successful
    }
    return true; 
}

bool BQ4050::read_dataflash_block(bq4050_block_t *block) {
    if (!this->_wd_mac_cmd(block->cmd)) {
        LOG_E("Write DF CMD [0x%04X] failed!", block->cmd);
        return false;
    }
    if (!this->_rd_df_block(block)) {
        LOG_E("Read DF CMD  [0x%04X] failed!", block->cmd);
        return false;
    }
    return true; // Return true if data was read successfully and PEC matches
}

bool BQ4050::fet_toggle(){
    if(this->_wd_mac_cmd(MAC_CMD_FET_CONTROL)) {
        delay(100);  // Wait for the device to process the command
        return true; // Return true if command was sent successfully
    }
    return false;    // Return false if there was an error sending the command
}

bool BQ4050::reset(){
    if(this->_wd_mac_cmd(MAC_CMD_DEV_RESET)) {
        delay(100);  // Wait for the device to reset
        return true; // Return true if reset command was sent successfully
    }
    return false; // Return false if there was an error sending the reset command
}