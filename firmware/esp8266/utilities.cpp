 
#include "utilities.h"
 

////////////////////////////
//utilities//
////////////////////////////


void textOut(String data){
  if(outputMode == 2) {
    //sendRemoteData(data, "commandout", 0xFFFF); //do this in loop, not now
    responseBuffer += data;
  } else {
    Serial.print(data);
  }
}

String makeAsteriskString(uint8_t number){
  String out = "";
  for(uint8_t i=0; i<number; i++) {
    out += "*";
  }
  return out;
}

void dumpMemoryStats(int marker){
  char buffer[80]; 
  sprintf(buffer, "Memory (%d): Free=%d, MaxBlock=%d, Fragmentation=%d%%\n", marker,
                  ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(),
                  100 - (ESP.getMaxFreeBlockSize() * 100 / ESP.getFreeHeap()));
  textOut(String(buffer));
}


String urlEncode(String str, bool minimizeImpact) {
  String encodedString = "";
  char c;
  char hexDigits[] = "0123456789ABCDEF"; // Hex conversion lookup
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += "%20";
    } else if (isalnum(c) || c == '.' || (minimizeImpact && ( c == '|'  || c == '*' ||  c == ','))) {
      encodedString += c;
    } else {
      encodedString += '%';
      encodedString += hexDigits[(c >> 4) & 0xF];
      encodedString += hexDigits[c & 0xF];
    }
  }
  return encodedString;
}

String joinValsOnDelimiter(uint32_t vals[], String delimiter, int numberToDo) {
  String out = "";
  for(int i=0; i<numberToDo; i++){
    out = out + (String)vals[i];
    if(i < numberToDo-1) {
      out = out + delimiter;
    }
  }
  return out;
}

String joinMapValsOnDelimiter(SimpleMap<String, int> *pinMap, String delimiter) {
  String out = "";
  for (int i = 0; i < pinMap->size(); i++) {
    //i had to rework this because pinMap was saving out of order in some cases
    String key = pinList[i];
    out = out + (String)pinMap->get(key);
    if(i < pinMap->size()- 1) {
      out = out + delimiter;
    }
  }
  return out;
}

static int appendNullOrNumber(char *buf, size_t bufSize, size_t pos, double val, const char *fmt) {
  if (pos >= (int)bufSize) return pos;
  if (isnan(val)) {
    // append nothing (empty field)
    return pos; 
  } else {
    int n = snprintf(buf + pos, bufSize - pos, fmt, val);
    if (n < 0) return pos;
    pos += n;
    return pos;
  }
}

static int appendNullOrInt(char *buf, size_t bufSize, size_t pos, long val) {
  if (pos >= (int)bufSize) return pos;
  if (val < 0) {
    return pos;
  } else {
    int n = snprintf(buf + pos, bufSize - pos, "%ld", val);
    if (n < 0) return pos;
    pos += n;
    return pos;
  }
}

String nullifyOrNumber(double inVal) {
  if(isnan(inVal)) {
    return "";
  } else {
    return String(inVal);
  }
}

String nullifyOrInt(int inVal) {
  if(isnan(inVal)) {
    return "";
  } else {
    return String(inVal);
  }
}

void shiftArrayUp(uint32_t array[], uint32_t newValue, int arraySize) {
    // Shift elements down by one index
    for (int i =  1; i < arraySize ; i++) {
        array[i - 1] = array[i];
    }
    // Insert the new value at the beginning
    array[arraySize - 1] = newValue;
}




int splitStringToCharArrays(char *input, char delim, char outArray[][50], int maxParts) {
    if (!input || maxParts <= 0) return 0;

    int partCount = 0;
    char *start = input;
    char *p = input;

    while (*p != '\0' && partCount < maxParts) {
        if (*p == delim) {
            int len = p - start;
            if (len >= 64) len = 63; // truncate to fit buffer
            strncpy(outArray[partCount], start, len);
            outArray[partCount][len] = '\0';
            partCount++;
            start = p + 1;
        }
        p++;
    }

    // Copy the final segment
    if (partCount < maxParts) {
        int len = p - start;
        if (len >= 64) len = 63;
        strncpy(outArray[partCount], start, len);
        outArray[partCount][len] = '\0';
        partCount++;
    }

    return partCount;
}

