//a library of functions for a master to communicate with the i2cslave
#include "globals.h"
#include <Wire.h>
#include "i2cslave.h"
#include "utilities.h"
#include <Arduino.h>

// Existing watchdog commands
#define COMMAND_REBOOT              128   //reboots the slave asynchronously using the watchdog system
#define COMMAND_MILLIS              129   //returns the millis() value of the slave 
#define COMMAND_LASTWATCHDOGREBOOT  130   //millis() of the last time the slave sent a reboot signal to the master
#define COMMAND_WATCHDOGREBOOTCOUNT 131   //number of times the slave has rebooted the master since it was itself rebooted
#define COMMAND_LASTWATCHDOGPET     132   //millis() of the last time the master petted the slave in its watchdog function
#define COMMAND_LASTPETATBITE       133   //how many seconds late the last watchdog pet was when the slave sent a reboot signal
#define COMMAND_REBOOTMASTER        134   //reboot the master now by asserting the reboot line
#define COMMAND_SLEEP               135   //go into the kind of sleep where I2C will wake it up
#define COMMAND_DEEP_SLEEP          136   //go into unreachably deep sleep for n seconds
#define COMMAND_POWER_TYPE          137   //0: normal, 1: switch to low-power mode (going lightly to sleep after handling the last I2C request)

#define COMMAND_WATCHDOGPETBASE     200   //commands above 200 are used to tell the slave how often it needs to be petted.  this command can also update the slave's unix timestamp

// New EEPROM-style commands
#define COMMAND_EEPROM_SETADDR      150   // set pointer for read/write
#define COMMAND_EEPROM_WRITE        151   // sequential write mode 
#define COMMAND_EEPROM_READ         152   // sequential read mode
#define COMMAND_EEPROM_NORMAL       153   // exit EEPROM mode, back to default behavior

#define COMMAND_VERSION             160   //returns the human-updated version number of the firmware source code.  this version began at 2000
#define COMMAND_COMPILEDATETIME     161   //unix timestamp of when the firmware was compiled
#define COMMAND_TEMPERATURE         162   //a pseudo-random poor approximation of temperature
#define COMMAND_FREEMEMORY          163   //returns free memory on the slave
#define COMMAND_GET_SLAVE_CONFIG    164   //returns where in the EEPROM the slave's local configuration is persisted

//serial commands
#define COMMAND_PARSE_BUFFER                169   //explicitly parse data in the txBuffer using the serial parser system
#define COMMAND_SERIAL_SET_BAUD_RATE        170   //using an ordinal to set common serial baud rates.  1 is 300, 5 is 9600, 9 is 115200
#define COMMAND_RETRIEVE_SERIAL_BUFFER      171   //retrieves values from the serial read buffer if we are in serial mode #1
#define COMMAND_POPULATE_SERIAL_BUFFER      172   //sets values in the serial buffer that the slave will transmit via serial
#define COMMAND_GET_LAST_PARSE_TIME         173   //retrieves the unix time of the last serial parse, if unix time is known
#define COMMAND_GET_PARSED_SERIAL_DATA      174   //returns a whole packet of parsed data
#define COMMAND_SET_PARSED_OFFSET           175   //if parsed data packet is large, this will set a pointer into it for retrieval from the master
#define COMMAND_GET_PARSED_DATUM            176   //returns a specific value found by the serial parser given an ordinal into a 16 bit sequence in the packet
#define COMMAND_GET_PARSE_CONFIG_NUMBER     177   //returns the number of parser configs (blocks used by the serial parser, equivalent to items in css array)
#define COMMAND_GET_PARSED_PACKET_SIZE      178   //returns the size of the parsed data packet
#define COMMAND_SET_SERIAL_MODE             179   //sets serial mode:  
                                                  //0 - no serial
                                                  //1 - serial pass-through to master
                                                  //2 - slave parses incoming values in serial but cannot transmit serial values
                                                  //3 - gather interesting serial lines from parser for master to pick up
                                                  //4 - parses incoming values in serial, though can still transmit data serial port
                                                  //5 - fakes the reception of data via serial using I2C data sent from master using send slave serial (command COMMAND_POPULATE_SERIAL_BUFFER)
