#include "framfunctions.h"
#include "rootfunctions.h"

//FRAM MEMORY MAP:
/*
GLOBALS:
framIndexAddress:
currentRecordCount:

FRAM MEMORY MAP:
0->ci[FRAM_INDEX_SIZE] * 2: index
top_of_precedin_index->>FRAM_LOG_TOP:  logged records
FRAM_LOG_TOP:      two bytes of the number of FRAM records
FRAM_LOG_TOP + 2:  two bytes of last byte transmitted to backend
FRAM_LOG_TOP + 4:  alternative location for master configuration
*/


//FRAM functions 

void writeRecordToFRAM(const std::vector<std::tuple<uint8_t, uint8_t, double>>& record) {
  //Serial.println(currentRecordCount);
  if (currentRecordCount >= ci[FRAM_INDEX_SIZE]) {
    currentRecordCount = 0;
  }
  uint16_t recordStartAddress = (currentRecordCount == 0) 
    ? framIndexAddress + (ci[FRAM_INDEX_SIZE] * 2)  // Leave space for the index table
    : read16(framIndexAddress + uint16_t((currentRecordCount -1) * 2)) + lastRecordSize + 1; //record.size is 4 now?
  // 🔒 PROTECT CONFIG REGION
  uint16_t nextStart = recordStartAddress + lastRecordSize + 1;
  
  if (nextStart >= ci[FRAM_LOG_TOP]) {
    currentRecordCount = 0;
  
    recordStartAddress = framIndexAddress + (ci[FRAM_INDEX_SIZE] * 2);
  }
  //Serial.println(record.size());
  uint16_t addr = recordStartAddress;
  //Serial.println(addr);
  lastRecordSize = 0;
  for (const auto& [ordinal, type, dbl] : record) {
    uint8_t ordinalTypeArray[2] = {ordinal, type};
    // Write the 1-byte ordinal
    noInterrupts();
    fram.write(addr, ordinalTypeArray, 2); // Write 2 bytes for ordinal and type
    interrupts();
    lastRecordSize += 2;
    addr += 2;
    // Write the value (either float or 4-byte integer based on ordinal)
    if (type == 5) {
      // Write as float
      float temp = (float)dbl; // Non-const copy
      uint8_t* valueBytes = reinterpret_cast<uint8_t*>(&temp);
      noInterrupts();
      fram.write(addr, valueBytes, 4);
      interrupts();
      lastRecordSize += 4;
      addr += 4;
    } else if (type == 2){
      // Write as 4-byte integer
      int32_t asInt = static_cast<int32_t>(dbl); // Convert double to int
      uint8_t valueBytes[4] = {
        (asInt >> 24) & 0xFF,
        (asInt >> 16) & 0xFF,
        (asInt >> 8) & 0xFF,
        asInt & 0xFF
      };
      noInterrupts();
      fram.write(addr, valueBytes, 4);
      interrupts();
      lastRecordSize += 4;
      addr += 4;
    } else if (type == 0){
      // Write as 1-byte integer
      uint8_t asInt = static_cast<uint8_t>(dbl); // Convert double to int
      uint8_t asIntArray[1];
      asIntArray[0] = asInt;
      noInterrupts();
      fram.write(addr, asIntArray, 1);
      interrupts();
      lastRecordSize += 1;
      addr += 1;
    } else if (type == 6) {
      // Write as double
      float temp = (double)dbl; // Non-const copy
      uint8_t* valueBytes = reinterpret_cast<uint8_t*>(&temp);
      noInterrupts();
      fram.write(addr, valueBytes, 8);
      interrupts();
      lastRecordSize += 8;
      addr += 8;
    }
    //ordinal++;
  }
  // Write a delimiter (0xFFFFFFFF) to mark the end of the record
  uint8_t delimiter = 0xFF;
  uint8_t delimiterArray[1] = {delimiter};
  fram.write(addr, delimiterArray, 1);
  //don't count the delimter in lastRecordSize;
  //addr +=  1;
  //Serial.print(" last record size: ");
  //Serial.println((int)lastRecordSize);
  // Update the index table
  uint8_t indexBytes[2] = {uint8_t(recordStartAddress >> 8), uint8_t(recordStartAddress & 0xFF)};
  noInterrupts();
  fram.write(framIndexAddress + currentRecordCount * 2, indexBytes, 2);
  interrupts();
  currentRecordCount++;
  //also store currentRecordCount
  writeRecordCountToFRAM(currentRecordCount);
  //did it save correctly at all?
  //displayFramRecord(currentRecordCount-1);
}

