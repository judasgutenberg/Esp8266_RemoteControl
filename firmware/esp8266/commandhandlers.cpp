 #include "commandhandlers.h"


void cmdDeferredReboot(String* param, int argCount, bool deferred) {
  if(!deferred) {
    textOut(F("Rebooting... \n"));
    //notYetDeferred();
  } else {
    rebootEsp();
  }
}


void cmdRebootMasterFromSlave(String* param, int argCount, bool deferred) {
  if(!deferred) {
    textOut(F("Slave instigating master reboot... \n"));
    //notYetDeferred();
  } else {
    requestLong(ci[SLAVE_I2C], 134);
  }
}


void cmdLocalUpdateFirmware(String* param, int argCount, bool deferred) {
  if(!deferred) {
    textOut(F("Attempting firmware update using file: ") + String(param[0]) + F("...\n"));
    //notYetDeferred();
  } else {
    flashFromLittleFS(param[0].c_str());
  }
}

void cmdUpdateFirmware(String* param, int argCount, bool deferred) {
  if(!deferred) {
    textOut(F("Attempting firmware update...\n"));
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
          possibleResult = F("Update failed; error (") + String(ESPhttpUpdate.getLastError()) + ") " + ESPhttpUpdate.getLastErrorString() + "\n";
          //textOut(possibleResult);
          possibleEndingMessage = possibleResult;
          break;
      
        case HTTP_UPDATE_NO_UPDATES:
          possibleResult = F("No update available\n");
          //textOut(possibleResult);
          possibleEndingMessage = possibleResult;
          break;
      
        case HTTP_UPDATE_OK:
          possibleResult = F("Update successful; rebooting...\n");
          //textOut(possibleResult);
          possibleEndingMessage = possibleResult;
          break;
      }
    } else {
      possibleResult = flashUrl + F(" does not exist; no action taken\n");
      //textOut(possibleEndingMessage);
      possibleEndingMessage = possibleResult;
    }
  }
}

/////////////////////

void cmdBeforeBoot(String* param, int argCount, bool deferred) {
  textOut(F("Apparent bad reboot count: "));
  textOut(String(rtc.rebootCount));
  textOut(F(", millis up: "));
  textOut(String(rtc.lastMillis));

  textOut(F(", version: "));
  textOut(String(rtc.lastVersion));
  textOut(F(", last commandLogId: "));
  textOut(String(rtc.lastCommandLogId));
  textOut(F(", last commandId: "));
  textOut(String(rtc.lastCommandId));
  textOut(F(", last commandType: "));
  textOut(String(rtc.lastCommandType));
  textOut(F(", useHardcodedConfig: "));
  textOut(String(rtc.useHardcodedConfig));
  textOut("\n");
}

void cmdQuitSafeMode(String* param, int argCount, bool deferred) {
  rtcMarkStable();
  textOut(F("Startup safe mode disabled\n"));
}

void cmdSetPreboot(String* param, int argCount, bool deferred) {
  int loc = param[0].toInt();
  uint32_t value = param[1].toInt();
  if(loc == 1) {
    rtc.lastVersion = value;
  } else if (loc == 2)  {
    rtc.lastCommandId = value;
  } else if (loc == 3)  {
    rtc.lastCommandType = value;
  } else if (loc == 4)  {
    rtc.useHardcodedConfig = value;
  } else {
    rtc.lastCommandLogId = value;
  }
  rtcWrite(rtc);
  textOut(F("Preboot set\n"));
}
////////////////////


void cmdfastCom(String* param, int argCount, bool deferred) {
  int value = param[0].toInt();
  String blurb = F("Fast communications changing to ");
  if(!deferred) {
    if(value == 1) {
      textOut(blurb + "on\n"); 
    } else {
      textOut(blurb + "off\n"); 
    }   
  } else {
    if(value == 1) {
      startWebSocket(3);
      //prevents nasty loops:
      oldOutputMode = 0;
      outputMode = 3;
    } else {
      stopWebSocket();
      //prevents nasty loops:
      oldOutputMode = 0;
      outputMode = 0;
    }   
  }
}