#define COMMAND_SET_UNIX_TIME               180   //sets unix timestamp, which the slave the automatically advances with reasonable accuracy
#define COMMAND_GET_UNIX_TIME               181   //returns unix timestamp as known to the slave
#define COMMAND_GET_CONFIG                  182   //gets a config item by ordinal number (from the configuration cis[] array)
#define COMMAND_SET_CONFIG                  183   //sets a config item by ordinal and value
#define COMMAND_GET_LONG                    184   //gets a config long item in cls by ordinal number (from the configuration cis[] array)
#define COMMAND_SET_LONG                    185   //sets a config long item in cls by ordinal and value
#define COMMAND_ENTER_BOOTLOADER            190   //reflash bootloader


#define EEPROM_MARKER_ADDR 0
#define EEPROM_INT_BASE    4   // ints start immediately after "DATA"

// ---- Write an int (2 bytes) ----
void writeIntToEEPROM(uint16_t addr, int value) {
    if(ci[SLAVE_I2C] < 1) {
      return;
    }
    uint8_t bytes[2];
    bytes[0] = value & 0xFF;        // LSB
    bytes[1] = (value >> 8) & 0xFF; // MSB

 
    setAddress(addr); 

    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(COMMAND_EEPROM_WRITE);
    Wire.write(bytes[0]);
    Wire.write(bytes[1]);
    Wire.endTransmission();
    delay(5);

    normalSlaveMode();
}

char readByteFromEEPROM(uint16_t addr) {
    if(ci[SLAVE_I2C] < 1) {
      return 0;
    }
    setAddress(addr); 

    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(COMMAND_EEPROM_READ);
    Wire.write(1);
    Wire.endTransmission();

    int value = 0;
    Wire.requestFrom(ci[SLAVE_I2C], (uint8_t)2);
    if (Wire.available() >= 1) {
        value = Wire.read();
    }

    normalSlaveMode();

    return value;
}

// ---- Read an int (2 bytes) ----
int readIntFromEEPROM(uint16_t addr) {
    if(ci[SLAVE_I2C] < 1) {
      return 0;
    }
    setAddress(addr); 

    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(COMMAND_EEPROM_READ);
    Wire.write(2);
    Wire.endTransmission();

    int value = 0;
    Wire.requestFrom(ci[SLAVE_I2C], (uint8_t)2);
    if (Wire.available() >= 2) {
        value = Wire.read();
        value |= (Wire.read() << 8);
    }

    normalSlaveMode();

    return value;
}

// ---- Write a long (4 bytes) ----
void writeLongToEEPROM(uint16_t addr, long value) {
    if(ci[SLAVE_I2C] < 1) {
      return;
    }
    uint8_t bytes[4];
    bytes[0] = value & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    bytes[2] = (value >> 16) & 0xFF;
    bytes[3] = (value >> 24) & 0xFF;

    setAddress(addr); 

    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(COMMAND_EEPROM_WRITE);
    Wire.write(bytes, 4);
    Wire.endTransmission();
    delay(5);

    normalSlaveMode();
}

// ---- Read a long (4 bytes) ----
long readLongFromEEPROM(uint16_t addr) {
    if(ci[SLAVE_I2C] < 1) {
      return 0;
    }
    setAddress(addr); 

    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(COMMAND_EEPROM_READ);
    Wire.write(4);
    Wire.endTransmission();

    long value = 0;
    Wire.requestFrom(ci[SLAVE_I2C], (uint8_t)4);
    if (Wire.available() >= 4) {
        value = Wire.read();
        value |= ((long)Wire.read() << 8);
        value |= ((long)Wire.read() << 16);
        value |= ((long)Wire.read() << 24);
    }

    normalSlaveMode();

    return value;
}

void writeStringToEEPROM(uint16_t addr, const char* str) {
    if(ci[SLAVE_I2C] < 1) {
      return;
    }
    if (!str) str = ""; // ensure non-null pointer

    // 1. Set EEPROM start address
    setAddress(addr);

    // 2. Send the string in chunks
    const char* p = str;
    while (true) {
        setAddress(addr);

        Wire.beginTransmission(ci[SLAVE_I2C]);
        Wire.write(COMMAND_EEPROM_WRITE);
        Wire.write(*p);
        Wire.endTransmission();
        delay(5);

        if (*p == 0) break;
        p++;
        addr++;
    }

    normalSlaveMode();
}


