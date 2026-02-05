/////////////////////////////////////////////
//slave reflasher code -- it can reflash an I2C slave over I2C!  You must set fuses and install twiboot:
/*
the commands would be something like this for Atmega328p
.\avrdude.exe -c usbtiny -p m328p -U lfuse:w:0xC2:m -U hfuse:w:0xD8:m -U efuse:w:0xFD:m
.\avrdude.exe -c usbtiny -p m328p -U flash:w:twiboot.hex:i

*/
/////////////////////////////////////////////
#ifndef SLAVEUPDATE_CONFIG_H
#define SLAVEUPDATE_CONFIG_H
#include "i2cslave.h"
#include "globals.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>


 
extern bool pagePending;   // true when current page has unsent data
uint8_t hexToByte(String hex);
bool sendFlashPage(uint32_t pageAddr, uint8_t *data, bool debug);
void flushLastPage(bool debug);
void processHexLine(String line, bool debug);
void streamHexFile(Stream *stream, bool debug = true);
void updateSlaveFirmware(String url);
void finalizeBootloaderUpdate(bool debug);
void runSlaveSketch();
void enterSlaveBootloader();
void leaveSlaveBootloader();



#endif
