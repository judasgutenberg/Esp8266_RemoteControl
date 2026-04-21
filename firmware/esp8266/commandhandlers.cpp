 #include "commandhandlers.h"
 #include "globals.h"


void cmdDeferredReboot(String* param, int argCount, bool deferred) {
  if(!deferred) {
    textOut("Rebooting... \n");
    //notYetDeferred();
  } else {
    rebootEsp();
  }
}

void cmdUpdateFirmware(String* param, int argCount, bool deferred) {
  if(!deferred) {
    textOut("Attempting firmware update...\n");
    //notYetDeferred();
  } else {
    millisAtPossibleReboot = millis();
    String rest = param[0];
    rest.trim(); //this should contain a url for new firmware.  if it begins with "/" assume it is on the same host as everything else
    String flashUrl = "";
    if(rest.startsWith("http://")) { //if we get a full url
      flashUrl = rest;
    } else if(rest.charAt(0) == '/') {
      flashUrl = "http://" + String(cs[HOST_GET]) + rest; //my firmware has an aversion to https!
    } else { //get the flash file from the backend using its security system, pulling it from the flash update directory, wherever it happens to be
      String encryptedStoragePassword = encryptStoragePassword(rest);
      flashUrl = "http://" + String(cs[HOST_GET]) + String(cs[URL_GET]) + "?k2=" + encryptedStoragePassword + "&architecture=" + architecture + "&device_id=" + ci[DEVICE_ID] + "&mode=reflash&data=" + urlEncode(rest, true);  
    }
    String possibleResult;
    //setSlaveLong(1, VERSION);
    //saveCommandState(lastCommandLogId, VERSION);
    if(urlExists(flashUrl.c_str())){
       t_httpUpdate_return ret = ESPhttpUpdate.update(clientGet, flashUrl.c_str());
       switch (ret) {
        case HTTP_UPDATE_FAILED:
          possibleResult = "Update failed; error (" + String(ESPhttpUpdate.getLastError()) + ") " + ESPhttpUpdate.getLastErrorString() + "\n";
          textOut(possibleResult);
          possibleEndingMessage = possibleResult;
          break;
      
        case HTTP_UPDATE_NO_UPDATES:
          possibleResult = "No update available\n";
          textOut(possibleResult);
          possibleEndingMessage = possibleResult;
          break;
      
        case HTTP_UPDATE_OK:
          possibleResult = "Update successful; rebooting...\n";
          textOut(possibleResult);
          possibleEndingMessage = possibleResult;
          break;
      }
    } else {
      possibleResult = flashUrl + " does not exist; no action taken\n";
      textOut(possibleEndingMessage);
      possibleEndingMessage = possibleResult;
    }
  }
}


////////////////////
void cmdVersion(String* param, int argCount, bool deferred) {
  textOut("Version: " + String(VERSION) + String("\n"));
}

void cmdRunSlaveSketch(String* param, int argCount, bool deferred) {
  runSlaveSketch();
  textOut("Hopefully running a sketch\n");
}

void cmdRunSlaveBootloader(String* param, int argCount, bool deferred) {
  enterSlaveBootloader();
  textOut("Slave is waiting for a sketch\n");
}

void cmdPetWatchdog(String* param, int argCount, bool deferred) {
  uint32_t unixTime = timeClient.getEpochTime();
  petWatchDog((uint8_t)ci[SLAVE_PET_WATCHDOG_COMMAND], unixTime);
  textOut("Watchdog petted\n");
}


void cmdGetWeatherSensors(String* param, int argCount, bool deferred) {
  String transmissionString = weatherDataString(ci[SENSOR_ID], ci[SENSOR_SUB_TYPE], ci[SENSOR_DATA_PIN], ci[SENSOR_POWER_PIN], ci[SENSOR_I2C], NULL, 0, deviceName, -1, ci[CONSOLIDATE_ALL_SENSORS_TO_ONE_RECORD]);
  textOut(transmissionString + "\n");
}

void cmdRebootEsp(String* param, int argCount, bool deferred) {
  rebootEsp();
}

void cmdOnePinAtATime(String* param, int argCount, bool deferred) {
  onePinAtATimeMode = (boolean)param[0].toInt(); //setting a global.
  textOut("One pin at a time mode now: " + param[0] + "\n");
}

void cmdClearLatencyAverage(String* param, int argCount, bool deferred) {
  latencyCount = 0;
  latencySum = 0;
  textOut("Latency average cleared\n");
}

void cmdIr(String* param, int argCount, bool deferred) {
  String commandData = param[0];
  commandData.replace(" ", ",");
  sendIr(commandData); //ir data must be comma-delimited
}

void cmdClearFram(String* param, int argCount, bool deferred) {
  if(ci[FRAM_ADDRESS] > 0) {
    clearFramLog(); 
    textOut("FRAM log cleared\n");
  }
}

void cmdDumpFram(String* param, int argCount, bool deferred) {
  if(ci[FRAM_ADDRESS] > 0) {
    displayAllFramRecords(); 
  }
}