void readStringFromSlaveEEPROM(uint16_t addr, char* buffer, size_t maxLen) {
    if(ci[SLAVE_I2C] < 1) {
      return;
    }
    // Set EEPROM address
    setAddress(addr); 

    // Enable sequential read
    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(COMMAND_EEPROM_READ);
    Wire.endTransmission();

    size_t count = 0;
    while (count < maxLen - 1) {
        Wire.requestFrom(ci[SLAVE_I2C], (uint8_t)1); // read 1 byte at a time
        if (Wire.available()) {
            char c = Wire.read();
            if (c == 0) break;  // stop at null terminator
            buffer[count++] = c;
        } else {
            break; // no more data
        }
    }
    buffer[count] = '\0';

    normalSlaveMode();
}

void readBytesFromSlaveEEPROM(uint16_t addr, char* buffer, size_t maxLen) {
    if(ci[SLAVE_I2C] < 1) {
      return;
    }
    // Set EEPROM address
    //Serial.println((int) addr);
    setAddress(addr); 
    const uint8_t blockSize = 32;
    // Enable sequential read
    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(COMMAND_EEPROM_READ);
    Wire.write(blockSize);
    Wire.endTransmission();
    size_t count = 0;
    while (count < maxLen -1) {   // reserve space for hex + null
        //Serial.print(count);
        //Serial.print(" : ");
        // Request a block of bytes
        uint8_t received = Wire.requestFrom(ci[SLAVE_I2C], blockSize);
        if (received == 0) goto finished;  // no more data from slave
        while (Wire.available() && count < maxLen - 1) {
            char b = Wire.read();
            //Serial.println(b);
            if (b == 0) {          // null terminator from slave
                //Serial.println("endieooo");
                buffer[count] = '\0';
                goto finished;
            }

            // Convert byte to two hex characters
            buffer[count++] = b;
        }

        // If fewer than blockSize bytes were returned, slave is out of data
        if (received < blockSize) break;
    }
    finished: 
      buffer[count] = '\0';
      normalSlaveMode();
}

void readDataParsedFromSlaveSerial() {
    if(ci[SLAVE_I2C] < 1) {
      return;
    }
    uint8_t parsedLen = 64;
    uint8_t bytesToReceive = 30;//for now i guess
    // Enable sequential read

    
    uint8_t  offset = 0;
    while(offset < parsedLen) {
      Wire.beginTransmission(ci[SLAVE_I2C]);
      Wire.write(COMMAND_SET_PARSED_OFFSET);
      Wire.write(offset);
      Wire.endTransmission();
      Wire.beginTransmission(ci[SLAVE_I2C]);
      Wire.write(COMMAND_GET_PARSED_SERIAL_DATA);
      Wire.write(bytesToReceive); 
      Wire.endTransmission();
      uint8_t received = Wire.requestFrom(ci[SLAVE_I2C], bytesToReceive);
      uint8_t i = 0;
      while(Wire.available()) {
        char b = Wire.read();
        //Serial.print((int)i + offset);
        //Serial.print(": ");
        //Serial.println((int)b);
        parsedSerialData[i  + offset] = b;
        i++;
      }
      offset += bytesToReceive;
    }


    normalSlaveMode();
}


void saveAllConfigToEEPROM(uint16_t addr) {
    if(ci[SLAVE_I2C] < 1) {
      return;
    }
    int* activeCi;
    char** activeCs;
    
    uint32_t slaveConfigLocation = requestLong(ci[SLAVE_I2C], COMMAND_GET_SLAVE_CONFIG);
    int totalConfigItems = CONFIG_TOTAL_COUNT;
    int totalStringConfigItems = CONFIG_STRING_COUNT;
    activeCi = ci;
    activeCs = cs;
    if(addr >= slaveConfigLocation) {
      totalConfigItems = CONFIG_SLAVE_TOTAL_COUNT;
      totalStringConfigItems = CONFIG_SLAVE_STRING_COUNT;
      activeCi = cis;
      activeCs = css;
    }
    // ============================================================
    // 1. Write marker "DATA"
    // ============================================================
    writeStringToEEPROM(addr, "DATA");
    addr += 5; // includes null terminator

    // ============================================================
    // 2. Write integer config array (ci[])
    // ============================================================
    for (int i = 0; i < totalConfigItems; i++) {
        writeIntToEEPROM(addr, activeCi[i]);
        addr += 2;                // 2 bytes per int
    }

    // ============================================================
    // 3. Write strings (null-terminated)
    // ============================================================
    for (int i = 0; i < totalStringConfigItems; i++) {
        const char* s = activeCs[i];
        if (s == NULL) s = "";
    
        writeStringToEEPROM(addr, s);
        addr += strlen(s) + 1;
    }
}