void splitString(const String& input, char delimiter, String* outputArray, int arraySize) {
  int lastIndex = 0;
  int count = 0;
  for (int i = 0; i < input.length(); i++) {
    if (input.charAt(i) == delimiter) {
      // Extract the substring between the last index and the current index
      outputArray[count++] = input.substring(lastIndex, i);
      lastIndex = i + 1;
      if (count >= arraySize) {
        break;
      }
    }
  }
  // Extract the last substring after the last delimiter
  outputArray[count++] = input.substring(lastIndex);
}

String replaceFirstOccurrenceAtChar(String str1, String str2, char atChar) { //thanks ChatGPT!
    int index = str1.indexOf(atChar);
    if (index == -1) {
        // If there's no '|' in the first string, return it unchanged.
        return str1;
    }
    String beforeDelimiter = str1.substring(0, index); // Part before the first '|'
    String afterDelimiter = str1.substring(index + 1); // Part after the first '|'

    // Construct the new string with the second string inserted.
    String result = beforeDelimiter + "|" + str2 + "|" + afterDelimiter;
    return result;
}

String replaceNthElement(const String& input, int n, const String& replacement, char delimiter) {
  String result = "";
  int currentIndex = 0;
  int start = 0;
  int end = input.indexOf(delimiter);

  while (end != -1 || start < input.length()) {
    if (currentIndex == n) {
      // Replace the nth element
      result += replacement;
    } else {
      // Append the current element
      result += input.substring(start, end != -1 ? end : input.length());
    }
    if (end == -1) break; // No more delimiters
    result += delimiter; // Append the delimiter
    start = end + 1;     // Move to the next element
    end = input.indexOf(delimiter, start);
    currentIndex++;
  }

  // If n is out of range, return the original string
  if (n >= currentIndex) {
    return input;
  }
  return result;
}

byte calculateChecksum(String input) {
    byte checksum = 0;
    for (int i = 0; i < input.length(); i++) {
        checksum += input[i];
    }
    return checksum;
}

byte countSetBitsInString(const String &input) {
    byte bitCount = 0;
    // Iterate over each character in the string
    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];
        
        // Count the set bits in the ASCII value of the character
        for (int bit = 0; bit < 8; ++bit) {
            if (c & (1 << bit)) {
                bitCount++;
            }
        }
    }
    return bitCount;
}


byte countZeroes(const String &input) {
    byte zeroCount = 0;
    // Iterate over each character in the string
    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];
        // Count the zeroes
        if (c == 48) {
            zeroCount++;
        }
   
    }
    return zeroCount;
}

uint8_t rotateLeft(uint8_t value, uint8_t count) {
    return (value << count) | (value >> (8 - count));
}

uint8_t rotateRight(uint8_t value, uint8_t count) {
    return (value >> count) | (value << (8 - count));
}