void cmdInitSensors(String* param, int argCount, bool deferred) {
  startWeatherSensors(ci[SENSOR_ID],  ci[SENSOR_SUB_TYPE], ci[SENSOR_I2C], ci[SENSOR_DATA_PIN], ci[SENSOR_POWER_PIN]);
  textOut(F("Sensors re-initialized\n"));
}

void cmdVersion(String* param, int argCount, bool deferred) {
  textOut(F("Version: ") + String(VERSION) + String("\n"));
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
  textOut(F("One pin at a time mode now: ") + param[0] + "\n");
}

void cmdClearLatencyAverage(String* param, int argCount, bool deferred) {
  latencyCount = 0;
  latencySum = 0;
  textOut(F("Latency average cleared\n"));
}

void cmdIr(String* param, int argCount, bool deferred) {
  String commandData = param[0];
  commandData.replace(" ", ",");
  sendIr(commandData); //ir data must be comma-delimited
}

void cmdClearFram(String* param, int argCount, bool deferred) {
  clearFramLog(); 
  textOut(F("FRAM log cleared\n"));
}

void cmdDumpFram(String* param, int argCount, bool deferred) {
  displayAllFramRecords(); 
}

void cmdDumpFramHex(String* param, int argCount, bool deferred) {
  String commandData = param[0];
  if(commandData == "") {
    hexDumpFRAM(2 * ci[FRAM_INDEX_SIZE], lastRecordSize, 15);
  } else {
    hexDumpFRAM(param[0].toInt(), lastRecordSize, 15);
  }
}

void cmdDumpFramHexAt(String* param, int argCount, bool deferred) {
  hexDumpFRAMAtIndex(param[0].toInt(), lastRecordSize, 15); 
}

void cmdSwapFram(String* param, int argCount, bool deferred) {
  swapFRAMContents(ci[FRAM_INDEX_SIZE] * 2, 554, lastRecordSize);
}

void cmdDumpFramRecord(String* param, int argCount, bool deferred) {
  displayFramRecord((uint16_t)param[0].toInt()); 
}

void cmdGetFramIndex(String* param, int argCount, bool deferred) {
  dumpFramRecordIndexes();
}

void cmdRebootSlave(String* param, int argCount, bool deferred) {
  requestLong(ci[SLAVE_I2C], 128);
  textOut("Slave rebooted\n");
}

void cmdSetDate(String* param, int argCount, bool deferred) { //params must be comma-delimited
  String dateArray[7];
  splitString(param[0], ',', dateArray, 7);
  setDateDs1307((byte) dateArray[0].toInt(), 
               (byte) dateArray[1].toInt(),     
               (byte) dateArray[2].toInt(),  
               (byte) dateArray[3].toInt(),  
               (byte) dateArray[4].toInt(),   
               (byte) dateArray[5].toInt(),      
               (byte) dateArray[6].toInt()); 
  textOut(F("Date set\n"));
}

void cmdGetDate(String* param, int argCount, bool deferred) {
  printRTCDate();
}

void cmdGetWatchdogInfo(String* param, int argCount, bool deferred) {
  slaveWatchdogInfo();
}

void cmdGetWatchdogData(String* param, int argCount, bool deferred) {
  textOut(slaveWatchdogData() + "\n");
}

void cmdListFiles(String* param, int argCount, bool deferred) {
  listFiles();
}

void cmdSaveMasterConfig(String* param, int argCount, bool deferred) {
  String toWord = param[0];
  String dest = param[1];
  String initialBlurb = F("Configuration saved to");
  /*
  Serial.print(toWord);
  Serial.print("*");
  Serial.println(dest);
  Serial.println("*");
  */
  if(ci[SLAVE_I2C] > 0 && ((ci[CONFIG_PERSIST_METHOD] == CONFIG_PERSIST_METHOD_I2C_SLAVE && dest == "") || dest == "slave")) {
    saveAllConfigToEEPROM(0);
    textOut(initialBlurb + F(" slave EEPROM\n"));
  /*
  } else if ((ci[CONFIG_PERSIST_METHOD] == CONFIG_PERSIST_METHOD_FLASH && dest == "") || dest == "flash") {
    saveAllConfigToFlash(0);
    textOut(initialBlurb + F(" local flash\n"));
  */
  } else if (ci[FRAM_ADDRESS] > 0  && ((ci[CONFIG_PERSIST_METHOD] == CONFIG_PERSIST_METHOD_FRAM && dest == "") || dest == "fram")) {
    saveAllConfigToFRAM(0);
    textOut(initialBlurb + F(" fram\n"));
  } else {
    saveAllConfigToFlash(0);
    textOut(initialBlurb + F(" local flash\n"));
  }
}