void cmdDumpFramHex(String* param, int argCount, bool deferred) {
  String commandData = param[0];
  if(ci[FRAM_ADDRESS] > 0) {
    if(commandData == "") {
      hexDumpFRAM(2 * ci[FRAM_INDEX_SIZE], lastRecordSize, 15);
    } else {
      hexDumpFRAM(param[0].toInt(), lastRecordSize, 15);
    }
  }
}

void cmdDumpFramHexAt(String* param, int argCount, bool deferred) {
  if(ci[FRAM_ADDRESS] > 0) {
    hexDumpFRAMAtIndex(param[0].toInt(), lastRecordSize, 15); 
  }
}

void cmdSwapFram(String* param, int argCount, bool deferred) {
  if(ci[FRAM_ADDRESS] > 0) {
    swapFRAMContents(ci[FRAM_INDEX_SIZE] * 2, 554, lastRecordSize);
  }
}

void cmdDumpFramRecord(String* param, int argCount, bool deferred) {
  if(ci[FRAM_ADDRESS] > 0) {
    displayFramRecord((uint16_t)param[0].toInt()); 
  }
}

void cmdGetFramIndex(String* param, int argCount, bool deferred) {
  if(ci[FRAM_ADDRESS] > 0) {
    dumpFramRecordIndexes();
  }
}

void cmdRebootSlave(String* param, int argCount, bool deferred) {
  if(ci[SLAVE_I2C] > 0) {
    requestLong(ci[SLAVE_I2C], 128);
    textOut("Slave rebooted\n");
  }
}

void cmdSetDate(String* param, int argCount, bool deferred) { //params must be comma-delimited
  if(ci[RTC_ADDRESS] > 0) {
    String dateArray[7];
    splitString(param[0], ',', dateArray, 7);
    setDateDs1307((byte) dateArray[0].toInt(), 
                 (byte) dateArray[1].toInt(),     
                 (byte) dateArray[2].toInt(),  
                 (byte) dateArray[3].toInt(),  
                 (byte) dateArray[4].toInt(),   
                 (byte) dateArray[5].toInt(),      
                 (byte) dateArray[6].toInt()); 
    textOut("Date set\n");
  }
}

void cmdGetDate(String* param, int argCount, bool deferred) {
  if(ci[RTC_ADDRESS] > 0) {
    printRTCDate();
  }
}

void cmdGetWatchdogInfo(String* param, int argCount, bool deferred) {
  if(ci[SLAVE_I2C] > 0) {
    slaveWatchdogInfo();
  }
}

void cmdGetWatchdogData(String* param, int argCount, bool deferred) {
  if(ci[SLAVE_I2C] > 0) {
    textOut(slaveWatchdogData() + "\n");
  }
}

void cmdListFiles(String* param, int argCount, bool deferred) {
  listFiles();
}

void cmdSaveMasterConfig(String* param, int argCount, bool deferred) {
  if(ci[SLAVE_I2C] > 0 && ci[CONFIG_PERSIST_METHOD] == CONFIG_PERSIST_METHOD_I2C_SLAVE) {
    saveAllConfigToEEPROM(0);
    textOut("Configuration saved to EEPROM\n");
  } else if (ci[CONFIG_PERSIST_METHOD] == CONFIG_PERSIST_METHOD_FLASH) {
    saveAllConfigToFlash(0);
    textOut("Configuration saved to flash\n");
  }
}

void cmdSaveSlaveConfig(String* param, int argCount, bool deferred) {
  if(ci[SLAVE_I2C] > 0) {
    saveAllConfigToEEPROM(512);
    textOut("Configuration saved\n");
  }
}

void cmdInitMasterDefaults(String* param, int argCount, bool deferred) {
  if(ci[SLAVE_I2C] > 0) {
    initMasterDefaults();
    textOut("Master config initialized\n");
  }
}

void cmdInitSlaveDefaults(String* param, int argCount, bool deferred) {
  if(ci[SLAVE_I2C] > 0) {
    initSlaveDefaults();
    textOut("Slave config initialized\n");
  }
}

void cmdGetUptime(String* param, int argCount, bool deferred) {
  textOut("Last booted: " + timeAgo("") + "\n");
}

void cmdGetWifiUptime(String* param, int argCount, bool deferred) {
  textOut("WiFi up: " + msTimeAgo(wifiOnTime) + "\n");
}

void cmdGetLastpoll(String* param, int argCount, bool deferred) {
  textOut("Last poll: " + msTimeAgo(lastPoll) + "\n");
}

void cmdGetLastdatalog(String* param, int argCount, bool deferred) {
  textOut("Last data: " + msTimeAgo(lastDataLogTime) + "\n");
}

void cmdMemory(String* param, int argCount, bool deferred) {
  dumpMemoryStats(0);
}

