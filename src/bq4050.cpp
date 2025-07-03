
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



uint16_t BQ4050::bq4050_rd_word(uint8_t reg){
    this->wire->beginTransmission(this->devAddr);
    this->wire->write(reg);
    this->wire->endTransmission();//send stop signal
    delay(1); // Wait for the device to process the command
    this->wire->requestFrom(this->devAddr, 2);//send start signal
    if (2 == this->wire->available()){
        uint8_t lsb = this->wire->read();
        uint8_t msb = this->wire->read();
        return (msb << 8) | lsb;
    }
    return 0xFFFF;
}

bool BQ4050::bq4050_rd_word_with_pec(uint8_t reg, uint16_t *value){
    uint8_t byteArr[] = {
        (uint8_t)(this->devAddr <<1 ), // slave write address, lsb = 0 
        reg,                      
    };
    uint8_t PECcheck = this->compute_crc8(byteArr, sizeof(byteArr));

    this->wire->beginTransmission(this->devAddr);
    this->wire->write(reg);
    this->wire->write(PECcheck);
    this->wire->endTransmission();//send stop signal
    delay(5);              // Wait for the device to process the command
    this->wire->requestFrom(this->devAddr, 3);//send start signal
    if (3 != this->wire->available()) return false; // Ensure we have 3 bytes to read

    uint8_t lsb = this->wire->read();
    uint8_t msb = this->wire->read();
    PECcheck    = this->wire->read();

    uint8_t retArr[] = {
        (uint8_t)(this->devAddr<<1) + 1, // slave write address, lsb = 1 
        lsb, 
        msb,                      
    };

    // dbgSerial.print("PEC received: 0x");
    // dbgSerial.println(PECcheck, HEX);
    // dbgSerial.print("PEC calculated: 0x");
    // dbgSerial.println(Compute_CRC8(retArr, sizeof(retArr)), HEX);




    if(PECcheck == this->compute_crc8(retArr, sizeof(retArr))){
        *value = (msb << 8) | lsb;
        return true; // Return true if PEC matches
    }
    return false;
}


void BQ4050::bq4050_wd_word(uint8_t reg, uint16_t value){
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