void cmdSaveSlaveConfig(String* param, int argCount, bool deferred) {
  saveAllConfigToEEPROM(512);
  textOut("Configuration saved\n");
}

void cmdInitMasterDefaults(String* param, int argCount, bool deferred) {
  initMasterDefaults();
  textOut("Master config initialized\n");
}

void cmdInitSlaveDefaults(String* param, int argCount, bool deferred) {
  initSlaveDefaults();
  textOut(F("Slave config initialized\n"));
}

void cmdGetUptime(String* param, int argCount, bool deferred) {
  textOut(F("Last booted: ") + timeAgo("") + "\n");
}

void cmdGetWifiUptime(String* param, int argCount, bool deferred) {
  textOut(F("WiFi up: ") + msTimeAgo(wifiOnTime) + "\n");
}

void cmdGetLastpoll(String* param, int argCount, bool deferred) {
  textOut(F("Last poll: ") + msTimeAgo(lastPoll) + "\n");
}

void cmdGetLastdatalog(String* param, int argCount, bool deferred) {
  textOut(F("Last data: ") + msTimeAgo(lastDataLogTime) + "\n");
}

void cmdTiming(String* param, int argCount, bool deferred) {
  textOut(F("Loop count: ") + String(loopCount));
  textOut(F(", Connection count: ") + String(connectionCount));
  textOut(F("\nLogged serial bytes: ") + String(serialByteCount));
  textOut(F("\nCurrent output mode last changed: ") + msTimeAgo(lastTimeOutputModeChanged));
 
  if(ci[SERIAL_FOR_COMMANDS_ONLY] == 0) {
    textOut(F(", Serial data parsed: ") + String(serialDataParsed));
    if(serialDataParsed > 0) {
      textOut(F(", Data found/data parsed: ") + String(serialByteCount/serialDataParsed));
    }
  }
  if(loopCount > 0){//don't want to divide by zero
    textOut(F("\nMillis/loop: ") + String(millis()/loopCount));
    textOut(F(", Serial bytes/loop: ") + String(serialByteCount/loopCount));
  }
  if(connectionCount > 0){ //don't want to divide by zero
    textOut(F(", Millis/connection: ") + String(millisecondsConnecting/connectionCount));
  }
  textOut("\n");
}

void cmdMemory(String* param, int argCount, bool deferred) {
  dumpMemoryStats(0);
}

void cmdResetInfo(String* param, int argCount, bool deferred) {
  textOut(F("Reset reason: ") + String(ESP.getResetReason()));
  textOut(F(", Reset info: ") + String(ESP.getResetInfo()));
  //textOut(F(", Reset pointer: ") + ESP.getResetInfoPtr());
  textOut("\n");
}

void cmdWifiInfo(String* param, int argCount, bool deferred) {
  textOut(F("Signal: ") + String(WiFi.RSSI()));
  textOut(F(", SSID: ") + WiFi.SSID());
  textOut("\n");
  textOut(F(" Local IP: ") + WiFi.localIP().toString());
  textOut(F(", MAC: ") + WiFi.macAddress());
  textOut("\n");
  textOut(F(" Gateway IP: ") + WiFi.gatewayIP().toString());
  textOut(F(", DNS IP: ") + WiFi.dnsIP().toString());
  textOut(F(", Subnet mask: ") + WiFi.subnetMask().toString());
  textOut("\n");
  textOut(F(" WiFi channel: ") + String(WiFi.channel()));
  textOut(F(", WiFi mode: ") + String(WiFi.getPhyMode()));
  textOut("\n");
}

ADC_MODE(ADC_VCC);