//this retrieves the last FRAM record sent to the backend
uint16_t readLastFramRecordSentIndex() {
  uint8_t countBytes[2];
  noInterrupts();
  fram.read(ci[FRAM_LOG_TOP] + 2, countBytes, 2); // Read 2 bytes from address ci[FRAM_LOG_TOP] + 2
  interrupts();
  delay(10);
  // Reassemble the uint16_t from the high and low bytes
  uint16_t outVal = (static_cast<uint16_t>(countBytes[0]) << 8) | countBytes[1];
  return outVal;
}

//this retrieves the NUMBER of FRAM records, nothing else
uint16_t readRecordCountFromFRAM() {
  uint8_t countBytes[2];
  noInterrupts();
  fram.read(ci[FRAM_LOG_TOP], countBytes, 2); // Read 2 bytes from address ci[FRAM_LOG_TOP]
  interrupts();
  delay(10);
  // Reassemble the uint16_t from the high and low bytes
  uint16_t recordCount = (static_cast<uint16_t>(countBytes[0]) << 8) | countBytes[1];
  return recordCount;
}

//stores the last sent FRAM record index
void writeLastFRAMRecordSentIndex(uint16_t index) {
  uint8_t countBytes[2] = {
    (index >> 8) & 0xFF, // High byte
    index & 0xFF         // Low byte
  };
  //no, i do not know why i had to do this
  uint8_t hi = countBytes[0];
  uint8_t low = countBytes[1];
  noInterrupts();
  delay(100);
  fram.write(ci[FRAM_LOG_TOP] + 2, &hi, 1); // Write 2 bytes 
  fram.write(ci[FRAM_LOG_TOP] + 3, &low, 1); // Write 2 bytes 
  interrupts();
}

//this stores the NUMBER of FRAM records, nothing else
void writeRecordCountToFRAM(uint16_t recordCount) {
  uint8_t countBytes[2] = {
    (recordCount >> 8) & 0xFF, // High byte
    recordCount & 0xFF         // Low byte
  };
  uint8_t hi = countBytes[0];
  uint8_t low = countBytes[1];
  noInterrupts();
  delay(5);
  fram.write(ci[FRAM_LOG_TOP], &hi, 1); // Write 2 bytes 
  fram.write(ci[FRAM_LOG_TOP] + 1, &low, 1); // Write 2 bytes 
  interrupts(); 
  //when we do this, we ALSO want to update lastFramRecordSentIndex so that it will point to the record immediately after the one we just saved so that we can maximize rescued data should we lose the internet
  uint16_t nextRecord = recordCount+1;
  if(nextRecord >= ci[FRAM_INDEX_SIZE])
  {
    nextRecord = 0;
  }
  writeLastFRAMRecordSentIndex(nextRecord);
}