int loadAllConfigFromEEPROM(int mode, uint16_t addr) { //can also be used to recover values from EEPROM
    if(ci[SLAVE_I2C] < 1) {
      return 0;
    }
    int* activeCi;
    char** activeCs;
    
    uint32_t slaveConfigLocation = requestLong(ci[SLAVE_I2C], COMMAND_GET_SLAVE_CONFIG);
    int totalConfigItems = CONFIG_TOTAL_COUNT;
    int totalStringConfigItems = CONFIG_STRING_COUNT;
    activeCi = ci;
    activeCs = cs;
    //Serial.println((int) slaveConfigLocation);
    //Serial.println((int) addr);
    if(addr >= slaveConfigLocation) {
      totalConfigItems = CONFIG_SLAVE_TOTAL_COUNT;
      totalStringConfigItems = CONFIG_SLAVE_STRING_COUNT;
      activeCi = cis;
      activeCs = css;
    }

    // ============================================================
    // 1. Read marker (must be "DATA")
    // ============================================================
    char marker[5];
    for (int i = 0; i < 5; i++) {
        marker[i] =  readByteFromEEPROM(addr + i); // 1 byte per location
    }
    marker[4] = '\0';

    if (strcmp(marker, "DATA") != 0) {
        // No valid data stored
        return 0;
    }
    

    addr += 5;  // Skip marker + null
 
    // ============================================================
    // 2. Read all integer configs
    // ============================================================
    for (int i = 0; i < totalConfigItems; i++) {
        if(mode == 0) {
          activeCi[i] = readIntFromEEPROM(addr);
        } else if (mode == 1) {
          textOut(String(readIntFromEEPROM(addr)));
          textOut("\n");
          
        }
        addr += 2;
    }

    // ============================================================
    // 3. Read all string configs
    // ============================================================
    for (int i = 0; i < totalStringConfigItems; i++) {
    
        char buffer[128];
        int pos = 0;
    
        while (pos < 127) {
            uint8_t b = (uint8_t)readByteFromEEPROM(addr++);
            buffer[pos++] = b;
            if (b == 0) break;
        }
        buffer[127] = 0;
    
        if (mode == 0) {
            size_t len = strlen(buffer);
    
            if (!activeCs[i]) {
                activeCs[i] = (char*)malloc(len + 1);
            } else if (strlen(activeCs[i]) < len) {
                char* tmp = (char*)realloc(activeCs[i], len + 1);
                if (tmp) activeCs[i] = tmp;
            }
    
            if (activeCs[i]) {
                memcpy(activeCs[i], buffer, len + 1);
            }
        } else if (mode == 1) {
            textOut(buffer);
            textOut("\n");
        }
    }

    if(mode == 0) {
      return 1;
    } else if (mode == 2) {
      return addr;
    }
    return 0;
}

void setAddress(uint16_t addr) {
    if(ci[SLAVE_I2C] < 1) {
      return;
    }
    Wire.beginTransmission((uint8_t)ci[SLAVE_I2C]);
    Wire.write((uint8_t)COMMAND_EEPROM_SETADDR);
    Wire.write((uint8_t)addr & 0xFF);        // low byte
    Wire.write((uint8_t)((addr >> 8) & 0xFF)); // high byte
    Wire.endTransmission();
}

///tests/////////////


void testWrite() {
    if(ci[SLAVE_I2C] < 1) {
      return;
    }
    uint16_t addr = 100;

    // 1. Set address
    setAddress(addr);

    delay(5);

    // 2. Write 4 data bytes
    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(COMMAND_EEPROM_WRITE);
    Wire.write(19);
    Wire.write(29);
    Wire.write(39);
    Wire.write(49);
    Wire.write(59);
    Wire.write(69);
    Wire.endTransmission();

    delay(5);

    // 3. Back to normal
    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(COMMAND_EEPROM_NORMAL);
    Wire.endTransmission();
}
 