void cmdChipInfo(String* param, int argCount, bool deferred) {
  textOut(F("Flash chip size: ") + String(ESP.getFlashChipSize()));
  textOut(F(", Flash chip real size: ") + String(ESP.getFlashChipRealSize()));
  textOut(F(", Flash chip speed: ") + String(ESP.getFlashChipSpeed()));
  textOut(F(", Flash chip mode: ") + String(ESP.getFlashChipMode()));
  textOut("\n");
}

void cmdCpuInfo(String* param, int argCount, bool deferred) {
  textOut(F("Cpu frequency: ") + String(ESP.getCpuFreqMHz()));
  textOut(F(", Cycle count: ") + String(ESP.getCycleCount()));
  textOut(F("\n Chip ID: ") + String(ESP.getChipId()));
  textOut(F(", Core version: ") + String(ESP.getCoreVersion()));
  textOut(F(", SDK version: ") + String(ESP.getSdkVersion()));
  textOut(F(", Boot version: ") + String(ESP.getBootVersion()));
  textOut(F(", Boot mode: ") + String(ESP.getBootMode()));
  textOut("\n");
  textOut(F(" VCC voltage: ") + String(ESP.getVcc()));
  textOut("\n");
}

void cmdFlashInfo(String* param, int argCount, bool deferred) {
  textOut(F("Free sketch space: ") + String(ESP.getFreeSketchSpace()));
  textOut(F(", Sketch size: ") + String(ESP.getSketchSize()) + "\n");
}

void cmdDumpSlaveSerialData(String* param, int argCount, bool deferred) {
  char buffer[128]; 
  readDataParsedFromSlaveSerial();
  //parsedSerialData
  //uint8_t parsedBuf[40]  = {0xF1, 0xF2, 0xD1, 0xD2, 0xC1, 0xC2, 0xB1, 0xB2};
  bytesToHex(parsedSerialData, 20, 0x00, buffer);
  textOut(F("Parsed serial packet: ") + String(buffer) + "\n");
}

void cmdDumpMasterSerialData(String* param, int argCount, bool deferred){
  textOut(joinValsOnDelimiter(serialParsedData, "*", MAX_PARSED_SERIAL_VALUES) + "\n");
}

void cmdFormatFileSystem(String* param, int argCount, bool deferred) {
  formatFileSystem();
  //textOut(F("File system formatted\n"));
}
///////////////////////

void cmdAnomalyLogTest(String* param, int argCount, bool deferred) {
  anomalyLog(param[0]);
}

void cmdRenameFile(String* param, int argCount, bool deferred) {
  renameFile(param[0].c_str(), param[1].c_str());
}

void cmdDel(String* param, int argCount, bool deferred) {
  deleteFile(param[0].c_str());
}

void cmdDownload(String* param, int argCount, bool deferred) {
  downloadFile(param[0].c_str(), extractFilename(param[0]).c_str());
}

