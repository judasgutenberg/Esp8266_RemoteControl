//header for a library of functions to communicate with the i2cslave
#ifndef EEPROM_CONFIG_H
#define EEPROM_CONFIG_H
#include "globals.h"
#include <stdint.h>
#include <Wire.h>
#include "config.h"     // <-- gives access to ci[] and cs[]
#include "utilities.h"


void saveAllConfigToEEPROM(uint16_t addr);
void readDataParsedFromSlaveSerial();
int loadAllConfigFromEEPROM(int mode, uint16_t addr);
int readIntFromEEPROM(uint16_t addr);
long readLongFromEEPROM(uint16_t addr);
void writeIntToEEPROM(uint16_t addr, int value);
void writeStringToEEPROM(uint16_t addr, const char* s);
void testWrite();
void testRead();
void setAddress(uint16_t addr);
void readStringFromSlaveEEPROM(uint16_t addr, char* buffer, size_t maxLen);
void readBytesFromSlaveEEPROM(uint16_t addr, char* buffer, size_t maxLen);
size_t readBytesFromSlaveSerial( char* buffer, size_t maxLen);
void sendSlaveSerial(String inVal);
void normalSlaveMode();
void enableSlaveSerial(int baudSelector);


uint32_t getParsedSlaveDatum(uint8_t ordinal);
uint32_t getSlaveConfigItem(uint8_t ordinal);
uint32_t getSlaveLong(uint8_t ordinal);
void setSlaveConfigItem(uint8_t ordinal, uint16_t value);
void setSlaveLong(uint8_t ordinal, uint32_t value);

uint32_t requestLong(byte slaveAddress, byte command);
void sendLong(byte slaveAddress, byte command, uint32_t value);
uint32_t requestLongWithData(byte slaveAddress, byte command, uint32_t value);

void petWatchDog(uint8_t command, uint32_t unixTime);
void slaveWatchdogInfo();
String slaveWatchdogData();

#endif
