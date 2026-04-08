 #include "commandhandlers.h"


void cmdDel(String* param, int argCount) {
  deleteFile(param[0].c_str());
}

void cmdDownload(String* param, int argCount) {
  downloadFile(param[0].c_str(), extractFilename(param[0]).c_str());
}

void cmdUpload(String* param, int argCount) {
  if(fileToUpload != "") {
    textOut(fileToUpload + " is still uploading; please wait\n");
    return;
  }
  fileToUpload = param[0];
  File f = LittleFS.open(fileToUpload, "r");
  if (!f) {
      textOut(fileToUpload + ": file not found\n");
      fileToUpload = "";
      return;
  }
  f.close();
  textOut(fileToUpload + " uploading to server...\n");
  fileUploadPosition = 0;
}

void cmdCat(String* param, int argCount) {
  param[0].trim();
  dumpFile(param[0].c_str());
}

void cmdReadSlaveEeprom(String* param, int argCount) {
  char buffer[500];
  readBytesFromSlaveEEPROM((uint16_t)param[0].toInt(), buffer, 500);
  textOut("EEPROM data:\n");
  textOut(String(buffer));
}

void cmdResetSerial(String* param, int argCount) {
  setSerialRate((byte)ci[BAUD_RATE_LEVEL]); 
  ETS_UART_INTR_DISABLE();
  ETS_UART_INTR_ENABLE();
  textOut("Serial reset\n");
}

void cmdConfigEeprom(String* param, int argCount) {
  loadAllConfigFromEEPROM(1, 0);
}

void cmdDumpSlaveEeprom(String* param, int argCount) {
  loadAllConfigFromEEPROM(1, 512);
}

void cmdSendSlaveSerial(String* param, int argCount) {
  sendSlaveSerial(param[0].c_str());
  textOut("Serial data sent to slave: " + param[0] + "\n");
}

void cmdSetSlaveTime(String* param, int argCount) {
  sendLong(ci[SLAVE_I2C], 180, param[0].toInt());
  textOut("Slave UNIX time set to: " + param[0] + "\n");
}

void cmdGetSlaveTime(String* param, int argCount) {
  uint32_t unixTime = requestLong(ci[SLAVE_I2C], 181);
  textOut("Slave UNIX time: " + String(unixTime) + "\n");
}

void cmdInitSlaveSerial(String* param, int argCount) {
  enableSlaveSerial(9);
  textOut("Serial on slave initiated\n");
}

void getSlaveSerial(String* param, int argCount) {
  char buffer[500]; 
  int count = readBytesFromSlaveSerial(buffer, 500);
  String result = String(buffer).substring(0, count);
  textOut(result);
}

void getSlaveParsedDatum(String* param, int argCount) {
  param[0].trim();
  uint8_t ordinal = param[0].toInt();
  uint16_t result = getParsedSlaveDatum(ordinal);
  textOut("Parsed slave value " + param[0] + ": " + result + "\n");
}

void updateSlaveFirmware(String* param, int argCount) {
  millisAtPossibleReboot = millis();
  param[0].trim(); //this should contain a url for new firmware.  if it begins with "/" assume it is on the same host as everything else
  String flashUrl = "";
  textOut("Updating slave firmware...\n");
  if(param[0].startsWith("http://")) { //if we get a full url
    flashUrl = param[0];
  } else if(param[0].charAt(0) == '/') {
    flashUrl = "http://" + String(cs[HOST_GET]) + param[0]; //my firmware has an aversion to https!
  } else { //get the flash file from the backend using its security system, pulling it from the flash update directory, wherever it happens to be
    String encryptedStoragePassword = encryptStoragePassword(param[0]);
    flashUrl = "http://" + String(cs[HOST_GET]) + String(cs[URL_GET]) + "?k2=" + encryptedStoragePassword + "&architecture=" + architecture + "&device_id=" + ci[DEVICE_ID] + "&mode=reflash&data=" + urlEncode(param[0], true);  
  }
  //Serial.println(flashUrl);
  String possibleResult;
  HTTPClient http;
  if(urlExists(flashUrl.c_str())){
    http.begin(clientGet, flashUrl.c_str());
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        textOut("Flash file failed to load");
    }
    uint32_t oldVersion = requestLong(ci[SLAVE_I2C], 160);
    enterSlaveBootloader();
    updateSlaveFirmware((String)flashUrl); //synchronous!  
    //startSlaveUpdate((String)flashUrl);  //for a failed state machine
    uint32_t newVersion = 0;
    int waitIterations = 0;
    while(newVersion == 0  && waitIterations < 20) {
      newVersion = requestLong(ci[SLAVE_I2C], 160);
      delay(200);
      waitIterations++;
    }
    textOut("Slave firmware updated from version " + String(oldVersion) + " to " + String(newVersion) + "\n");
  } else {
    possibleResult = flashUrl + " does not exist; no action taken\n";
    textOut(possibleResult);
    possibleEndingMessage = possibleResult;
  }
}