void cmdUpload(String* param, int argCount, bool deferred) {
  if(fileToUpload != "") {
    textOut(fileToUpload + F(" is still uploading; please wait\n"));
    return;
  }
  fileToUpload = param[0];
  File f = LittleFS.open(fileToUpload, "r");
  if (!f) {
      textOut(fileToUpload + F(": file not found\n"));
      fileToUpload = "";
      return;
  }
  f.close();
  //do it all in one go instead of as a series of polls
  int result = postFileUpload(fileToUpload.c_str(), String(ci[DEVICE_ID]).c_str());
  //textOut(String(result));
  fileToUpload = "";
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

///////////////////////////
 
void cmdSetSerialSwap(String* param, int argCount, bool deferred) {
 serialSwap(param[0].toInt());
 textOut(F("Serial swap set to: ") + String(param[0]) + "\n");
}

void cmdGetSerialSwap(String* param, int argCount, bool deferred) {
 textOut(F("Serial swap: ") + String(serialSwapped) + "\n");
}

void cmdSetSerialLogging(String* param, int argCount, bool deferred) {
  serialLogging = param[0].toInt();
  textOut(F("Serial logging set to "));
  if(String(param[1]).length() > 0) {
  serialLoggingFileName = String(param[1]);
  }
  if(serialLogging == 0) {
    textOut(F("off "));
  } else {
    textOut(F("on "));
  }
  if(serialLoggingFileName != "") {
    textOut(F("with filename '") + serialLoggingFileName + "'");
  }
  textOut("\n");
}

void cmdGetSerialLogging(String* param, int argCount, bool deferred) {
  textOut(F("Serial logging is "));
  if(serialLogging == 0) {
    textOut(F("off "));
  } else {
    textOut(F("on "));
  }
 if(serialLoggingFileName != "") {
  textOut(F("with filename '") + serialLoggingFileName + "'");
 }
 textOut("\n");
}

void cmdDumpParserConfig(String* param, int argCount, bool deferred) {
  if(blockCount == 0) {
    textOut(F("Serial parsing not configured; download a 'serialparser.cfg' file\n"));
  }
  for(int i=0; i<blockCount; i++) {
    dumpConfigBlock(blocks[i]);
  }
}

void cmdInitSerialParser(String* param, int argCount, bool deferred) {
  initSerialParser();
  if(blockCount == 0) {
    textOut(F("'serialparser.cfg' not found or it contained no valid configuration\n"));
  } else {
    textOut(F("Serial parsing initiated with data from 'serialparser.cfg'\n"));
  }
}


/////////////////////

void cmdResetSerial(String* param, int argCount, bool deferred) {
  setSerialRate((byte)ci[BAUD_RATE_LEVEL]); 
  ETS_UART_INTR_DISABLE();
  ETS_UART_INTR_ENABLE();
  textOut(F("Serial reset\n"));
}

void cmdDumpConfig(String* param, int argCount, bool deferred) {
  String fromWord = param[0];
  String source = param[1];

  if(ci[SLAVE_I2C] > 0 && ((ci[CONFIG_PERSIST_METHOD] == CONFIG_PERSIST_METHOD_I2C_SLAVE && source == "") || source == "slave")) {
    loadAllConfigFromEEPROM(1, 0);
  /*
  } else if ((ci[CONFIG_PERSIST_METHOD] == CONFIG_PERSIST_METHOD_FLASH && source == "") || source == "flash") {
    loadAllConfigFromFlash(1, 0);
  */
  } else if (ci[FRAM_ADDRESS] > 0  && ((ci[CONFIG_PERSIST_METHOD] == CONFIG_PERSIST_METHOD_FRAM && source == "") || source == "fram")) {
    loadAllConfigFromFRAM(1, 0);
  } else {
    loadAllConfigFromFlash(1, 0);
  }
}

void cmdDumpSlaveEeprom(String* param, int argCount, bool deferred) {
  loadAllConfigFromEEPROM(1, 512);
}

void cmdSendSlaveSerial(String* param, int argCount, bool deferred) {
  sendSlaveSerial(param[0].c_str());
  textOut(F("Serial data sent to slave: ") + param[0] + "\n");
}

void cmdSetSlaveTime(String* param, int argCount, bool deferred) {
  sendLong(ci[SLAVE_I2C], 180, param[0].toInt());
  textOut(F("Slave UNIX time set to: ") + param[0] + "\n");
}

void cmdGetSlaveTime(String* param, int argCount, bool deferred) {
  uint32_t unixTime = requestLong(ci[SLAVE_I2C], 181);
  textOut(F("Slave UNIX time: ") + String(unixTime) + "\n");
}

void cmdInitSlaveSerial(String* param, int argCount, bool deferred) {
  enableSlaveSerial(9);
  textOut(F("Serial on slave initiated\n"));
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
  textOut(F("Parsed slave value ") + param[0] + ": " + result + "\n");
}

void updateSlaveFirmware(String* param, int argCount, bool deferred) {
  String initialHttp = F("http://");
  millisAtPossibleReboot = millis();
  param[0].trim(); //this should contain a url for new firmware.  if it begins with "/" assume it is on the same host as everything else
  String flashUrl = "";
  textOut(F("Updating slave firmware...\n"));
  if(param[0].startsWith("http://")) { //if we get a full url
    flashUrl = param[0];
  } else if(param[0].charAt(0) == '/') {
    flashUrl = initialHttp + String(cs[HOST_GET]) + param[0]; //my firmware has an aversion to https!
  } else { //get the flash file from the backend using its security system, pulling it from the flash update directory, wherever it happens to be
    String encryptedStoragePassword = encryptStoragePassword(param[0]);
    flashUrl = initialHttp + String(cs[HOST_GET]) + String(cs[URL_GET]) + "?k2=" + encryptedStoragePassword + "&architecture=" + architecture + "&device_id=" + ci[DEVICE_ID] + "&mode=reflash&data=" + urlEncode(param[0], true);  
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
    possibleResult = flashUrl + F(" does not exist; no action taken\n");
    textOut(possibleResult);
    possibleEndingMessage = possibleResult;
  }
}

void getMasterEepromUsed(String* param, int argCount, bool deferred) {
  int bytesUsed = loadAllConfigFromEEPROM(2, 0);
  textOut(F("Slave EEPROM bytes used for master: ") + (String)bytesUsed + "\n");
}

void getSlaveEepromUsed(String* param, int argCount, bool deferred) {
  int bytesUsed = loadAllConfigFromEEPROM(2, 512);
  textOut(F("Slave EEPROM bytes used for slave: ") + (String)(bytesUsed - 512) + "\n");
}

void getSlave(String* param, int argCount, bool deferred) {
  param[0].trim(); 
  uint16_t result = getSlaveConfigItem((byte)param[0].toInt()); 
  textOut(F("Sought slave configuration: ") + String(result) + "\n");
}

void setSlaveParserBasis(String* param, int argCount, bool deferred) {
  String ordinalString = param[0];
  int ordinal = ordinalString.toInt();
  String value = param[1];
  css[ordinal] = strdup(value.c_str());
  textOut(F("Slave parser basis #") + ordinalString + " set to " + value + "\n");
}


void setSlaveBasis(String* param, int argCount, bool deferred) {
  String ordinalString = param[0];
  String value = param[1];
  int ordinal = ordinalString.toInt();
  cis[ordinal] = value.toInt();
  textOut(F("Slave parser basis #") + ordinalString + " set to " + value + "\n");
}

void setSlave(String* param, int argCount, bool deferred) {
  String ordinalString = param[0];
  String value = param[1];
  int ordinal = ordinalString.toInt();
 
  if(isInteger(ordinalString)) {
    setSlaveConfigItem(ordinal, value.toInt());
    textOut(F("Slave configuration ") + ordinalString + " set to: " + value + "\n");
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
          ordinalString + F(" run on slave with value: ") +
          value + "\n"
        );
  } else {
    long result = requestLong(ci[SLAVE_I2C], ordinal);
    textOut(F("Slave data for command ") + ordinalString + ": " + String(result) + "\n");
  }
}