//let's define how cs[ENCRYPTION_SCHEME] works:
//a data_string is passed in as is the cs[STORAGE_PASSWORD].  we want to be able to recreate the cs[STORAGE_PASSWORD] server-side, so no can be lossy on the cs[STORAGE_PASSWORD]
//it's all based on nibbles. two operations are performed by byte from these two nibbles, upper nibble first, then lower nibble
//00: do nothing (byte unchanged)
//01: xor cs[STORAGE_PASSWORD] at this position with count_of_zeroes in dataString
//02: xor between cs[STORAGE_PASSWORD] at this position and timestampString at this position.
//03: xor between cs[STORAGE_PASSWORD] at this position and data_string  at this position.
//04: xor cs[STORAGE_PASSWORD] with ROL data_string at this position
//05: xor cs[STORAGE_PASSWORD] with ROR data_string at this position
//06: xor cs[STORAGE_PASSWORD] with checksum of entire data_string;
//07: xor cs[STORAGE_PASSWORD] with checksum of entire stringified timestampString
//08: xor cs[STORAGE_PASSWORD] with set_bits of entire data_string
//09  xor cs[STORAGE_PASSWORD] with set_bits in entire timestampString
//10: ROL this position of cs[STORAGE_PASSWORD] 
//11: ROR this position of cs[STORAGE_PASSWORD] 
//12: ROL this position of cs[STORAGE_PASSWORD] twice
//13: ROR this position of cs[STORAGE_PASSWORD] twice
//14: invert byte of cs[STORAGE_PASSWORD] at this position (zeroes become ones and ones become zeroes)
//15: xor cs[STORAGE_PASSWORD] at this position with count_of_zeroes in full timestampString
String encryptStoragePassword(String datastring) {
    int timeStamp = timeClient.getEpochTime();
    char buffer[10];
    itoa(timeStamp, buffer, 10);  // Convert timestamp to string
    String timestampStringInterim = String(buffer);
    String timestampString = timestampStringInterim.substring(1,8); //second param is OFFSET
    //Serial.print(timestampString);
    //Serial.println("<=known to frontend");
    //timestampString = "0123227";
    // Convert cs[ENCRYPTION_SCHEME] into an array of nibbles
    uint8_t nibbles[16];  // 16 hex characters ? 16 nibbles
    
    for (int i = 0; i < 16; i++) {
        char c = cs[ENCRYPTION_SCHEME][i];
        if (c >= '0' && c <= '9')
            nibbles[i] = c - '0';
        else if (c >= 'A' && c <= 'F')
            nibbles[i] = 10 + (c - 'A');
        else if (c >= 'a' && c <= 'f')
            nibbles[i] = 10 + (c - 'a');
        else
            nibbles[i] = 0; // or handle invalid char
    }
    // Process nibbles
    String processedString = "";
    int counter = 0;
    uint8_t thisByteResult;
    uint8_t thisByteOfStoragePassword;
    uint8_t thisByteOfDataString;
    uint8_t thisByteOfTimestampString;
    uint8_t dataStringChecksum = calculateChecksum(datastring);
    uint8_t timestampStringChecksum = calculateChecksum(timestampString);
    uint8_t dataStringSetBits = countSetBitsInString(datastring);
    uint8_t timestampStringSetBits = countSetBitsInString(timestampString);
    uint8_t dataStringZeroCount = countZeroes(datastring);
    uint8_t timestampStringZeroCount = countZeroes(timestampString);
    for (int i = 0; i < strlen(cs[STORAGE_PASSWORD]) * 2; i++) {
        thisByteOfDataString = datastring[counter % datastring.length()];
        thisByteOfTimestampString = timestampString[counter % timestampString.length()];
        /*
        Serial.print(timestampString);
        Serial.print(":");
        Serial.print(timestampString[counter % timestampString.length()]);
        Serial.print(":");
        Serial.print(counter);
        Serial.print(":");
        Serial.println(counter % timestampString.length());
        */
        
        if(i % 2 == 0) {
          thisByteOfStoragePassword = cs[STORAGE_PASSWORD][counter];
        } else {
          thisByteOfStoragePassword = thisByteResult;
        }
        uint8_t nibble = nibbles[i % 16];
        if(nibble == 0) {
          //do nothing
        } else if(nibble == 1) { 
          thisByteResult = thisByteOfStoragePassword ^ dataStringZeroCount;
        } else if(nibble == 2) { 
          thisByteResult = thisByteOfStoragePassword ^ thisByteOfTimestampString;
        } else if(nibble == 3) { 
          thisByteResult = thisByteOfStoragePassword ^ thisByteOfDataString;
        } else if(nibble == 4) { 
          thisByteResult = thisByteOfStoragePassword ^ rotateLeft(thisByteOfDataString, 1);
        } else if(nibble == 5) { 
          thisByteResult = thisByteOfStoragePassword ^ rotateRight(thisByteOfDataString, 1);
        } else if(nibble == 6) { 
          thisByteResult = thisByteOfStoragePassword ^ dataStringChecksum;
        } else if(nibble == 7) { 
          thisByteResult = thisByteOfStoragePassword ^ timestampStringChecksum;
        } else if(nibble == 8) { 
          thisByteResult = thisByteOfStoragePassword ^ dataStringSetBits;
        } else if(nibble == 9) { 
          thisByteResult = thisByteOfStoragePassword ^ timestampStringSetBits;
        } else if(nibble == 10) { 
          thisByteResult = rotateLeft(thisByteOfStoragePassword, 1);
        } else if(nibble == 11) { 
          thisByteResult = rotateRight(thisByteOfStoragePassword, 1);
        } else if(nibble == 12) { 
          thisByteResult = rotateLeft(thisByteOfStoragePassword, 2);
        } else if(nibble == 13) { 
          thisByteResult = rotateRight(thisByteOfStoragePassword, 2);
        } else if(nibble == 14) { 
          thisByteResult = ~thisByteOfStoragePassword; //invert the byte
        } else if(nibble == 15) { 
          thisByteResult = thisByteOfStoragePassword ^ timestampStringZeroCount;
        } else {
          thisByteResult = thisByteOfStoragePassword;
        }
        
        // Advance the counter every other nibble
        
        if (i % 2 == 1) {
            char hexStr[3];
            sprintf(hexStr, "%02X", thisByteResult); 
            String paddedHexStr = String(hexStr); 
            processedString += paddedHexStr;  // Append nibble as hex character
             
            /*
            Serial.print("%");
            Serial.print(thisByteOfStoragePassword);
            Serial.print(",");
            Serial.print(thisByteOfDataString);
            Serial.print("|");
            Serial.print(thisByteOfTimestampString);
            Serial.print("|");
            Serial.print(dataStringZeroCount);
            Serial.print("|");
            Serial.print(timestampStringZeroCount);
            Serial.print(" via: ");
            Serial.print(nibble);
            Serial.print("=>");
            Serial.println(thisByteResult);
            */
            counter++;
        } else {
            /*
            Serial.print("&");
            Serial.print(thisByteOfStoragePassword);
            Serial.print(",");
            Serial.print(thisByteOfDataString);
            Serial.print("|");
            Serial.print(thisByteOfTimestampString);
            Serial.print("|");
            Serial.print(dataStringZeroCount);
            Serial.print("|");
            Serial.print(timestampStringZeroCount);
            Serial.print(" via: ");
            Serial.print(nibble);
            Serial.print("=>");
            Serial.println(thisByteResult);
            */
            
        }
        //Serial.print(":");
    }
    return processedString;
}