void getMasterEepromUsed(String* param, int argCount) {
  int bytesUsed = loadAllConfigFromEEPROM(2, 0);
  textOut("Slave EEPROM bytes used for master: " + (String)bytesUsed + "\n");
}

void getSlaveEepromUsed(String* param, int argCount) {
  int bytesUsed = loadAllConfigFromEEPROM(2, 512);
  textOut("Slave EEPROM bytes used for slave: " + (String)(bytesUsed - 512) + "\n");
}

void getSlave(String* param, int argCount) {
  param[0].trim(); 
  uint16_t result = getSlaveConfigItem((byte)param[0].toInt()); 
}

void setSlaveParserBasis(String* param, int argCount) {
  String ordinalString = param[0];
  int ordinal = ordinalString.toInt();
  String value = param[1];
  css[ordinal] = strdup(value.c_str());
  textOut("Slave parser basis #" + ordinalString + " set to " + value + "\n");
}

void setSlaveBasis(String* param, int argCount) {
  String ordinalString = param[0];
  String value = param[1];
  int ordinal = ordinalString.toInt();
  cis[ordinal] = value.toInt();
  textOut("Slave parser basis #" + ordinalString + " set to " + value + "\n");
}

void setSlave(String* param, int argCount) {
  String ordinalString = param[0];
  String value = param[1];
  int ordinal = ordinalString.toInt();
 
  if(isInteger(ordinalString)) {
    setSlaveConfigItem(ordinal, value.toInt());
    textOut("Slave configuration " + ordinalString + " set to: " + value + "\n");
  }
}


void runSlave(String* param, int argCount) {
  String ordinalString = param[0];
  int ordinal = ordinalString.toInt();
  String value = param[1];
 if(value != "") {
    sendLong(ci[SLAVE_I2C], ordinal, value.toInt());
    textOut(
          "Commmand " +
          ordinalString + " run on slave with value: " +
          value + "\n"
        );
  } else {
    long result = requestLong(ci[SLAVE_I2C], ordinal);
    textOut("Slave data for command " + ordinalString + ": " + String(result) + "\n");
  }
}

void cmdSet(String* param, int argCount) {
  String key = param[0];
  String value = param[1];
 if(!isInteger(key)) {
  textOut("Configuration '" + key + "' does not exist:\n");
  return;
 }
 int configIndex = key.toInt();
 if(value != "") {
    value.trim();
    if(configIndex>=CONFIG_STRING_COUNT) {
      ci[configIndex] = value.toInt();
    } else {   
      char buffer[50];    
      value.toCharArray(buffer, sizeof(buffer));
      cs[configIndex] = strdup(buffer); 
    }
  }
  textOut("Configuration set to: " + value + "\n");
}  


void cmdGet(String* param, int argCount) {
  String key = param[0];
  if(!isInteger(key)) {
    textOut("Configuration '" + key + "' does not exist:\n");
    return;
  }
  int configIndex = key.toInt();
  String value;
  if(configIndex>=CONFIG_STRING_COUNT) {
    value = String(ci[configIndex]);
  } else {
    value = String(cs[configIndex]);
  }
  textOut("Sought configuration: " + value + "\n");
 
}
