void testRead() {
    if(ci[SLAVE_I2C] < 1) {
      return;
    }
    uint16_t addr = 100;
    // 1. Set address for reading
    setAddress(addr);
    delay(5);

    // 2. Enter READ mode
    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(COMMAND_EEPROM_READ);
    Wire.write(4);
    Wire.endTransmission();
    delay(5);

    // 3. Request 4 bytes
    Wire.requestFrom(ci[SLAVE_I2C], 4);
    Serial.println("\n\nEEPROM readback:");
    for (int i = 0; i < 4; i++) {
        if (Wire.available()) {
            byte b = Wire.read();
            Serial.println(b);
        }
    }
    // 4. Back to normal
    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(COMMAND_EEPROM_NORMAL);
    Wire.endTransmission();
}

size_t readBytesFromSlaveSerial( char* buffer, size_t maxLen) {
    if(ci[SLAVE_I2C] < 1) {
      return 0;
    }
    // Put slave into serial-read mode
    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(COMMAND_RETRIEVE_SERIAL_BUFFER);
    Wire.write(maxLen);
    Wire.endTransmission();

    size_t count = 0;
    const uint8_t blockSize = 32;

    while (count < maxLen -1) {   // reserve space for hex + null

        // Request a block of bytes
        uint8_t received = Wire.requestFrom(ci[SLAVE_I2C], blockSize);
        if (received == 0) break;  // no more data from slave
        while (Wire.available() && count < maxLen - 1) {
            char b = Wire.read();
            //Serial.println(b);
            if (b == 0) {          // null terminator from slave
                //Serial.println("endieooo");
                buffer[count] = '\0';
                goto finished;
            }

            // Convert byte to two hex characters
            buffer[count++] = b;
        }

        // If fewer than blockSize bytes were returned, slave is out of data
        if (received < blockSize) break;
    }
    
    finished:
      //buffer[count] = '\0';
      // Tell slave to return to normal mode
      normalSlaveMode();
      return count;
      //Serial.println(buffer);
}

void sendSlaveSerial(String inVal) {
  if (ci[SLAVE_I2C] < 1) {
    return;
  }
  inVal.trim();
  const uint8_t MAX_I2C_PAYLOAD = 31; // 32 - 1 command byte
  char buffer[128];                  // adjust if you expect longer strings
  int totalLen = inVal.length();
  if (totalLen == 0) {
    return;
  }
  // clamp to buffer size
  if (totalLen >= sizeof(buffer)) {
    totalLen = sizeof(buffer) - 1;
  }
  inVal.toCharArray(buffer, sizeof(buffer));
  uint16_t offset = 0;
  while (offset < totalLen) {
    uint8_t chunkLen = totalLen - offset;
    if (chunkLen > MAX_I2C_PAYLOAD) {
      chunkLen = MAX_I2C_PAYLOAD;
    }
    delay(5);
    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(COMMAND_POPULATE_SERIAL_BUFFER);
    Wire.write((uint8_t*)(buffer + offset), chunkLen);
    Wire.endTransmission();
    delay(5);
    offset += chunkLen;
  }
  normalSlaveMode();
}


void normalSlaveMode() {
  if(ci[SLAVE_I2C] < 1) {
    return;
  }
  Wire.beginTransmission(ci[SLAVE_I2C]);
  Wire.write(COMMAND_EEPROM_NORMAL);
  Wire.endTransmission();
}

void enableSlaveSerial(int baudRateSelect) {
  if(ci[SLAVE_I2C] < 1) {
    return;
  }
  Wire.beginTransmission(ci[SLAVE_I2C]);
  Wire.write(COMMAND_SERIAL_SET_BAUD_RATE); //set baud rate
  Wire.write(baudRateSelect); //set slave serial to 115200
  Wire.endTransmission();
}


//////////////////////////////////////////////////////////
uint32_t getParsedSlaveDatum(uint8_t ordinal) {
  if(ci[SLAVE_I2C] < 1) {
    return 0;
  }
  Wire.beginTransmission(ci[SLAVE_I2C]);
  Wire.write(COMMAND_GET_PARSED_DATUM);    // send the command
  Wire.write(ordinal);    // send the ordinal into the slave config array
  Wire.endTransmission();
  yield();
  delay(1);
  Wire.requestFrom(ci[SLAVE_I2C], 4);
  uint32_t value = 0;
  byte buffer[4];
  int i = 0;
  while (Wire.available() && i < 4) {
    byte singleByte = Wire.read();
    //Serial.print("byte: ");
    //Serial.println(singleByte);
    buffer[i++] = singleByte;
  }
  for (int j = 0; j < i; j++) {
    value |= ((long)buffer[j] << (8 * j));
  }
  return value;
}