//writes a single record to the backend and changes its delimiter so that it won't be sent again
void sendAStoredRecordToBackend() {
  //first get the last FRAM record sent:
  //Serial.print("Last FRAM record sent: ");
  uint16_t positionInFram = readLastFramRecordSentIndex();
  uint32_t timestamp = 0;
  uint8_t delimiter;
  String weatherDataRowString = makeAsteriskString(19);
  String whereWhenDataRowString = makeAsteriskString(8);
  String powerDataRowString = makeAsteriskString(2);
  if(positionInFram >= ci[FRAM_INDEX_SIZE]){ //it might be too big if it overflows or way off if it was never written
    positionInFram = 0;
  }
  //Serial.println(positionInFram);
  std::vector<std::tuple<uint8_t, uint8_t, double>> record;
  readRecordFromFRAM(positionInFram, record, delimiter);
  if(delimiter == 0xFF) { //we only send records to the backend if this is the delimiter.  after we send it, we update the delimiter to 0xFE
    if(ci[DEBUG] > 1) {
      //Serial.print("Sending FRAM Record at ");
      //Serial.println(positionInFram);
    }
    fRAMRecordsSkipped = 0;
    //we need to assemble a suitable string from the FRAM record
    //the string looks something like: 
    for (const auto& [ordinal, type, value] : record) {
      //Serial.print("ordinal being dealt with: ");
      //Serial.println(ordinal);
      if(ordinal < 32) {  
        weatherDataRowString = replaceNthElement(weatherDataRowString, ordinal, String(value), '*');
      } else if (ordinal >= 32 && ordinal < 48) {
        whereWhenDataRowString = replaceNthElement(whereWhenDataRowString, ordinal-32, String(value), '*');
      } else if (ordinal >= 48 && ordinal < 64) {
        powerDataRowString = replaceNthElement(powerDataRowString, ordinal-48, String(value), '*');
      }
    }
    compileAndSendDeviceData(weatherDataRowString, whereWhenDataRowString, powerDataRowString, true, positionInFram);
  } else {
    positionInFram++;
    //now save the ordinal in ci[FRAM_LOG_TOP] + 2
    fRAMRecordsSkipped++;
    if(fRAMRecordsSkipped > ci[FRAM_INDEX_SIZE]) {
      fRAMRecordsSkipped = 0;
      haveReconnected = false; //we sent all the stored records, so we're all done being reconnected
    }
    writeLastFRAMRecordSentIndex(positionInFram);
  }
}

void changeDelimiterOnRecord(uint16_t index, uint8_t newDelimiter) {
  uint16_t indexAddress = framIndexAddress + (index * 2);
  uint16_t location = read16(indexAddress);
  yield();
  uint8_t bytesPerLine = getRecordSizeFromFRAM(location);
  if(ci[DEBUG] > 1) {
    Serial.print(F("Bytes per line: "));
    Serial.println(bytesPerLine);
    Serial.print(F("index in: "));
    Serial.println(index);
  }
  //bytes per line should be 25
  uint8_t buffer[bytesPerLine];   // Buffer to hold data for one line
  //displayFramRecord(index);
  //hexDumpFRAM(location, bytesPerLine, 5);
  fram.read(location, buffer, bytesPerLine); 
  //Serial.print("delimiter found: ");
  //Serial.println(buffer[bytesPerLine-1], HEX);
  buffer[bytesPerLine-1] = newDelimiter;
  noInterrupts();
  delay(5);
  fram.write(location, buffer, bytesPerLine); 
  interrupts();
  //index++;
  //now save the ordinal in ci[FRAM_LOG_TOP] + 2
  writeLastFRAMRecordSentIndex(index + 1);
}

//FRAM MEMORY MAP:
/*
ci[FRAM_LOG_TOP]: number of FRAM records
ci[FRAM_LOG_TOP] + 2: ordinal of last FRAM record sent to backend
*/

uint16_t read16(uint16_t addr) {
  uint8_t bytes[2];
  fram.read(addr, bytes, 2); // Read 2 bytes
  return (uint16_t(bytes[0]) << 8) | uint16_t(bytes[1]);
}

