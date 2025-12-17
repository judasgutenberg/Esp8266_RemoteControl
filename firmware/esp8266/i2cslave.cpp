//a library of functions for a master to communicate with the i2cslave
#include "globals.h"
#include <Wire.h>
#include "i2cslave.h"
#include "utilities.h"
#include <Arduino.h>

#define COMMAND_EEPROM_SETADDR 150
#define COMMAND_EEPROM_WRITE   151
#define COMMAND_EEPROM_READ    152
#define COMMAND_EEPROM_NORMAL  153

//housekeeping functions
#define COMMAND_VERSION 160
#define COMMAND_COMPILEDATETIME 161
#define COMMAND_TEMPERATURE 162
#define COMMAND_FREEMEMORY 163
#define COMMAND_GET_SLAVE_CONFIG 164

//serial commands
#define COMMAND_SERIAL_SET_BAUD_RATE 170
#define COMMAND_RETRIEVE_SERIAL_BUFFER 171
#define COMMAND_POPULATE_SERIAL_BUFFER 172

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
        Wire.beginTransmission(ci[SLAVE_I2C]);
        Wire.write(COMMAND_EEPROM_WRITE);

        // send up to 31 bytes per transaction
        uint8_t bytesThisChunk = 0;
        for (; bytesThisChunk < 31; bytesThisChunk++) {
            Wire.write(*p);
            if (*p == 0) { // write null terminator and stop
                break;
            }
            p++;
        }

        Wire.endTransmission();
        delay(5);

        // stop outer loop if null terminator was sent
        if (*(p) == 0) break;

        // in case the string length exceeds 31, continue in next iteration
        p++; // move past last byte sent
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
  if(ci[SLAVE_I2C] < 1) {
    return;
  }
  inVal.trim(); 
  char buffer[50];    
  inVal.toCharArray(buffer, sizeof(buffer));
 
  delay(5);
  Wire.beginTransmission(ci[SLAVE_I2C]);
  Wire.write(COMMAND_POPULATE_SERIAL_BUFFER);
  int stringLen = inVal.length();
  if(stringLen > 30) {
    stringLen = 30;
  }
  Wire.write(buffer, stringLen); 
  Wire.endTransmission();
  delay(5);
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

void petWatchDog(uint8_t command, uint32_t unixTime) { //also updates unix time if that is set to larger than 0
  //Serial.println(unixTime);
  if(ci[SLAVE_I2C] < 1) {
    return;
  }
  sendLong(ci[SLAVE_I2C], command, unixTime);
}