uint32_t getSlaveConfigItem(uint8_t ordinal) {
  if(ci[SLAVE_I2C] < 1) {
    return 0;
  }
  Wire.beginTransmission(ci[SLAVE_I2C]);
  Wire.write(COMMAND_GET_CONFIG);    // send the command
  Wire.write(ordinal);    // send the ordinal into the slave config array
  Wire.endTransmission();
  yield();
  delay(1);
  Wire.requestFrom(ci[SLAVE_I2C], 4);
  uint32_t value = 0;
  byte buffer[4];
  int i = 0;
  while (Wire.available() && i < 4) {
    byte singleByte = Wire.read();
    //Serial.print("byte: ");
    //Serial.println(singleByte);
    buffer[i++] = singleByte;
  }
  for (int j = 0; j < i; j++) {
    value |= ((long)buffer[j] << (8 * j));
  }
  //Serial.println(value);
  return value;
}

uint32_t getSlaveLong(uint8_t ordinal) {
  if(ci[SLAVE_I2C] < 1) {
    return 0;
  }
  Wire.beginTransmission(ci[SLAVE_I2C]);
  Wire.write(COMMAND_GET_LONG);    // send the command
  Wire.write(ordinal);    // send the ordinal into the slave config array
  Wire.endTransmission();
  yield();
  delay(1);
  Wire.requestFrom(ci[SLAVE_I2C], 4);
  uint32_t value = 0;
  byte buffer[4];
  int i = 0;
  while (Wire.available() && i < 4) {
    byte singleByte = Wire.read();
    //Serial.print("byte: ");
    //Serial.println(singleByte);
    buffer[i++] = singleByte;
  }
  for (int j = 0; j < i; j++) {
    value |= ((long)buffer[j] << (8 * j));
  }
  //Serial.println(value);
  return value;
}


void setSlaveConfigItem(uint8_t ordinal, uint16_t value) {
  if(ci[SLAVE_I2C] < 1) {
    return;
  }
  Wire.beginTransmission(ci[SLAVE_I2C]);
  Wire.write(COMMAND_SET_CONFIG);    // send the command
  Wire.write(ordinal);    // send the ordinal into the slave config array
  uint8_t bytes[2];
  bytes[0] = value & 0xFF;        // LSB
  bytes[1] = (value >> 8) & 0xFF; // MSB
  Wire.write(bytes[0]); 
  Wire.write(bytes[1]); 
  Wire.endTransmission();
  yield();
}

void setSlaveLong(uint8_t ordinal, uint32_t value) {
  if(ci[SLAVE_I2C] < 1) {
    return;
  }
  Wire.beginTransmission(ci[SLAVE_I2C]);
  Wire.write(COMMAND_SET_LONG);    // send the command
  Wire.write(ordinal);    // send the ordinal into the slave config array
 
  uint8_t bytes[4];
  bytes[0] = value & 0xFF;
  bytes[1] = (value >> 8) & 0xFF;
  bytes[2] = (value >> 16) & 0xFF;
  bytes[3] = (value >> 24) & 0xFF;
  for(uint8_t i = 0; i<4; i++) {
    Wire.write(bytes[i]); 
  }

  Wire.endTransmission();
  yield();
}

///////////////////////////////////////////////////////////
//slave functions that accept any i2c address
uint32_t requestLong(byte slaveAddress, byte command) {
  Wire.beginTransmission(slaveAddress);
  Wire.write(command);    // send the command
  Wire.endTransmission();
  yield();
  delay(1);
  Wire.requestFrom(slaveAddress, 4);
  uint32_t value = 0;
  byte buffer[4];
  int i = 0;
  while (Wire.available() && i < 4) {
    buffer[i++] = Wire.read();
  }
  for (int j = 0; j < i; j++) {
    value |= ((long)buffer[j] << (8 * j));
  }
  return value;
}

void sendLong(byte slaveAddress, byte command, uint32_t value) {
    uint8_t bytes[5];
    bytes[0] = command;
    bytes[1] = value & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = (value >> 16) & 0xFF;
    bytes[4] = (value >> 24) & 0xFF;
    Wire.beginTransmission(slaveAddress);
    Wire.write(bytes, 5);
    Wire.endTransmission();
    delay(5);
}

