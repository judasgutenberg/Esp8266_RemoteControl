
#ifndef RTCFUNCTIONS_H
#define RTCFUNCTIONS_H
#include "utilities.h"
#include "i2cslave.h"
#include "config.h"
#include "globals.h"
#include <tuple>
#include <cmath> 
#include <math.h> // for isnan, NAN
#include <map>
#include <vector>



extern const int _ytab[2][12];
byte decToBcd(byte val);
byte bcdToDec(byte val);
void syncRTCWithNTP();
uint32_t getSecsSinceEpoch(uint16_t epoch, uint8_t month, uint8_t day, uint8_t years, uint8_t hour, uint8_t minute, uint8_t second);
void setDateDs1307(byte second,        // 0-59
                   byte minute,        // 0-59
                   byte hour,          // 1-23
                   byte dayOfWeek,     // 1-7
                   byte dayOfMonth,    // 1-28/29/30/31
                   byte month,         // 1-12
                   byte year);          // 0-99
void getDateDs1307(byte *second,
          byte *minute,
          byte *hour,
          byte *dayOfWeek,
          byte *dayOfMonth,
          byte *month,
          byte *year);
void printRTCDate();
uint32_t currentRTCTimestamp();
 






#endif