void cmdSet(String* param, int argCount, bool deferred) {
  String key = param[0];
  String value = param[1];
 if(!isInteger(key)) {
  textOut(F("Configuration '") + key + F("' does not exist:\n"));
  return;
 }
 int configIndex = key.toInt();
 if(value != "") {
    value.trim();
    if(configIndex>=CONFIG_STRING_COUNT) {
      ci[configIndex] = value.toInt();
      //handle some specific sets in terms of what they do:
      if(key.toInt() == I2C_SPEED) {
        Wire.setClock(ci[I2C_SPEED] * 1000);
      }
    } else {   
      char buffer[50];    
      value.toCharArray(buffer, sizeof(buffer));
      cs[configIndex] = strdup(buffer); 
      

    }
  }
  textOut(F("Configuration set to: ") + value + "\n");
}  


void cmdGet(String* param, int argCount, bool deferred) {
  String key = param[0];
  if(!isInteger(key)) {
    textOut(F("Configuration '") + key + "' does not exist:\n");
    return;
  }
  int configIndex = key.toInt();
  String value;
  if(configIndex>=CONFIG_STRING_COUNT) {
    value = String(ci[configIndex]);
  } else {
    value = String(cs[configIndex]);
  }
  textOut(F("Sought configuration: ") + value + "\n");
}
