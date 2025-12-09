//header for a library of functions to communicate with the i2cslave
#ifndef UTILITIES_H
#define UTILITIES_H
#include "globals.h"
#include <stdint.h>
#include <Wire.h>
#include "config.h"     // <-- gives access to ci[] and cs[]
#include <SimpleMap.h>
#include <Arduino.h>
#include <tuple>
#include <cmath> 
#include <math.h> // for isnan, NAN
 
void textOut(String data);
String makeAsteriskString(uint8_t number);
void dumpMemoryStats(int marker);
String urlEncode(String str, bool minimizeImpact);
String joinValsOnDelimiter(uint32_t vals[], String delimiter, int numberToDo);
String joinMapValsOnDelimiter(SimpleMap<String, int> *pinMap, String delimiter);
static int appendNullOrNumber(char *buf, size_t bufSize, size_t pos, double val, const char *fmt);
static int appendNullOrInt(char *buf, size_t bufSize, size_t pos, long val);
String nullifyOrNumber(double inVal);
String nullifyOrInt(int inVal);
void shiftArrayUp(uint32_t array[], uint32_t newValue, int arraySize);
void splitString(const String& input, char delimiter, String* outputArray, int arraySize);
String replaceFirstOccurrenceAtChar(String str1, String str2, char atChar);
String replaceNthElement(const String& input, int n, const String& replacement, char delimiter);
byte calculateChecksum(String input);
byte countSetBitsInString(const String &input);
byte countZeroes(const String &input);
uint8_t rotateLeft(uint8_t value, uint8_t count);
uint8_t rotateRight(uint8_t value, uint8_t count);
String encryptStoragePassword(String datastring);
long requestLong(byte slaveAddress, byte command);


#endif