//slave functions
long requestLong(byte slaveAddress, byte command) {
  Wire.beginTransmission(slaveAddress);
  Wire.write(command);    // send the command
  Wire.endTransmission();
  yield();
  delay(1);
  Wire.requestFrom(slaveAddress, 4);
  long value = 0;
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


//time functions 
 
 
// Convert a SQL-style datetime string ("YYYY-MM-DD HH:MM:SS") to a Unix timestamp
time_t parseDateTime(String dateTime) {
    int year, month, day, hour, minute, second;
    
    // Convert String to C-style char* for parsing
    if (sscanf(dateTime.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
        return 0; // Return 0 if parsing fails
    }

    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;
    
    return mktime(&t); // Convert struct tm to Unix timestamp
}


String msTimeAgo(uint32_t base, uint32_t millisFromPast) {
  return humanReadableTimespan((uint32_t) (base - millisFromPast)/1000);
}

String msTimeAgo(uint32_t millisFromPast) {
  return humanReadableTimespan((uint32_t) (millis() - millisFromPast)/1000);
}
 
// Overloaded version: Uses NTP time as the default comparison
String timeAgo(String sqlDateTime) {
    return timeAgo(sqlDateTime, timeClient.getEpochTime());
}

// Returns a human-readable "time ago" string
String timeAgo(String sqlDateTime, time_t compareTo) {
    time_t past;
    time_t nowTime;

    if (sqlDateTime.length() == 0) {
        // If an empty string is passed, use millis() for uptime
        uint32_t uptimeSeconds = millis() / 1000;
        nowTime = uptimeSeconds;
        past = 0;
    } else {
        past = parseDateTime(sqlDateTime);
        nowTime = compareTo;
    }

    if (past == -1 || past > nowTime) {
        return "Invalid time";
    }

    time_t diffInSeconds = nowTime - past;
    return humanReadableTimespan((uint32_t) diffInSeconds);
}

String humanReadableTimespan(uint32_t diffInSeconds) {
    int seconds = diffInSeconds % 60;
    int minutes = (diffInSeconds / 60) % 60;
    int hours = (diffInSeconds / 3600) % 24;
    int days = diffInSeconds / 86400;

    if (days > 0) {
        return String(days) + (days == 1 ? " day ago" : " days ago");
    }
    if (hours > 0) {
        return String(hours) + (hours == 1 ? " hour ago" : " hours ago");
    }
    if (minutes > 0) {
        return String(minutes) + (minutes == 1 ? " minute ago" : " minutes ago");
    }
    return String(seconds) + (seconds == 1 ? " second ago" : " seconds ago");
}

