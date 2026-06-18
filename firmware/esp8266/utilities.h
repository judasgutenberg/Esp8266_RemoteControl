//header for a library of functions to communicate with the i2cslave
#ifndef UTILITIES_H
#define UTILITIES_H
#include "globals.h"
#include "rootfunctions.h"
#include <stdint.h>
#include <Wire.h>
#include "config.h"     // <-- gives access to ci[] and cs[]
#include <Arduino.h>
#include <tuple>
#include <cmath> 
#include <math.h> // for isnan, NAN
#include <map>
#include <WebSocketsClient.h>


void textOut(String data);
String numericEquivalents(String inVal);
String makeAsteriskString(uint8_t number);
void logHeap(const char* label);
void dumpMemoryStats(int marker);
String urlEncode(String str, bool minimizeImpact);
String joinValsOnDelimiter(uint32_t vals[], String delimiter, int numberToDo);
String joinValsOnDelimiter(uint16_t vals[], String delimiter, int numberToDo);
String joinPinMapValsOnDelimiter(String delimiter) ;
//String joinMapValsOnDelimiter(SimpleMap<String, int> *pinMap, String delimiter);
bool isInteger(const String &s);
static int appendNullOrNumber(char *buf, size_t bufSize, size_t pos, double val, const char *fmt);
static int appendNullOrInt(char *buf, size_t bufSize, size_t pos, long val);
String nullifyOrNumber(double inVal);
String nullifyOrInt(int inVal);
void shiftArrayUp(uint32_t array[], uint32_t newValue, int arraySize);
String extractFilename(String url);
String pinDescription(String pinString);
int splitStringToCharArrays(char *input, char delim, char outArray[][60], int maxParts);
int splitString(const String& input, char delimiter, String* outputArray, int arraySize);
String replaceFirstOccurrenceAtChar(String str1, String str2, char atChar);
String replaceNthElement(const String& input, int n, const String& replacement, char delimiter);
byte calculateChecksum(String input);
byte countSetBitsInString(const String &input);
byte countZeroes(const String &input);
uint8_t rotateLeft(uint8_t value, uint8_t count);
uint8_t rotateRight(uint8_t value, uint8_t count);
String encryptStoragePassword(String datastring);
uint32_t setSerialRate(byte baudRateLevel);

time_t parseDateTime(String dateTime);
String msTimeAgo(uint32_t base, uint32_t millisFromPast);
String msTimeAgo(uint32_t millisFromPast);
String timeAgo(String sqlDateTime);
String timeAgo(String sqlDateTime, time_t compareTo);
String humanReadableTimespan(uint32_t diffInSeconds);
void bytesToHex(const uint8_t* data, size_t length, char spacerChar, char* outBuffer);

bool urlExists(const char* url);






#endif