void readRecordFromFRAM(uint16_t recordIndex, std::vector<std::tuple<uint8_t, uint8_t, double>>& record, uint8_t & delimiter) {
  //was causing lots of out of memory runtime errors
  uint16_t indexAddress = framIndexAddress + (recordIndex * 2);
  uint16_t recordStartAddress = read16(indexAddress);
  record.clear();
  record.reserve(200);
  uint16_t addr = recordStartAddress;
  while (addr < ci[FRAM_LOG_TOP]) {
    uint8_t ordinal;
    uint8_t type;
    fram.read(addr, &ordinal, 1); // Read 1 byte for the ordinal
    addr += 1;
    fram.read(addr, &type, 1); // Read 1 byte for the type
    addr += 1;
    if (ordinal >= 0xF0) {
      delimiter = ordinal; // Correctly assign to reference parameter
      break; // End of record... we allow a variety of end delimiters to encode record status
    }
    cleanup();   
    delay(5);
    if (type == 5) {
      uint8_t valueBytes[4];
      fram.read(addr, valueBytes, 4);
      addr += 4;
      float dbl;
      //ESP.getMaxFreeBlockSize();
      memcpy(&dbl, valueBytes, 4);
      record.emplace_back(std::make_tuple(ordinal, type, (double)dbl));
    } else if (type == 2) {
      uint8_t valueBytes[4];
      fram.read(addr, valueBytes, 4);
      addr += 4;
      int32_t value = (valueBytes[0] << 24) | (valueBytes[1] << 16) | (valueBytes[2] << 8) | valueBytes[3];
      record.emplace_back(std::make_tuple(ordinal, type, static_cast<double>(value)));
    } else if (type == 0) {
      uint8_t valueBytes[1];
      fram.read(addr, valueBytes, 1);
      addr += 1;
      int32_t value = valueBytes[0];
      record.emplace_back(std::make_tuple(ordinal, type, static_cast<double>(value)));
    } else if (type == 6) { // Read double
      uint8_t valueBytes[8];
      fram.read(addr, valueBytes, 8);
      double dbl;
      dumpMemoryStats(68);
      memcpy(&dbl, valueBytes, 8);
      dumpMemoryStats(69);
      record.emplace_back(std::make_tuple(ordinal, type, (double)dbl));
      addr += 8;
    }
  }
}

//if you pass in 0xFFFF it gets the length of the latest record
uint16_t getRecordSizeFromFRAM(uint16_t recordStartAddress) {
  if (currentRecordCount == 0) {
    //Serial.println("No records exist.");
    return 0;
  }
  // Locate the last record's start address using the index table
  uint16_t lastRecordIndex = currentRecordCount - 1;
  uint16_t indexAddress = framIndexAddress + (lastRecordIndex * 2);
  if(recordStartAddress == 0xFFFF) {
    recordStartAddress = read16(indexAddress);
  }
  uint16_t size = 0;
  while (size + recordStartAddress < ci[FRAM_LOG_TOP]) {
    yield();
    uint8_t readBytes[2];
    fram.read(recordStartAddress + size, readBytes, 2); // Read ordinal (1 byte)
    uint8_t ordinal = readBytes[0];
    if (ordinal >= 0xF0) { // Found the delimiter (end of record) ... we allow a variety of end delimiters to encode record status 
      size++;
      return size;
      break;
    }
    size += 2;
    uint8_t type = readBytes[1];
    if (type == 5 || type == 2) {
      // Float (4 bytes)
      size += 4;
    } else if (type == 0) {
      // 1-byte integer
      size += 1;
    } else if (type == 6) {
      // Double (8 bytes)
      size += 8;
    }
  }
  //Serial.print("Last record size: ");
  //Serial.println(size);
  return size;
}

void clearFramLog(){
  currentRecordCount = 0;
  writeRecordCountToFRAM(currentRecordCount);
  textOut("Current record cursor reset\n");
}

