
#ifndef FRAMFUNCTIONS_H
#define FRAMFUNCTIONS_H
#include "utilities.h"
#include "i2cslave.h"
#include "config.h"
#include "globals.h"
#include <tuple>
#include <cmath> 
#include <math.h> // for isnan, NAN
#include <map>
#include <vector>



void writeRecordToFRAM(const std::vector<std::tuple<uint8_t, uint8_t, double>>& record);
uint16_t readLastFramRecordSentIndex();
uint16_t readRecordCountFromFRAM();
void writeLastFRAMRecordSentIndex(uint16_t index);
void writeRecordCountToFRAM(uint16_t recordCount);
void sendAStoredRecordToBackend();
void changeDelimiterOnRecord(uint16_t index, uint8_t newDelimiter);
uint16_t read16(uint16_t addr);
void readRecordFromFRAM(uint16_t recordIndex, std::vector<std::tuple<uint8_t, uint8_t, double>>& record, uint8_t & delimiter);
uint16_t getRecordSizeFromFRAM(uint16_t recordStartAddress);





void clearFramLog();
void displayFramRecord(uint16_t recordIndex);
void displayAllFramRecords();
void dumpFramRecordIndexes();
void hexDumpFRAMAtIndex(uint16_t index, uint8_t bytesPerLine, uint16_t maxLines);
void hexDumpFRAM(uint16_t startAddress, uint8_t bytesPerLine, uint16_t maxLines);
void swapFRAMContents(uint16_t location1, uint16_t location2, uint16_t length);
void addOfflineRecord(std::vector<std::tuple<uint8_t, uint8_t, double>>& record, uint8_t ordinal, uint8_t type, double value);


#endif