void cmdDumpSerialPacket(String* param, int argCount, bool deferred) {
  char buffer[128]; 
  readDataParsedFromSlaveSerial();
  //parsedSerialData
  //uint8_t parsedBuf[40]  = {0xF1, 0xF2, 0xD1, 0xD2, 0xC1, 0xC2, 0xB1, 0xB2};
  bytesToHex(parsedSerialData, 20, buffer);
  textOut("Parsed serial packet: " + String(buffer) + "\n");
}


void cmdFormatFileSystem(String* param, int argCount, bool deferred) {
  formatFileSystem();
  textOut("File system formatted\n");
}
///////////////////////



void cmdDel(String* param, int argCount, bool deferred) {
  deleteFile(param[0].c_str());
}

void cmdDownload(String* param, int argCount, bool deferred) {
  downloadFile(param[0].c_str(), extractFilename(param[0]).c_str());
}

void cmdUpload(String* param, int argCount, bool deferred) {
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

void cmdCat(String* param, int argCount, bool deferred) {
  param[0].trim();
  dumpFile(param[0].c_str());
}

void cmdReadSlaveEeprom(String* param, int argCount, bool deferred) {
  char buffer[500];
  readBytesFromSlaveEEPROM((uint16_t)param[0].toInt(), buffer, 500);
  textOut("EEPROM data:\n");
  textOut(String(buffer));
}

void cmdResetSerial(String* param, int argCount, bool deferred) {
  setSerialRate((byte)ci[BAUD_RATE_LEVEL]); 
  ETS_UART_INTR_DISABLE();
  ETS_UART_INTR_ENABLE();
  textOut("Serial reset\n");
}

void cmdConfigEeprom(String* param, int argCount, bool deferred) {
  loadAllConfigFromEEPROM(1, 0);
}

void cmdDumpSlaveEeprom(String* param, int argCount, bool deferred) {
  loadAllConfigFromEEPROM(1, 512);
}

void cmdSendSlaveSerial(String* param, int argCount, bool deferred) {
  sendSlaveSerial(param[0].c_str());
  textOut("Serial data sent to slave: " + param[0] + "\n");
}

void cmdSetSlaveTime(String* param, int argCount, bool deferred) {
  sendLong(ci[SLAVE_I2C], 180, param[0].toInt());
  textOut("Slave UNIX time set to: " + param[0] + "\n");
}

void cmdGetSlaveTime(String* param, int argCount, bool deferred) {
  uint32_t unixTime = requestLong(ci[SLAVE_I2C], 181);
  textOut("Slave UNIX time: " + String(unixTime) + "\n");
}

void cmdInitSlaveSerial(String* param, int argCount, bool deferred) {
  enableSlaveSerial(9);
  textOut("Serial on slave initiated\n");
}

void getSlaveSerial(String* param, int argCount, bool deferred) {
  char buffer[500]; 
  int count = readBytesFromSlaveSerial(buffer, 500);
  String result = String(buffer).substring(0, count);
  textOut(result);
}

void getSlaveParsedDatum(String* param, int argCount, bool deferred) {
  param[0].trim();
  uint8_t ordinal = param[0].toInt();
  uint16_t result = getParsedSlaveDatum(ordinal);
  textOut("Parsed slave value " + param[0] + ": " + result + "\n");
}

void updateSlaveFirmware(String* param, int argCount, bool deferred) {
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

void getMasterEepromUsed(String* param, int argCount, bool deferred) {
  int bytesUsed = loadAllConfigFromEEPROM(2, 0);
  textOut("Slave EEPROM bytes used for master: " + (String)bytesUsed + "\n");
}

void getSlaveEepromUsed(String* param, int argCount, bool deferred) {
  int bytesUsed = loadAllConfigFromEEPROM(2, 512);
  textOut("Slave EEPROM bytes used for slave: " + (String)(bytesUsed - 512) + "\n");
}

void getSlave(String* param, int argCount, bool deferred) {
  param[0].trim(); 
  uint16_t result = getSlaveConfigItem((byte)param[0].toInt()); 
}

void setSlaveParserBasis(String* param, int argCount, bool deferred) {
  String ordinalString = param[0];
  int ordinal = ordinalString.toInt();
  String value = param[1];
  css[ordinal] = strdup(value.c_str());
  textOut("Slave parser basis #" + ordinalString + " set to " + value + "\n");
}

//hmmmmmmm



void setSlaveBasis(String* param, int argCount, bool deferred) {
  String ordinalString = param[0];
  String value = param[1];
  int ordinal = ordinalString.toInt();
  cis[ordinal] = value.toInt();
  textOut("Slave parser basis #" + ordinalString + " set to " + value + "\n");
}

void setSlave(String* param, int argCount, bool deferred) {
  String ordinalString = param[0];
  String value = param[1];
  int ordinal = ordinalString.toInt();
 
  if(isInteger(ordinalString)) {
    setSlaveConfigItem(ordinal, value.toInt());
    textOut("Slave configuration " + ordinalString + " set to: " + value + "\n");
  }
}


void runSlave(String* param, int argCount, bool deferred) {
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

void cmdSet(String* param, int argCount, bool deferred) {
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


void cmdGet(String* param, int argCount, bool deferred) {
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