void displayFramRecord(uint16_t recordIndex) { //want to get rid of after testing!
    // Read the record from FRAM
    std::vector<std::tuple<uint8_t, uint8_t, double>> record;
    uint8_t delimiter;
    //dumpMemoryStats(9);
    readRecordFromFRAM(recordIndex, record, delimiter);
    //dumpMemoryStats(10);
    // Display the record
    textOut("Record #");    
    textOut(String(recordIndex));
    textOut(":\n");
    for (const auto& [ordinal, type, value] : record) {
      textOut("  Ordinal: ");
      textOut(String(ordinal));
      //Serial.print("  Type: ");
      //Serial.print(type);
      textOut(", Value: ");
      if(type == 5) {
        textOut(String(value, 3)); // Print with 3 decimal places
      } else if (type == 6) {
        textOut(String(value, 12)); // Print with 12 decimal places
      } else if (type == 0 || type == 2) {
        textOut(String(value, 0)); // Print with 0 decimal places
      }
      textOut("\n");
    }
    textOut("  Delimiter: ");
    textOut(String(delimiter, HEX));
    textOut("\n"); // Add spacing between records
    //hexDumpFRAM(read16(recordIndex), 28, 5);
}

void displayAllFramRecords() { //want to get rid of after testing!
  textOut("Reading all records from FRAM:");
  textOut("Record Count: ");
  textOut(String(currentRecordCount));
  textOut("\n");
  for (uint16_t i = 0; i < currentRecordCount; i++) {
    displayFramRecord(i);
    textOut("\n"); // Add spacing between records
  }
  textOut("All records displayed.\n");
}

void dumpFramRecordIndexes() {  
  uint8_t bytes[2];
  for(int i=0; i<currentRecordCount; i++) {
    int index = read16(uint16_t (i*2));
    textOut(String(i));
    textOut(": ");
    textOut(String(index));
    textOut("\n");
  }
}

void hexDumpFRAMAtIndex(uint16_t index, uint8_t bytesPerLine, uint16_t maxLines) {
  uint16_t indexAddress = framIndexAddress + (index * 2);
  uint16_t address = read16(indexAddress);
  uint8_t buffer[bytesPerLine];   // Buffer to hold data for one line
  
  for (uint16_t line = 0; line < maxLines; ++line) {
    // Read a line's worth of data from FRAM
    fram.read(address, buffer, bytesPerLine);
    
    // Print the address with leading zeros
    textOut("0x");
    if (address < 0x1000) textOut("0");
    if (address < 0x0100) textOut("0");
    if (address < 0x0010) textOut("0");
    textOut(String(address, HEX));
    textOut(": ");
    
    // Print the data in hexadecimal format with leading zeros
    for (uint8_t i = 0; i < bytesPerLine; ++i) {
      if (address + i < 0xFFFF) { // Ensure we don't overflow the 16-bit address space
        if (buffer[i] < 0x10) textOut("0"); // Add leading zero if less than 0x10
        textOut(String(buffer[i], HEX));
        textOut(" ");
      } else {
        break; // Stop if address exceeds FRAM bounds
      }
    }
    textOut("\n"); // Move to the next line
    address += bytesPerLine; // Increment the address for the next line
    // Stop if we've reached the end of FRAM memory
    if (address >= 0xFFFF) {
      break;
    }
  }
}

void hexDumpFRAM(uint16_t startAddress, uint8_t bytesPerLine, uint16_t maxLines) {
  uint16_t address = startAddress; // Initialize with the starting address
  uint8_t buffer[bytesPerLine];   // Buffer to hold data for one line
  
  for (uint16_t line = 0; line < maxLines; ++line) {
    // Read a line's worth of data from FRAM
    fram.read(address, buffer, bytesPerLine);
    
    // Print the address with leading zeros
    textOut("0x");
    if (address < 0x1000) textOut("0");
    if (address < 0x0100) textOut("0");
    if (address < 0x0010) textOut("0");
    textOut(String(address, HEX));
    textOut(": ");
    
    // Print the data in hexadecimal format with leading zeros
    for (uint8_t i = 0; i < bytesPerLine; ++i) {
      if (address + i < 0xFFFF) { // Ensure we don't overflow the 16-bit address space
        if (buffer[i] < 0x10) textOut("0"); // Add leading zero if less than 0x10
        textOut(String(buffer[i], HEX));
        textOut(" ");
      } else {
        break; // Stop if address exceeds FRAM bounds
      }
    }
    textOut("\n"); // Move to the next line
    address += bytesPerLine; // Increment the address for the next line
    // Stop if we've reached the end of FRAM memory
    if (address >= 0xFFFF) {
      break;
    }
  }
}