//with this you can send a parameter to a command and get back a value.  i only use this when petting the watchdog to send unixTime and get back millis
uint32_t requestLongWithData(byte slaveAddress, byte command, uint32_t value) {
    uint8_t bytes[5];
    bytes[0] = command;
    bytes[1] = value & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = (value >> 16) & 0xFF;
    bytes[4] = (value >> 24) & 0xFF;
    Wire.beginTransmission(slaveAddress);
    Wire.write(bytes, 5);
    Wire.endTransmission();
    yield();
    delay(1);
    Wire.requestFrom(slaveAddress, 4);
    uint32_t returnValue = 0;
    byte buffer[4];
    int i = 0;
    while (Wire.available() && i < 4) {
      buffer[i++] = Wire.read();
    }
    for (int j = 0; j < i; j++) {
      returnValue |= ((long)buffer[j] << (8 * j));
    }
    return returnValue;
}


///////////////////////////////////
//watchdog functions
void petWatchDog(uint8_t command, uint32_t unixTime) { //also updates unix time if that is set to larger than 0
  //Serial.println(unixTime);
  if(ci[SLAVE_I2C] < 1) {
    return;
  }
  uint32_t slaveMillis = requestLongWithData(ci[SLAVE_I2C], command, unixTime);
  //Serial.println("--------------------------");
  //Serial.print(slaveMillis);
  //Serial.print(" <? ");
  //Serial.println(lastSlaveMillis);
  if(slaveMillis < lastSlaveMillis) { //oh shit, the slave must've rebooted.  We need to resend pin info!
    //Serial.println("gotta resend");
    resendSlavePinInfo = true;
  }
  lastSlaveMillis = slaveMillis;
  //set the global lastPet:
  lastPet = millis();
}

void slaveWatchdogInfo() {
  long ms      = requestLong(ci[SLAVE_I2C], COMMAND_MILLIS); // millis
  long lastReboot = requestLong(ci[SLAVE_I2C], COMMAND_LASTWATCHDOGREBOOT); // last watchdog reboot time
  long rebootCount  = requestLong(ci[SLAVE_I2C], COMMAND_WATCHDOGREBOOTCOUNT); // reboot count
  long lastWePetted  = requestLong(ci[SLAVE_I2C], COMMAND_LASTWATCHDOGPET);
  long lastPetAtBite  = requestLong(ci[SLAVE_I2C], COMMAND_LASTPETATBITE);
  lastSlavePowerMode  = getSlaveConfigItem(SLAVE_POWER_MODE);
  textOut("Watchdog millis: " + String(ms) + "; Last reboot at: " + String(lastReboot) + " (" + msTimeAgo(ms, lastReboot) + "); Reboot count: " + String(rebootCount) + "; Last petted: " + String(lastWePetted) + " (" + msTimeAgo(ms, lastWePetted) + "); Bit " + String(lastPetAtBite) + " seconds after pet\n");
} 

String slaveWatchdogData() {
    long ms      = requestLong(ci[SLAVE_I2C], COMMAND_MILLIS); // millis
    long lastReboot = requestLong(ci[SLAVE_I2C], COMMAND_LASTWATCHDOGREBOOT); // last watchdog reboot time
    long rebootCount  = requestLong(ci[SLAVE_I2C], COMMAND_WATCHDOGREBOOTCOUNT); // reboot count
    long lastWePetted  = requestLong(ci[SLAVE_I2C], COMMAND_LASTWATCHDOGPET);
    long lastPetAtBite  = requestLong(ci[SLAVE_I2C], COMMAND_LASTPETATBITE);
    //also set the global lastSlavePowerMode so we can dial back the calling of this function when we are in a sleep mode
    lastSlavePowerMode  = getSlaveConfigItem(SLAVE_POWER_MODE);
    //Serial.println(lastSlavePowerMode);
    return String(ms) + "*" + String(lastReboot) + "*" + String(rebootCount) + "*" + String(lastWePetted) + "*" + String(lastPetAtBite);
}

//big-endian functions for communicating with the twiboot bootloader. the preceding code is little-endian
////////////////////////////////////////////////////////////////////

// write a big-endian 16-bit value to a slave at a specific "register" address
void i2cWite16(uint8_t slaveAddr, uint8_t reg, uint16_t value) {
    Wire.beginTransmission(slaveAddr);
    Wire.write(reg);                // "register" or address in slave memory
    Wire.write((value >> 8) & 0xFF); // high byte
    Wire.write(value & 0xFF);       // low byte
    Wire.endTransmission();
}