void swapFRAMContents(uint16_t location1, uint16_t location2, uint16_t length) {
  if (length == 0) {
    textOut("Invalid length: 0\n");
    return;
  }
  uint8_t buffer1[length];
  uint8_t buffer2[length];
  fram.read(location1, buffer1, length); // Read `length` bytes from location1 into buffer1
  fram.read(location2, buffer2, length); // Read `length` bytes from location2 into buffer2

  // Swap the data by writing back to the other location
  fram.write(location1, buffer2, length); // Write buffer2's data to location1
  fram.write(location2, buffer1, length); // Write buffer1's data to location2

  textOut("Swap completed\n");
}

void addOfflineRecord(std::vector<std::tuple<uint8_t, uint8_t, double>>& record, uint8_t ordinal, uint8_t type, double value) {
  if (!isnan(value)) {
    record.emplace_back(std::make_tuple(ordinal, type, value));  // ? Safer
  }
}

//////////////////////////////////////
//if we save config in FRAM:
/////////////////////////////////////

void saveAllConfigToFRAM(uint16_t addr) {
    addr = ci[FRAM_LOG_TOP] + 4; 

    int* activeCi = ci;
    char** activeCs = cs;

    int totalConfigItems = CONFIG_TOTAL_COUNT;
    int totalStringConfigItems = CONFIG_STRING_COUNT;

    // ============================================================
    // 1. Write marker "DATA"
    // ============================================================
    const char* marker = "DATA";
    fram.write(addr, (uint8_t*)marker, 5); // includes null
    addr += 5;

    // ============================================================
    // 2. Write integer config array (ci[])
    // ============================================================
    for (int i = 0; i < totalConfigItems; i++) {
        uint8_t bytes[2] = {
            (activeCi[i] >> 8) & 0xFF,
            activeCi[i] & 0xFF
        };
        fram.write(addr, bytes, 2);
        addr += 2;
    }

    // ============================================================
    // 3. Write strings (null-terminated)
    // ============================================================
    for (int i = 0; i < totalStringConfigItems; i++) {
        const char* s = activeCs[i];
        if (s == NULL) s = "";

        size_t len = strlen(s) + 1;
        fram.write(addr, (uint8_t*)s, len);
        addr += len;
    }
}

int loadAllConfigFromFRAM(int mode, uint16_t addr) {
    addr = ci[FRAM_LOG_TOP] + 4;
    int* activeCi = ci;
    char** activeCs = cs;
    int totalConfigItems = CONFIG_TOTAL_COUNT;
    int totalStringConfigItems = CONFIG_STRING_COUNT;
    // ============================================================
    // 1. Read marker
    // ============================================================
    char marker[5];
    fram.read(addr, (uint8_t*)marker, 5);
    if (strcmp(marker, "DATA") != 0) {
        if (mode == 1) {
          textOut(F("No data found"));
    
        }
        return 0;
    }
    addr += 5;
    // ============================================================
    // 2. Read integer configs
    // ============================================================
    for (int i = 0; i < totalConfigItems; i++) {
        uint8_t bytes[2];
        fram.read(addr, bytes, 2);

        int val = (bytes[0] << 8) | bytes[1];

        if (mode == 0) {
            activeCi[i] = val;
        } else if (mode == 1) {
            textOut(String(val));
            textOut("\n");
        }
        addr += 2;
    }
    // ============================================================
    // 3. Read string configs
    // ============================================================
    for (int i = 0; i < totalStringConfigItems; i++) {
        char buffer[128];
        int pos = 0;
        while (pos < 127) {
            uint8_t b;
            fram.read(addr, &b, 1);
            addr++;
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
    if (mode == 0) {
        return 1;
    } else if (mode == 2) {
        return addr;
    }
    return 0;
}
