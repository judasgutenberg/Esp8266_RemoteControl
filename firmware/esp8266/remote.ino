/*
 * ESP8266 Remote Control. Also sends weather data from multiple kinds of sensors (configured in config.c) 
 * originally built on the basis of something I found on https://circuits4you.com
 * reorganized and extended by Gus Mueller, April 24 2022 - January 10 2026
 * Also resets a Moxee Cellular hotspot if there are network problems
 * since those do not include watchdog behaviors
 */

 //note:  this needs to be compiled in the Arduino environment alongside:
 //config.cpp
 //config.h
 //globals.cpp
 //globals.h
 //i2cslave.cpp
 //i2cslave.h
 //index.h
 //utilities.cpp
 //utilities.h

#include "globals.h"
#include "i2cslave.h"
#include "utilities.h"
#include "slaveupdate.h"
#include "commandhandlers.h"

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <Adafruit_INA219.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_ADT7410.h>
#include "Adafruit_EEPROM_I2C.h"
#include "Adafruit_FRAM_I2C.h"

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SimpleMap.h>
 

#include "Zanshin_BME680.h"  // Include the BME680 Sensor library
#include <DHT.h>
#include <Adafruit_AHTX0.h>
#include <SFE_BMP180.h>
#include <Adafruit_BMP085.h>
#include <Temperature_LM75_Derived.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>

#include <tuple>
#include <cmath> 
#include <math.h> // for isnan, NAN

#include <LittleFS.h>

#include "index.h" //Our HTML webpage contents with javascriptrons


//static globals for the state machine
static RemoteState remoteState = RS_IDLE;
static String remoteDatastring;      // original datastring param
static String remoteMode;            // mode param
static uint16_t remoteFRAMordinal;   // fRAMordinal param
static String remoteURL;             // final GET URL
static unsigned long stateStartMs = 0;
static unsigned long connectBackoffUntil = 0;
static int attemptCount = 0;
static String responseBufferSM;      // accumulate response
static uint32_t taskStartTimeMs = 0; // logging timer
//Jonathan Lemire had some ideas an ETSY, so he should've been in a subtrerrainian
void listFiles();
void formatFileSystem();
int loadAllConfigFromFlash(int mode, uint16_t addr);

typedef void (*CommandHandler)(String* args, int argCount, bool deferred);

struct CommandDef {
  const char* name;
  CommandHandler handler;
  int maxArgs;
  bool exactMatch;
  uint8_t configuration; //see below
  //bits specifying requirements in the higher byte:  <requires_deferment><unused><unused><requires_ir> 
  //bits specifying requirements in the lower byte:  <requires_rtc><requires_fram><requires_slave><requires_fs>
};
 
CommandDef commands[] = {
  {"reboot now",            cmdRebootEsp, 0,        true,         0b00000000},
  {"reboot slave",          cmdRebootSlave, 0,      true,         0b00000010},
  {"watchdog reboot",       cmdRebootMasterFromSlave, 0, true,    0b10000010},
  {"reboot",                cmdDeferredReboot, 0,   true,         0b10000000},
  {"update firmware",       cmdUpdateFirmware, 1,   true,         0b10000000},
  {"version",               cmdVersion, 0,          true,         0b00000000},
  {"run slave sketch",      cmdRunSlaveSketch, 0,   true,         0b00000010},
  {"slave bootloader",      cmdRunSlaveBootloader, 0, true,       0b00000010},
  {"pet watchdog",          cmdPetWatchdog, 0,      true,         0b00000010},
  {"get weather sensors",   cmdGetWeatherSensors, 0, true,        0b00000000},
  {"one pin at a time",     cmdOnePinAtATime, 0, false,           0b00000000},
  {"clear latency average", cmdClearLatencyAverage, 0, true,      0b00000000},
  {"ir",                    cmdIr, 1,               false,        0b00010000},
  {"clear fram",            cmdClearFram, 0,        true,         0b00000100},
  {"dump fram",             cmdDumpFram, 0,         true,         0b00000100},
  {"dump fram hex",         cmdDumpFramHex, 1,      false,        0b00000100}, 
  {"dump fram hex#",        cmdDumpFramHexAt, 1,    false,        0b00000100}, 
  {"swap fram",             cmdSwapFram, 0,         true,         0b00000100},
  {"dump fram record",      cmdDumpFramRecord, 1,   false,        0b00000100},
  {"get fram index",        cmdGetFramIndex, 0,     true,         0b00000100},
  {"set date",              cmdSetDate, 1,          false,        0b00001000},
  {"get date",              cmdGetDate, 0,          true,         0b00001000},
  {"get watchdog info",     cmdGetWatchdogInfo, 0,  true,         0b00000010},
  {"get watchdog data",     cmdGetWatchdogData, 0,  true,         0b00000010},
  {"ls",                    cmdListFiles, 0,        true,         0b00000001},
  {"save master config",    cmdSaveMasterConfig, 2, false,         0b00000000},
  {"save slave config",     cmdSaveSlaveConfig, 0,  true,         0b00000010},
  {"init master defaults",  cmdInitMasterDefaults, 0, true,       0b00000000}, 
  {"init slave defaults",   cmdInitSlaveDefaults, 0, true,        0b00000010},
  {"uptime",                cmdGetUptime, 0,        true,         0b00000000},
  {"get wifi uptime",       cmdGetWifiUptime, 0,    true,         0b00000000},
  {"get lastpoll",          cmdGetLastpoll, 0,      true,         0b00000000},
  {"get lastdatalog",       cmdGetLastdatalog, 0,   true,         0b00000000},
  {"memory",                cmdMemory, 0,           true,         0b00000000},
  {"dump parsed serial packet", cmdDumpSerialPacket, 0, true,     0b00000010},
  {"format file system",    cmdFormatFileSystem, 0, true,         0b00000001},
  ///////////
  {"rm",                    cmdDel, 1, false,                     0b00000001},
  {"download",              cmdDownload, 1, false,                0b00000001},
  //{"mkdir", cmdMkdir, 1, false, 0b00000000},
  {"upload",                cmdUpload, 1, false,                  0b00000001},
  {"cat",                   cmdCat, 1, false,                     0b00000001},
  {"read slave eeprom",     cmdReadSlaveEeprom, 1, false,         0b00000010},
  {"reset serial",          cmdResetSerial, 0, false,             0b00000000},
  {"dump config eeprom",    cmdConfigEeprom, 0, false,            0b00000010},
  {"dump slave config eeprom", cmdDumpSlaveEeprom, 0, false,      0b00000010},
  {"send slave serial",     cmdSendSlaveSerial, 1, false,         0b00000010},
  {"set slave time",        cmdSetSlaveTime, 1, false,            0b00000010},
  {"get slave time",        cmdGetSlaveTime, 0, false,            0b00000010},
  {"init slave serial",     cmdInitSlaveSerial, 0, false,         0b00000010},
  {"get slave serial",      getSlaveSerial, 0, false,             0b00000010},
  {"get slave parsed datum", getSlaveParsedDatum, 1, false,       0b00000010},
  {"update slave firmware", updateSlaveFirmware, 1, false,        0b00000010},
  {"get master eeprom used", getMasterEepromUsed, 0, false,       0b00000010},
  {"get slave eeprom used", getSlaveEepromUsed, 0, false,         0b00000010},
  {"get slave",             getSlave, 1, false,                   0b00000010},
  {"set slave parser basis", setSlaveParserBasis, 2, false,       0b00000010},
  {"set slave basis",       setSlaveBasis, 2, false,              0b00000010},
  {"set slave",             setSlave, 2, false,                   0b00000010},
  {"run slave",             runSlave, 2, false,                   0b00000010},
  {"set",                   cmdSet, 2, false,                     0b00000010},
  {"get",                   cmdGet, 1, false,                     0b00000010},
  // add more here…
};

// Start a new remote task (replacement for original sendRemoteData)
void startRemoteTask(const String& datastring, const String& mode, uint16_t fRAMordinal) {
  if(ci[POLLING_SKIP_LEVEL] < 1) {
    //Serial.println("////////////////////////skippy!");
    return;
  }
  if(remoteState != RS_IDLE) {
    // Already running a task. You could choose to queue multiple, but for now bail.
    // If you want to always start a new one, consider returning or queueing.
    //Serial.println("////////////////////////not idle!");
    return;
  }
  remoteDatastring = datastring;
  remoteMode = mode;
  remoteFRAMordinal = fRAMordinal;
  responseBufferSM = "";
  attemptCount = 0;
  taskStartTimeMs = millis();
  stateStartMs = millis();
  connectBackoffUntil = 0;
  remoteURL = ""; // will be built in RS_PREPARE
  remoteState = RS_PREPARE;
  additionalUrlParams = "";
  if(fileToUpload != "") { //we're uploading a file, so bypass other data
    String possibleDataString = packetString(fileToUpload, fileUploadPosition);
    if(possibleDataString == "") {
      fileToUpload = "";
      fileUploadPosition = 0;
    } else {
      remoteDatastring = possibleDataString;
      remoteMode = "savePacket";
      File f = LittleFS.open(fileToUpload, "r");
      if (!f) {
          textOut(fileToUpload + ": file not found\n");
          fileToUpload = "";
          return;
      }
      uint32_t totalFileSize = f.size();
      f.close();
      additionalUrlParams = "filename=" + fileToUpload + "&cursor=" +  fileUploadPosition + "&total_size=" + totalFileSize;
    }
  }
}

//ESP8266's home page:----------------------------------------------------
void handleRoot() {
 String s = MAIN_page; //Read HTML contents
 server.send(200, "text/html", s); //Send web page
}

void lookupLocalPowerData() {//sets the globals with the current reading from the ina219
  if(ci[INA219_ADDRESS] < 1) { //if we don't have a ina219 then do not bother
    return;
  }
  float shuntvoltage = 0;
  float busvoltage = 0;
  float current_mA = 0;
  float loadvoltage = 0;
  float power_mW = 0;
  shuntvoltage = ina219->getShuntVoltage_mV();
  busvoltage = ina219->getBusVoltage_V();
  current_mA = ina219->getCurrent_mA();
  power_mW = ina219->getPower_mW();
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  measuredVoltage = loadvoltage;
  measuredAmpage = current_mA;
}

String weatherDataString(int sensorId, int sensorSubtype, int dataPin, int powerPin, int i2c, int deviceFeatureId, char objectCursor, String sensorName, int ordinalOfOverwrite, int consolidateAllSensorsToOneRecord) {
  if(ci[POLLING_SKIP_LEVEL] < 10) {
    return "";
  }
  // --- locals ---
  double humidityFromSensor = NAN;
  double temperatureFromSensor = NAN;
  double pressureFromSensor = NAN;
  double gasFromSensor = NAN;
  String sensorValueStr = "";
  if (deviceFeatureId == 0) {
    objectCursor = 0;
  }
  // sensor-reading branches (unchanged)
  if (ci[SENSOR_ID] == 1) {
    if (powerPin > -1) {
      digitalWrite(powerPin, HIGH);
    }
    delay(10);
    if (i2c) {
      sensorValueStr = String(getPinValueOnSlave((char)i2c, (char)dataPin));
    } else {
      sensorValueStr = String(analogRead(dataPin));
    }
    if (powerPin > -1) {
      digitalWrite(powerPin, LOW);
    }
  } else if (ci[SENSOR_ID] == 53) {
    VL53L0X_RangingMeasurementData_t measure;
    lox[objectCursor].rangingTest(&measure, false);
    if (measure.RangeStatus != 4) {
      sensorValueStr = String(measure.RangeMilliMeter);
    } else {
      sensorValueStr = "-1";
    }
  } else if (sensorId == 680) {
    int32_t humidityRaw = 0, temperatureRaw = 0, pressureRaw = 0, gasRaw = 0;
    BME680[objectCursor].getSensorData(temperatureRaw, humidityRaw, pressureRaw, gasRaw);
    humidityFromSensor = (double)humidityRaw / 1000.0;
    temperatureFromSensor = (double)temperatureRaw / 100.0;
    pressureFromSensor = (double)pressureRaw / 100.0;
    gasFromSensor = (double)gasRaw / 100.0;
  } else if (sensorId == 2301) {
    if (powerPin > -1) {
      digitalWrite(powerPin, HIGH);
    }
    delay(10);
    humidityFromSensor = (double)dht[objectCursor]->readHumidity();
    temperatureFromSensor = (double)dht[objectCursor]->readTemperature();
    if (powerPin > -1) digitalWrite(powerPin, LOW);
  } else if (sensorId == 280) {
    temperatureFromSensor = BMP280[objectCursor].readTemperature();
    pressureFromSensor = BMP280[objectCursor].readPressure() / 100.0;
  } else if (sensorId == 2320) {
    sensors_event_t humidityEvent, tempEvent;
    AHT[objectCursor].getEvent(&humidityEvent, &tempEvent);
    humidityFromSensor = humidityEvent.relative_humidity;
    temperatureFromSensor = tempEvent.temperature;
  } else if (sensorId == 7410) {
    temperatureFromSensor = adt7410[objectCursor].readTempC();
  } else if (sensorId == 180) {
    char status = BMP180[objectCursor].startTemperature();
    if (status != 0) {
      delay(status);
      if (BMP180[objectCursor].getTemperature(temperatureFromSensor) != 0) {
        if (BMP180[objectCursor].startPressure(3) != 0) {
          delay(status);
          BMP180[objectCursor].getPressure(pressureFromSensor, temperatureFromSensor);
        }
      }
    }
  } else if (sensorId == 85) {
    temperatureFromSensor = BMP085d[objectCursor].readTemperature();
    pressureFromSensor = BMP085d[objectCursor].readPressure() / 100.0;
  } else if (sensorId == 75) {
    temperatureFromSensor = LM75[objectCursor].readTemperatureC();
  } else {
    // no sensor
  }

  // --- Move big buffers out of stack into static storage ---
  const int FIELDS = 12;
  const int FIELD_MAX = 48;   // reduced per-field size; adjust if you expect big fields
  static char fields[FIELDS][FIELD_MAX];
  for (int i = 0; i < FIELDS; ++i) {
    fields[i][0] = '\0';
  }

  // fill fields (check bounds)
  if (!isnan(temperatureFromSensor)) {
    snprintf(fields[0], FIELD_MAX, "%.2f", temperatureFromSensor);
  }
  if (!isnan(pressureFromSensor)) {
    snprintf(fields[1], FIELD_MAX, "%.2f", pressureFromSensor);
  }
  if (!isnan(humidityFromSensor)){
    snprintf(fields[2], FIELD_MAX, "%.2f", humidityFromSensor);
  }
  if (!isnan(gasFromSensor)){
    snprintf(fields[3], FIELD_MAX, "%.2f", gasFromSensor);
  }

  if (ordinalOfOverwrite >= 0 && ordinalOfOverwrite < FIELDS) {
    String useVal = sensorValueStr;
    if (useVal.length() == 0) {
      if (sensorSubtype == 1) {
        useVal = String(pressureFromSensor);
      } else if (sensorSubtype == 2) {
        useVal = String(humidityFromSensor);
      } else if (sensorSubtype == 3) {
        useVal = String(gasFromSensor);
      } else {
        useVal = String(temperatureFromSensor);
      }
    }
    strncpy(fields[ordinalOfOverwrite], useVal.c_str(), FIELD_MAX - 1);
    fields[ordinalOfOverwrite][FIELD_MAX - 1] = '\0';
  }

  // big transmission buffer (static)
  static char tx[1600]; // increase if required
  size_t pos = 0;
  const size_t bufSize = sizeof(tx);
  // append first 4 fields with '*' delimiter, safe checks after every write
  for (int i = 0; i < 4; ++i) {
    if (i > 0) {
      if (pos + 1 < bufSize) {
        tx[pos++] = '*';
      } else { 
        tx[bufSize - 1] = '\0'; return String(tx); 
      }
    }
    if (fields[i][0] != '\0') {
      int n = snprintf(tx + pos, bufSize - pos, "%s", fields[i]);
      if (n < 0) { 
        tx[bufSize - 1] = '\0'; 
        return String(tx); 
      }
      pos += (size_t)n;
      if (pos >= bufSize) { 
        tx[bufSize - 1] = '\0'; 
        return String(tx); 
      }
    }
  }
  //send memory telemetry in reserved fields if configured to
  if(ci[SEND_MEM_DATA_IN_RESERVED] == 1) {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    uint32_t fragPct = 100 - (maxBlock * 100 / freeHeap);
    
    // worst case size estimate:
    // 9 stars + 3 numbers (~10 digits each) + 2 separators + null ˜ ~45 chars
    int n = snprintf(
      tx + pos,
      bufSize - pos,
      "*********%u*%u*%u**",
      freeHeap,
      maxBlock,
      fragPct
    );
    
    if (n < 0) {
      tx[bufSize - 1] = '\0';
      return String(tx);
    }
    
    pos += (size_t)n;
    
    if (pos >= bufSize) {
      tx[bufSize - 1] = '\0';
      return String(tx);
    }
  } else {
    // append reserved stars (13 chars)
    if (pos + 13 < bufSize) {
      memcpy(tx + pos, "*************", 13);
      pos += 13;
    } else {
      tx[bufSize - 1] = '\0';
      return String(tx);
    }
  }

  // offline FRAM logging (keeps existing behavior)
  if (offlineMode) {
    if (millis() - lastOfflineLog > 1000 * ci[OFFLINE_LOG_GRANULARITY]) {
      unsigned long millisVal = millis();
      if (ci[FRAM_ADDRESS] > 0) {
        std::vector<std::tuple<uint8_t, uint8_t, double>> framWeatherRecord;
        if (!isnan(temperatureFromSensor)) {
          addOfflineRecord(framWeatherRecord, 0, 5, temperatureFromSensor);
        }
        if (!isnan(pressureFromSensor))    {
          addOfflineRecord(framWeatherRecord, 1, 5, pressureFromSensor);
        }
        if (!isnan(humidityFromSensor))    {
          addOfflineRecord(framWeatherRecord, 2, 5, humidityFromSensor);
        }
        if (ci[RTC_ADDRESS] > 0) {
          addOfflineRecord(framWeatherRecord, 32, 2, currentRTCTimestamp());
          if(ci[DEBUG] > 0) {
            Serial.println(currentRTCTimestamp());
          }
        } else {
          addOfflineRecord(framWeatherRecord, 32, 2, timeClient.getEpochTime());
        }
        writeRecordToFRAM(framWeatherRecord);
        if(ci[DEBUG] > 0) {
          Serial.println(F("Saved a record to FRAM."));
        }
        // print trimmed tx for info
        tx[(pos < bufSize - 1) ? pos : bufSize - 1] = '\0';
        if(ci[DEBUG] > 0) {
          Serial.print(tx);
          Serial.println(millisVal);
        }
      }
      lastOfflineLog = millis();
    }
  }

  // append sensor id, device feature id, name, consolidate flag
  int n = snprintf(tx + pos, bufSize - pos, "%ld*%d*%s*%d", (long)ci[SENSOR_ID], deviceFeatureId, sensorName.c_str(), consolidateAllSensorsToOneRecord);
  if (n > 0) {
    pos += (size_t)n;
    if (pos >= bufSize) pos = bufSize - 1;
  }

  tx[(pos < bufSize) ? pos : (bufSize - 1)] = '\0';
  return String(tx); // single String allocation only
}

void startWeatherSensors(int sensorIdLocal, int sensorSubTypeLocal, int i2c, int pinNumber, int powerPin) {
  //i've made all these inputs generic across different sensors, though for now some apply and others do not on some sensors
  //for example, you can set the i2c address of a BME680 or a BMP280 but not a BMP180.  you can specify any GPIO as a data pin for a DHT
  int objectCursor = 0;
  if(sensorObjectCursor->has((String)ci[SENSOR_ID])) {
    objectCursor = sensorObjectCursor->get((String)sensorIdLocal);;
  } 
  if(sensorIdLocal == 1) { //simple analog input
    //all we need to do is turn on power to whatever the analog device is
    if(powerPin > -1) {
      pinMode(powerPin, OUTPUT);
      digitalWrite(powerPin, LOW);
    }
  /*
  //if i implemented an IR transmitter as a weather device:
  } else if(sensorIdLocal == 2) { //IR sender LED, not weather equipment, but a device certainly
    //we don't need the objectCursor system because there will only ever be one ir sender diode
    if(pinNumber > -1) { 
      irsend.begin();
    }
  */
  } else if(sensorIdLocal == 53) {
    if(!lox[objectCursor].begin(i2c)) {
      if(ci[DEBUG] > 0) {
        Serial.println(F("Failed to boot VL53L0X"));
      }
    } else {
      if(ci[DEBUG] > 0) {
        Serial.print(F("VL53L0X at "));
        Serial.println(i2c);
      }
    }
  } else if(sensorIdLocal == 680) {
    if(ci[DEBUG] > 0) {
      Serial.print(F("Initializing BME680 sensor...\n"));
    }
    if (!BME680[objectCursor].begin((uint8_t)i2c)) {  // Start B DHTME680 using I2C, use first device found
      if(ci[DEBUG] > 0) {
        Serial.print(F(" - Unable to find BME680.\n"));
      }
    } 
    if(ci[DEBUG] > 0) {
      Serial.print(F("- Setting 16x oversampling for all sensors\n"));
    }
    BME680[objectCursor].setOversampling(TemperatureSensor, Oversample16);  // Use enumerated type values
    BME680[objectCursor].setOversampling(HumiditySensor, Oversample16);     // Use enumerated type values
    BME680[objectCursor].setOversampling(PressureSensor, Oversample16);     // Use enumerated type values
    //Serial.print(F("- Setting IIR filter to a value of 4 samples\n"));
    BME680[objectCursor].setIIRFilter(IIR4);  // Use enumerated type values
    //Serial.print(F("- Setting gas measurement to 320\xC2\xB0\x43 for 150ms\n"));  // "?C" symbols
    BME680[objectCursor].setGas(320, 150);  // 320?c for 150 milliseconds
  } else if (sensorIdLocal == 2301) {
    if(ci[DEBUG] > 0) {
      Serial.print(F("Initializing DHT AM2301 sensor at pin: "));
    }
    if(powerPin > -1) {
      pinMode(powerPin, OUTPUT);
      digitalWrite(powerPin, LOW);
    }
    dht[objectCursor] = new DHT(pinNumber, sensorSubTypeLocal);
    dht[objectCursor]->begin();
  } else if (sensorIdLocal == 2320) { //AHT20
    if (AHT[objectCursor].begin()) {
      if(ci[DEBUG] > 0) {
        Serial.println(F("Found AHT20"));
      }
    } else {
      if(ci[DEBUG] > 0) {
        Serial.println(F("Didn't find AHT20"));
      }
    }  
  } else if (sensorIdLocal == 7410) { //adt7410
    adt7410[objectCursor].begin(i2c);
    adt7410[objectCursor].setResolution(ADT7410_16BIT);
  } else if (sensorIdLocal == 180) { //BMP180
    BMP180[objectCursor].begin();
  } else if (sensorIdLocal == 85) { //BMP085
    if(ci[DEBUG] > 0) {
      Serial.print(F("Initializing BMP085...\n"));
    }
    BMP085d[objectCursor].begin();
  } else if (sensorIdLocal == 280) {
    if(ci[DEBUG] > 0) {
      Serial.print(F("Initializing BMP280 at i2c: "));
      Serial.print((int)i2c);
      Serial.print(F(" objectcursor:"));
      Serial.print((int)objectCursor);
      Serial.println();
    }
    if(!BMP280[objectCursor].begin(i2c)){
      if(ci[DEBUG] > 0) {
        Serial.println(F("Couldn't find BMX280!"));
      }
    }
  }
  sensorObjectCursor->put((String)sensorIdLocal, objectCursor + 1); //we keep track of how many of a particular ci[SENSOR_ID] we use
}

void handleWeatherData() {
  String transmissionString = weatherDataString(ci[SENSOR_ID], ci[SENSOR_SUB_TYPE], ci[SENSOR_DATA_PIN], ci[SENSOR_POWER_PIN], ci[SENSOR_I2C], NULL, 0, deviceName, -1, ci[CONSOLIDATE_ALL_SENSORS_TO_ONE_RECORD]);
  server.send(200, "text/plain", transmissionString); //Send values only to client ajax request
}

void compileAndSendDeviceData(const String& weatherData,const String& whereWhenData, const String& powerData, bool doPinCursorChanges, uint16_t fRAMOrdinal) {
    if(ci[POLLING_SKIP_LEVEL] < 8){
      return;
    }
    // Large fixed buffer (adjust as needed)
    static char tx[2048];
    int pos = 0;

    // --- WEATHER DATA -------------------------------------------------
    if (weatherData.length() > 0) {
        pos += snprintf(tx + pos, sizeof(tx) - pos, "%s", weatherData.c_str());
    } else if (ci[SENSOR_ID] > -1) {
        String s = weatherDataString(ci[SENSOR_ID], ci[SENSOR_SUB_TYPE], ci[SENSOR_DATA_PIN], ci[SENSOR_POWER_PIN], ci[SENSOR_I2C], NULL, 0, deviceName, -1, ci[CONSOLIDATE_ALL_SENSORS_TO_ONE_RECORD]);
        pos += snprintf(tx + pos, sizeof(tx) - pos, "%s", s.c_str());
    }

    // --- ADDITIONAL SENSOR DATA ---------------------------------------
    String additionalSensor = handleDeviceNameAndAdditionalSensors(
        (char*)additionalSensorInfo.c_str(), false
    );
    if (pos == 0 && additionalSensor.startsWith("!")) {
        additionalSensor.remove(0, 1);   // strip leading !
    }
    pos += snprintf(tx + pos, sizeof(tx) - pos, "%s", additionalSensor.c_str());
    // --- WHERE / WHEN -------------------------------------------------
    if (whereWhenData.length() > 0) {
        pos += snprintf(tx + pos, sizeof(tx) - pos, "|%s", whereWhenData.c_str());
    } else {
        pos += snprintf(tx + pos, sizeof(tx) - pos, "|*%lu*%lu*", millis(), timeClient.getEpochTime());
    }
    // latency
    if (latencyCount > 0) {
        pos += snprintf(tx + pos, sizeof(tx) - pos, "%u", (1000 * latencySum) / latencyCount);
    }
    // latitude*longitude*elevation*velocity*uncertainty placeholder
    pos += snprintf(tx + pos, sizeof(tx) - pos, "*****");
    // slave I2C data
    if (ci[SLAVE_I2C] > 0 && lastSlavePowerMode < 2) { //don't get this info with every poll if the slave is trying to sleep more deeply than 1
        String s = slaveWatchdogData();
        pos += snprintf(tx + pos, sizeof(tx) - pos, "*%s", s.c_str());
    }
    // --- POWER DATA ----------------------------------------------------
    if (powerData.length() > 0) {
        pos += snprintf(tx + pos, sizeof(tx) - pos, "|%s", powerData.c_str());
    } else {
        pos += snprintf(tx + pos, sizeof(tx) - pos, "|*%f*%f", measuredVoltage, measuredAmpage);
    }
    // future expansion
    pos += snprintf(tx + pos, sizeof(tx) - pos, "|||");
    // --- EXTRA INFO ----------------------------------------------------
    pos += snprintf(tx + pos, sizeof(tx) - pos, "|");
    // clean up IP address
    String ipToUse = ipAddress;
    int sp = ipToUse.indexOf(' ');
    if (sp > 0){
      ipToUse.remove(sp);
    }
    if (ipAddressAffectingChange.length() > 0) {
        ipToUse = ipAddressAffectingChange;
        changeSourceId = 1;
    }
    // pin cursor update
    if (onePinAtATimeMode && doPinCursorChanges) {
        pinCursor++;
        if (pinCursor >= pinTotal) pinCursor = 0;
    }
    pos += snprintf(tx + pos, sizeof(tx) - pos,
        "%d*%d*%d*%s*%d*%d*%d",
        lastCommandId, pinCursor, (int)localSource,
        ipToUse.c_str(), (int)requestNonJsonPinInfo,
        (int)justDeviceJson, changeSourceId
    );
    // pinMap
    {
        String s = joinStdMapValsOnDelimiter(pinMap, "*");
        pos += snprintf(tx + pos, sizeof(tx) - pos, "|%s", s.c_str());
    }
    // moxee reboot info
    {
        String s = joinValsOnDelimiter(moxeeRebootTimes, "*", 10);
        pos += snprintf(tx + pos, sizeof(tx) - pos, "|%s", s.c_str());
    }
    // --- SEND OUT ------------------------------------------------------
    if (!offlineMode) {
        startRemoteTask(String(tx), "getDeviceData", fRAMOrdinal);
    }
}

void wiFiConnect() {
  const int NUM_WIFI_CREDENTIALS = 5;
  unsigned long lastDotTime = 0;
  unsigned long lastOfflineReconnectAttemptTime = millis();
  bool connected = false;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);
  uint8_t validWiFiAttempts = 0;
  for (int attempt = 0; attempt < NUM_WIFI_CREDENTIALS; attempt++) {
    int ssidIndex = (currentWifiIndex + attempt) % NUM_WIFI_CREDENTIALS;
    const char* wifiSsid = cs[WIFI_SSID + ssidIndex * 2];
    const char* wifiPassword = cs[WIFI_PASSWORD + ssidIndex * 2];
    if (wifiSsid && wifiSsid[0] != '\0') {
      validWiFiAttempts++;
      if(ci[DEBUG] > 0) {
        Serial.printf("\nAttempting WiFi connection to: %s\n", wifiSsid);
      }
      WiFi.disconnect(true);
      unsigned long disconnectTime = millis();
      // short wait for disconnect to take effect (non-blocking)
      while (millis() - disconnectTime < 100) {
        yield();
      }
      WiFi.begin(wifiSsid, wifiPassword);
      int wiFiSeconds = 0;
      bool initialAttemptPhase = true;
      lastOfflineReconnectAttemptTime = millis();
      while (WiFi.status() != WL_CONNECTED) {
        delay(5);
        unsigned long now = millis();
        // print dot every second
        if (now - lastDotTime >= 1000) {
          if(ci[DEBUG] > 0) {
            Serial.print(".");
          }
          lastDotTime = now;
          wiFiSeconds++;
        }
        // print asterisk and retry every 10 seconds
        if (now - lastOfflineReconnectAttemptTime > 10000) {
          WiFi.disconnect();
          WiFi.begin(wifiSsid, wifiPassword);
          lastOfflineReconnectAttemptTime = now;
          if(ci[DEBUG] > 0) {
            Serial.print("*");
          }
        }
        if (WiFi.status() == WL_NO_SSID_AVAIL) {
          if(ci[DEBUG] > 0) {
            Serial.printf("\nSSID not found: %s\n", wifiSsid);
          }
          break; // try next SSID
        }
        // timeout handling
        uint32_t wifiTimeoutToUse = ci[WIFI_TIMEOUT];
        if (knownMoxeePhase == 0) {
          wifiTimeoutToUse = ci[GRANULARITY_WHEN_IN_MOXEE_PHASE_0];
        } else {
          wifiTimeoutToUse = ci[GRANULARITY_WHEN_IN_MOXEE_PHASE_1];
        }
        if (wiFiSeconds > wifiTimeoutToUse) {
          Serial.print(wiFiSeconds);
          Serial.print(" ");
          Serial.println(wifiTimeoutToUse);
          if(ci[DEBUG] > 0) {
            Serial.println("");
            Serial.println(F("WiFi taking too long"));
          }
          if(ci[MOXEE_POWER_SWITCH] > 0) {
            if(ci[DEBUG] > 0) {
              Serial.println(F("rebooting Moxee"));
            }
            rebootMoxee();
          }
          if(ci[DEBUG] > 0) {
            Serial.println(F("trying another"));
          }
          wiFiSeconds = 0;
          initialAttemptPhase = false;
          break; // move to next SSID
        }
      }
      if (!initialAttemptPhase && wiFiSeconds > (ci[WIFI_TIMEOUT] / 2) &&  ci[FRAM_ADDRESS] > 0) {
        offlineMode = true;
        haveReconnected = false;
        return;
      }
      yield(); // keep Wi-Fi background tasks alive
    }
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      currentWifiIndex = (ssidIndex + 1) % NUM_WIFI_CREDENTIALS; // next SSID next time
      if(ci[DEBUG] > 0) {
        Serial.printf("\nConnected to %s, IP: %s\n", wifiSsid, WiFi.localIP().toString().c_str());
      }
      ipAddress = WiFi.localIP().toString();
      break;
    }
  }
  if (!connected) {
    if(validWiFiAttempts > 0) {
      if(ci[DEBUG] > 0) {
        Serial.println(F("\nAll WiFi attempts failed."));
      }
    }
    if (ci[FRAM_ADDRESS] > 0){
      
    }
    //had required a FRAM_ADDRESS but perhaps not
    offlineMode = true;
    haveReconnected = false;
  } else {
    offlineMode = false;
    haveReconnected = true;
  }
}

// Call this frequently from loop()
void runRemoteTask() {
  yield();
  if(offlineMode) {
    return;
  }
  switch(remoteState) {

    case RS_IDLE:
      if(ci[DEBUG] > 20) {
        Serial.println(F("RS_IDLE"));
      }
      return;

    // -------------------- prepare the URL and initial decisions --------------------
    case RS_PREPARE: {
      if(ci[DEBUG] > 10) {
        Serial.println(F("RS_PREPARE"));
      }
      // Reproduce the decision logic in your original function:
      // decide if mode should become saveData etc
      if(remoteMode == "getDeviceData") {
        if(millis() - lastDataLogTime > ci[DATA_LOGGING_GRANULARITY] * 1000UL || lastDataLogTime == 0) {
          remoteMode = "saveData";
        }
        if(deviceName == "") {
          remoteMode = "getInitialDeviceInfo";
        }
      }
      String encryptedStoragePassword = encryptStoragePassword(remoteDatastring);
      // build URL exactly like before
      remoteURL = String(cs[URL_GET]) + "?k2=" + encryptedStoragePassword + "&device_id=" + ci[DEVICE_ID] + "&mode=" + remoteMode + "&data=" + urlEncode(remoteDatastring, true);
      if(additionalUrlParams != "") {
        remoteURL += "&" + additionalUrlParams;
      }
      if(ci[DEBUG] > 0) {
        Serial.println(remoteURL);
      }
      // initialize connect attempt spacing
      connectBackoffUntil = 0;
      attemptCount = 0;
      stateStartMs = millis();
      remoteState = RS_CONNECTING;
      return;
    }

    // -------------------- attempt non-blocking connect --------------------
    case RS_CONNECTING: {
      if(ci[DEBUG] > 10) {
        Serial.println(F("RS_CONNECTING"));
      }
      // space out attempts by CONNECT_RETRY_SPACING_MS without blocking
      if(millis() < connectBackoffUntil) {
        // still waiting to try again
        remoteState = RS_CONNECT_WAIT;
        yield();
        return;
      }
      if (clientGet.connected()) {
         if(ci[DEBUG] > 20) {
          Serial.println(F("RS_CONNECTING...connected"));
        }
        // should not be connected here, but if it is, go send request
        remoteState = RS_SENDING_REQUEST;
        stateStartMs = millis();
        yield();
        return;
      }
      // Try to connect once
      if(clientGet.connect(cs[HOST_GET], 80)) {
        if(ci[DEBUG] > 20) {
          Serial.println(F("RS_CONNECTING...port 80"));
        }
        remoteState = RS_SENDING_REQUEST;
        stateStartMs = millis();
        connectionFailureMode = false;
        yield();
      } else {
         if(ci[DEBUG] > 20) {
          Serial.println(F("RS_CONNECTING...otherwise"));
        }
        yield();
        attemptCount++;
        connectBackoffUntil = millis() + CONNECT_RETRY_SPACING_MS;
        // If we've exceeded retries, treat as failure (same reaction as original)
        if (attemptCount >= ci[CONNECTION_RETRY_NUMBER]) {
          // connection failed permanently; set failure globals and reboot moxee
          connectionFailureTime = millis();
          connectionFailureMode = true;
          rebootMoxee();
          yield();
          if(ci[DEBUG] > 1) {
            Serial.println();
            Serial.print(F("Connection failed (host): "));
            Serial.println(cs[HOST_GET]);
          }
          // finish the task
          remoteState = RS_DONE;
        } else {
          // wait for next attempt
          remoteState = RS_CONNECT_WAIT;
        }
      }
      return;
    }

    case RS_CONNECT_WAIT: {
      if(ci[DEBUG] > 10) {
        Serial.println(F("RS_CONNECT_WAIT"));
      }
      // just transition back to CONNECTING when time has passed
      yield();
      if(millis() >= connectBackoffUntil) {
        remoteState = RS_CONNECTING;
      }
      return;
    }
    // -------------------- send the HTTP request (non-blocking single-shot) --------------------
    case RS_SENDING_REQUEST: {
      if(ci[DEBUG] > 10) {
        Serial.println(F("RS_SENDING_REQUEST"));
      }
      yield();
      // send the GET request in one go (small, so ok)
      clientGet.println(F("GET ") + remoteURL + " HTTP/1.1");
      clientGet.print(F("Host: "));
      clientGet.println(cs[HOST_GET]);
      clientGet.println(F("User-Agent: ESP8266/1.0"));
      clientGet.println(F("Accept-Encoding: identity"));
      clientGet.println(F("Connection: close"));
      clientGet.println();
      yield();
      remoteState = RS_WAITING_FOR_REPLY;
      return;
    }
    // -------------------- wait for any reply (with timeout) --------------------
    case RS_WAITING_FOR_REPLY: {
      if(ci[DEBUG] > 10) {
        Serial.println(F("RS_WAITING_FOR_REPLY"));
      }
      if(clientGet.available() > 0) {
        yield();
        // Got first bytes — start reading
        remoteState = RS_READING_REPLY;
        stateStartMs = millis();
        return;
      }
      // timeout for first response byte
      if(millis() - stateStartMs > REPLY_AVAIL_TIMEOUT_MS) {
        yield();
        // try a simpler connection as your original attempted; here we simply abort and finish
        // Close socket and mark done (original did some attempt, then returned)
        clientGet.stop();
        remoteState = RS_DONE;
        return;
      }
      // otherwise keep waiting (non-blocking)
      return;
    }
    // -------------------- drain the socket into responseBufferSM (non-blocking) --------------------
    case RS_READING_REPLY: {
      if(ci[DEBUG] > 10) {
        Serial.println(F("RS_READING_REPLY"));
      }
      // Read everything currently available; do not block waiting for more
      while (clientGet.available() > 0) {
        yield();
        char c = clientGet.read();
        responseBufferSM += c;
        // protect memory: if it's huge, stop reading further to avoid OOM
        if(responseBufferSM.length() > (int)MAX_RESPONSE_SIZE) {
          // we reached a safety cap; break and close socket
          break;
        }
        // yield rarely is not needed inside here, but we return below to keep loop small
      }
      // If remote closed or we've read a lot and connection is no longer active, move to processing
      if (!clientGet.connected()) {
        yield();
        // remote closed gracefully — good time to process
        clientGet.stop();
        remoteState = RS_PROCESSING_REPLY;
        stateStartMs = millis();
        return;
      }
      // If buffer is huge enough to process now, close and process
      if(responseBufferSM.length() > 4096) {
        yield();
        clientGet.stop();
        remoteState = RS_PROCESSING_REPLY;
        stateStartMs = millis();
        return;
      }
      yield();
      // If we've been waiting too long since first byte, close and process what we have
      if (millis() - stateStartMs > CONNECT_TIMEOUT_MS) {
        yield();
        clientGet.stop();
        remoteState = RS_PROCESSING_REPLY;
        stateStartMs = millis();
        return;
      }
      // else continue reading later (non-blocking)
      return;
    }
    // -------------------- process the response AFTER socket is closed --------------------
    case RS_PROCESSING_REPLY: {
      if(ci[DEBUG] > 10) {
        Serial.println(F("RS_PROCESSING_REPLY"));
      }
      bool receivedData = false;
      bool receivedDataJson = false;
      // Make a mutable working buffer ONCE
      // (You can reuse this across calls if you want to go full zero-allocation)
      int len = responseBufferSM.length();
      char *buf = (char*)malloc(len + 1);
      if (!buf) {
        return;
      }
      memcpy(buf, responseBufferSM.c_str(), len);
      buf[len] = '\0';
      char *line = buf;
      char *next;
      while (line && *line) {
        yield();
        // find next newline
        next = strchr(line, '\n');
        if (next) {
          *next = '\0';   // terminate this line
          next++;         // move to start of next
        }
        // --- trim in place ---
        while (*line == ' ' || *line == '\r' || *line == '\t') line++;
        char *end = line + strlen(line) - 1;
        while (end > line && (*end == ' ' || *end == '\r' || *end == '\t')) {
          *end-- = '\0';
        }
        if (*line == '\0') {
          line = next;
          continue;
        }
        char first = line[0];
        
        //backend confirmation logic (heap-safe version)
        // check: line does NOT contain "\"error\":"
        bool hasError = (strstr(line, "\"error\":") != NULL);
        // check: first character
        char firstChar = line[0];
        bool validStart = (firstChar == '{' || firstChar == '*' || firstChar == '|' || firstChar == '=');
        // check: remoteMode (still a String, but not created in loop)
        bool validMode = (remoteMode == F("saveData") || remoteMode == F("commandout") || remoteMode == F("savePacket"));
        if (!hasError && validMode && validStart) {
          permissionErrorCount = 0;
          lastDataLogTime = millis();
          moxeeRebootCount = 0;
          for (int i = 0; i < 11; i++){
            moxeeRebootTimes[i] = 0;
          }
          if (lastCommandLogId == 0 && responseBuffer == "" && outputMode == 0) {
            canSleep = true;
          }
          if (remoteMode == F("commandout") || outputMode == 2) {
            lastCommandLogId = 0;
          }
          // run deferred command safely
          if (deferredCommand && deferredCommand[0] != '\0') {
            yield();
            runCommand(deferredCommand, true);
          }
          outputMode = 0;
          responseBuffer = "";
        } else if(hasError) {
          permissionErrorCount++;
        }
        if(permissionErrorCount > 10) {
          initMasterDefaults(); //if we come up with the wrong configuration stored in alterable storage, we will have repeated permission errors, so, we need to use the baked-in configuration
        }
        // ============================
        // HANDLE CASES 
        // ============================
        if (first == '*') {
          if (ci[DEBUG] > 1) {
            Serial.print(F("Initial: "));
            Serial.println(line);
          }
          if (cs[SENSOR_CONFIG_STRING] != "") {
            // ⚠️ this still uses String, but outside tight loop frequency
            String tmp = line;
            tmp = replaceFirstOccurrenceAtChar(tmp, String(cs[SENSOR_CONFIG_STRING]), '|');
            strncpy(line, tmp.c_str(), len); // copy back
          }
          additionalSensorInfo = line;
          handleDeviceNameAndAdditionalSensors((char *)additionalSensorInfo.c_str(), true);
          break;
        } else if (first == '{') {
          if (ci[DEBUG] > 1) {
            Serial.print(F("JSON: "));
            Serial.println(line);
          }
          receivedDataJson = true;
          break;
        } else if (first == '|') {
          if (ci[DEBUG] > 1) {
            Serial.print(F("delimited: "));
            Serial.println(line);
          }
          // split in-place on '!'
          char *parts[3] = {0};
          int part = 0;
          parts[part++] = line;
          for (char *p = line; *p && part < 3; p++) {
            if (*p == '!') {
              *p = '\0';
              parts[part++] = p + 1;
            }
          }
          setLocalHardwareToServerStateFromNonJson(parts[0]);
          if (part > 1 && strlen(parts[1]) > 5) {
            if (ci[DEBUG] > 1) {
              Serial.print(F("COMMAND: "));
              Serial.println(parts[1]);
            }
          }
          if (lastCommandLogId == 0 && part > 2) {
            lastCommandLogId = strtoul(parts[2], NULL, 10);
            if (lastCommandLogId > 0) {
              canSleep = false;
              deferredCanSleep = true;
            } else if (deferredCanSleep) {
              canSleep = true;
              deferredCanSleep = false;
            }
            // build "!command" WITHOUT String
            char cmdBuf[128];
            cmdBuf[0] = '!';
            strncpy(cmdBuf + 1, parts[1] ? parts[1] : "", sizeof(cmdBuf) - 2);
            cmdBuf[sizeof(cmdBuf) - 1] = '\0';
            runCommand(cmdBuf, false);
          }
          receivedDataJson = true;
          break;
        } else if (first == '!') {
          if (strchr(line, '|')) {
            runCommand(line, false);
            break;
          } else {
            fileUploadPosition = atoi(line + 1);
            File f = LittleFS.open(fileToUpload, "r");
            if (!f) {
              textOut(fileToUpload + F(": file not found\n"));
              fileToUpload = "";
              free(buf);
              return;
            }
            uint32_t totalFileSize = f.size();
            if (totalFileSize >= fileUploadPosition) {
              textOut(fileToUpload + F(" has finished uploading\n"));
              fileToUpload = "";
            } else {
              char pct[32];
              snprintf(pct, sizeof(pct), "%u%%\n",
                       (100 * fileUploadPosition) / totalFileSize);
              textOut(pct);
            }
            break;
          }
        }
        else {
          if (ci[DEBUG] > 2) {
            Serial.print(F("web data: "));
            Serial.println(line);
          }
        }
        receivedData = true;
        line = next;
      }
      free(buf);
      remoteState = RS_DONE;
      return;
    }
    // -------------------- finish and reset state --------------------
    case RS_DONE: {
      if(ci[DEBUG] > 10) {
        Serial.println(F("RS_DONE"));
      }
      // clean up and go idle. clientGet should already be stopped in prior steps, but be safe:
      if(clientGet.connected()) clientGet.stop();
      remoteState = RS_IDLE;
      // keep other globals as-is (we updated them in processing)
      return;
    }
    default:
      remoteState = RS_IDLE;
      return;
  } // switch
}


String handleDeviceNameAndAdditionalSensors(char * sensorData, bool intialize){
  if(ci[POLLING_SKIP_LEVEL] < 11) {
    return "";
  }
  String additionalSensorArray[12];
  String deviceInfoValues[2];
  String specificSensorData[8];
  int i2c;
  int pinNumber;
  int powerPin;
  int sensorIdLocal;
  int sensorSubTypeLocal;
  int deviceFeatureId;
  int consolidateAllSensorsToOneRecord = 0;
  String out = "";
  String deviceInfo;
  int objectCursor = 0;
  int oldSensorId = -1;
  int ordinalOfOverwrite = 0;
  String sensorName;
  splitString(sensorData, '|', additionalSensorArray, 12);
  deviceInfo = additionalSensorArray[0].substring(1);
  splitString(deviceInfo, '*', deviceInfoValues, 2);
  deviceName = deviceInfoValues[0];
  architecture = deviceInfoValues[1];
  //Serial.println(deviceInfo);
  //Serial.println(architecture);
  
  requestNonJsonPinInfo = 1; //set this global
  for(int i=1; i<12; i++) {
    String sensorDatum = additionalSensorArray[i];
    if(sensorDatum.indexOf('*')>-1) {
      splitString(sensorDatum, '*', specificSensorData, 8);
      pinNumber = specificSensorData[0].toInt();
      powerPin = specificSensorData[1].toInt();
      sensorIdLocal = specificSensorData[2].toInt();
      sensorSubTypeLocal = specificSensorData[3].toInt();
      i2c = specificSensorData[4].toInt();
      deviceFeatureId = specificSensorData[5].toInt();
      sensorName = specificSensorData[6];
      ordinalOfOverwrite = specificSensorData[7].toInt();
      consolidateAllSensorsToOneRecord = specificSensorData[8].toInt();
      if(oldSensorId != sensorIdLocal) { //they're sorted by ci[SENSOR_ID], so the objectCursor needs to be set to zero if we're seeing the first of its type
        objectCursor = 0;
      }
      if(sensorIdLocal == ci[SENSOR_ID]) { //this particular additional sensor is the same type as the base (non-additional) sensor, so we have to pre-start it higher
        objectCursor++;
      }
      if(intialize) {
        startWeatherSensors(sensorIdLocal, sensorSubTypeLocal, i2c, pinNumber, powerPin); //guess i have to pass all this additional info
      } else {
        //otherwise do a weatherDataString
        out = out + "!" + weatherDataString(sensorIdLocal, sensorSubTypeLocal, pinNumber, powerPin, i2c, deviceFeatureId, objectCursor, sensorName, ordinalOfOverwrite, consolidateAllSensorsToOneRecord);
      }
      objectCursor++;
      oldSensorId = sensorIdLocal;
    }
  }
 return out;
}

//if the backend sends too much text data at once, it is likely to get gzipped, which is hard to deal with on a microcontroller with limited resources
//so a better strategy is to send double-delimited data instead of JSON, with data consistently in known ordinal positions
//thereby making the data payloads small enough that the server never gzips them
//i've made it so the ESP8266 can receive data in either format. it takes the lead on specifying which format it prefers
//but if it misbehaves, i can force it to be one format or the other remotely 
void setLocalHardwareToServerStateFromNonJson(char *nonJsonLine) {
    if (ci[POLLING_SKIP_LEVEL] < 15) {
        return;
    }
    static int limitCursor = 1;
    if (limitCursor > 11) {
        limitCursor = 1;
    }
    int pinNumber = 0;
    int value = -1;
    int canBeAnalog = 0;
    int serverSaved = 0;

    char friendlyPinName[50];
    char nonJsonPinArray[12][50];
    char nonJsonDatum[64];
    char nonJsonPinDatum[5][50];

    char pinIdCopy[50];
    char i2c = 0;

    splitStringToCharArrays(nonJsonLine, '|', nonJsonPinArray, 12);

    int beginLoop = 1;
    int endLoop = 12;

    if (ci[POLLING_SKIP_LEVEL] == 16) {
        beginLoop = limitCursor;
        endLoop = limitCursor + 1;
        limitCursor++;
    }

    int foundPins = 0;
    int initialPinMapSize = pinMap.size();
    for (int i = beginLoop; i < endLoop; i++) {
        yield();

        strncpy(nonJsonDatum, nonJsonPinArray[i], sizeof(nonJsonDatum) - 1);
        nonJsonDatum[sizeof(nonJsonDatum) - 1] = '\0';

        if (!strchr(nonJsonDatum, '*')) continue;

        splitStringToCharArrays(nonJsonDatum, '*', nonJsonPinDatum, 5);
        yield();

        // ----------------------------
        // SAFE COPY (DO NOT MUTATE ORIGINAL)
        // ----------------------------
        strncpy(pinIdCopy, nonJsonPinDatum[1], sizeof(pinIdCopy) - 1);
        pinIdCopy[sizeof(pinIdCopy) - 1] = '\0';

        char *dotPos = strchr(pinIdCopy, '.');
        if (dotPos) {
            *dotPos = '\0';
            i2c = atoi(pinIdCopy);
            pinNumber = atoi(dotPos + 1);
        } else {
            i2c = 0;
            pinNumber = atoi(pinIdCopy);
        }

        value = atoi(nonJsonPinDatum[2]);
        canBeAnalog = atoi(nonJsonPinDatum[3]);
        serverSaved = atoi(nonJsonPinDatum[4]);

        strncpy(friendlyPinName, nonJsonPinDatum[0], sizeof(friendlyPinName) - 1);
        friendlyPinName[sizeof(friendlyPinName) - 1] = '\0';

        // ----------------------------
        // IMPORTANT: KEEP ORIGINAL KEY EXACTLY AS BEFORE
        // ----------------------------
        char *rawKey = nonJsonPinDatum[1];

        pinName[foundPins] = friendlyPinName;
        pinList[foundPins] = rawKey;

        // ----------------------------
        // SAFE MAP ACCESS (NO operator[] INSERT SIDE EFFECTS)
        // ----------------------------
        int existingValue = -1;
        auto it = pinMap.find(rawKey);
        if (it != pinMap.end()) {
            existingValue = it->second;
        }

        if (!localSource || serverSaved == 1) {
            if (serverSaved == 1) {
                localSource = false;
            } else {
              /*
                Serial.print("[");
                Serial.print(nonJsonPinDatum[1]);
                Serial.println("]");
                Serial.print(pinMap.size());
                Serial.print(" ");
                */
                auto it = pinMap.find(rawKey);
               
                
                pinMap.erase(rawKey);
                if(strlen(rawKey) < 8 && (it != pinMap.end()  || initialPinMapSize==0)) { //only insert if initialPinMapSize is 0 or pinMap key was found
                  pinMap.insert({rawKey, value});
                }
                //Serial.println(pinMap.size());
            }
        }

        // ----------------------------
        // OUTPUT SIDE EFFECTS (UNCHANGED LOGIC)
        // ----------------------------
        if (existingValue != value || resendSlavePinInfo) {
            if (i2c > 0) {
                setPinValueOnSlave(i2c, (char)pinNumber, (char)value);
            } else {
                pinMode(pinNumber, OUTPUT);
                if (canBeAnalog) {
                    analogWrite(pinNumber, value);
                } else {
                    digitalWrite(pinNumber, value > 0 ? HIGH : LOW);
                }
            }
        }

        foundPins++;
    }

    resendSlavePinInfo = false;
    pinTotal = foundPins;
}


//this will set any pins specified in the JSON
//but we don't do this any more
/*
void setLocalHardwareToServerStateFromJson(char * json){
  if(millis() - localChangeTime < 1000) { //don't accept any server values withing 5 seconds of a local change
    return;
  }
  char * nodeName="device_data";
  int pinNumber = 0;
  int value = -1;
  int canBeAnalog = 0;
  int enabled = 0;
  int pinCounter = 0;
  int serverSaved = 0;
  String friendlyPinName = "";
  char i2c = 0;
  DeserializationError error = deserializeJson(jsonBuffer, json);
  if(jsonBuffer["device"]) { //deviceName is a global
    deviceName = (String)jsonBuffer["device"];
    Serial.println("DEVICE: " + deviceName);
    //once we have deviceName, we can get data this way:
    requestNonJsonPinInfo = 1;
  }
  if(jsonBuffer[nodeName]) {
    pinCounter = 0;
    if(!onePinAtATimeMode) {
      pinMap->clear(); //this won't work
    }
    for(int i=0; i<jsonBuffer[nodeName].size(); i++) {
      friendlyPinName = (String)jsonBuffer[nodeName][i]["name"];
      pinNumber = (int)jsonBuffer[nodeName][i]["pin_number"];
      value = (int)jsonBuffer[nodeName][i]["value"];
      canBeAnalog = (int)jsonBuffer["nodeName"][i]["can_be_analog"];
      enabled = (int)jsonBuffer[nodeName][i]["enabled"];
      serverSaved = (int)jsonBuffer[nodeName][i]["ss"];
      i2c = (int)jsonBuffer[nodeName][i]["i2c"];
      if(i2c > 0) {
        //i2c = 0;
      }
      Serial.print("json pin: ");
      Serial.print(pinNumber);
      Serial.print("; value: ");
      Serial.print(value);
      Serial.print("; i2c: ");
      Serial.print((int)i2c);
      Serial.println();
      pinMode(pinNumber, OUTPUT);
      if(enabled) {
        for(char j=0; j<pinTotal; j++){
          String key;
          char sprintBuffer[6];
          sprintf(sprintBuffer, "%d.%d", i2c, pinNumber);
          key = (String)sprintBuffer;
          if(i2c < 1){
            key = (String)pinNumber;
          }
          //Serial.println("! " + (String)pinList[j] +  " =?: " + key +  " correcto? " + (int((String)pinList[j] == key)));
          if(!localSource || serverSaved == 1){
            if((String)pinList[j] == key) {
              if(serverSaved == 1) {//confirmation of serverSaved, so localSource flag is no longer needed
                Serial.println("SERVER SAVED==1!!");
                localSource = false;
              } else { //this will have the wrong value if serverSaved == 1
                pinMap->remove(key);
                pinMap->put(key, value);
              }
              pinName[j] = friendlyPinName;
            }
          }
        }
        if(i2c > 0) {
          setPinValueOnSlave(i2c, (char)pinNumber, (char)value); 
        } else {
          if(canBeAnalog) {
            analogWrite(pinNumber, value);
          } else {
            if(value > 0) {
              digitalWrite(pinNumber, HIGH);
            } else {
              digitalWrite(pinNumber, LOW);
            }
          }
        }
      }
      pinCounter++;
    }
  }
  nodeName="pin_list";
  String pinString;
  if(jsonBuffer[nodeName]) {
    pinCounter = 0;
    for(int i=0; i<jsonBuffer[nodeName].size(); i++) {
      pinString = (String)jsonBuffer[nodeName][i];
      pinList[pinCounter] = (String)pinString;
      pinCounter++;
    }
  }
  pinTotal = pinCounter;
}
*/

long getPinValueOnSlave(char i2cAddress, char pinNumber) { //might want a user-friendlier API here
  //reading an analog or digital value from the slave:
  Wire.beginTransmission(i2cAddress);
  Wire.write(pinNumber); //addresses greater than 64 are the same as AX (AnalogX) where X is 64-value
  Wire.endTransmission();
  delay(10); 
  Wire.requestFrom(i2cAddress, 4); //we only ever get back four-byte long ints
  long totalValue = 0;
  int byteCursor = 1;
  while (Wire.available()) {
    byte receivedValue = Wire.read(); // Read the received value from slave
    totalValue = totalValue + receivedValue * pow(256, 4-byteCursor);
    //Serial.println(receivedValue); // Print the received value
    byteCursor++;
  }
  return totalValue;
}

void setPinValueOnSlave(char i2cAddress, char pinNumber, char pinValue) {
  //if you have a slave Arduino set up with this code:
  //https://github.com/judasgutenberg/Generic_Arduino_I2C_Slave
  //and a device_type_feature specifies an i2c address
  //then this code will send the data to that slave Arduino
  /*
  Serial.print((int)i2cAddress);
  Serial.print(" ");
  Serial.print((int)pinNumber);
  Serial.print(" ");
  Serial.print((int)pinValue);
  Serial.println("");
  */
  Wire.beginTransmission(i2cAddress);
  Wire.write(pinNumber);
  Wire.write(pinValue);
  Wire.endTransmission();
  yield();
}

//i don't want to use JSON because the format is too bulky:
/*
//this will run commands sent to the server
//still needs to be implemented on the backend. but if i need it, it's here
void runCommandsFromJson(char * json){
  String command;
  int commandId;
  char * nodeName="commands";
  DeserializationError error = deserializeJson(jsonBuffer, json);
  if(jsonBuffer[nodeName]) {
    Serial.print("number of commands: ");
    Serial.print(jsonBuffer[nodeName].size());
    Serial.println();
    Serial.println();
    for(int i=0; i<jsonBuffer[nodeName].size(); i++) {
      command = (String)jsonBuffer[nodeName][i]["command"];
      commandId = (int)jsonBuffer[nodeName][i]["commandId"];
      //still have to run command!
      if(command == "reboot") {
        rebootEsp();
      } else if(command == "allpinsatonce") {
        onePinAtATimeMode = 0;
      }
      lastCommandId = commandId;
    }
  }
}
*/

///////////////////////////////////////////////
//command functions
///////////////////////////////////////////////
#define CFG_REQ_FS        0b00000001
#define CFG_REQ_SLAVE     0b00000010
#define CFG_REQ_FRAM      0b00000100
#define CFG_REQ_RTC       0b00001000
///////////////////////////////
#define CFG_REQ_IR        0b00010000
#define CFG_UNUSED_1      0b00100000
#define CFG_UNUSED_2      0b01000000
#define CFG_REQ_DEFER     0b10000000
  
void runCommand(const char * commandText, bool deferred){
  //can change the default values of some config data for things like polling
  //dumpMemoryStats(99);
  //return;
  String command;
  int commandId;
  String commandData;
  String commandArray[4];
  int latency;
  //first get rid of the first character, since all it does is signal that we are receiving a command:
  const char* cmd = commandText + 1;
  splitString(cmd, '|', commandArray, 3);
  commandId = commandArray[0].toInt();
  command = commandArray[1];
  commandData = commandArray[2];
  latencyCount++;
  latency = commandArray[3].toInt();
  latencySum += latency;
  //Serial.println(commandId);
  if(commandId == -2) {
    outputMode = 2;
  }
  String* results[4];
  int resultCount;
  command.trim();
  if(commandId) {
    //Serial.println(command);
    handleCommand(command, deferred);
    if(!deferred && (commandRequiresDeferment(command))) {
      //Serial.println("--------+"  + command + "*-----");
      notYetDeferred(commandText, commandId, (int32_t)(command.startsWith(F("update firmware"))));
    }
    command = "";
    if(commandId > 0) { //don't reset lastCommandId if the command came via serial port
      lastCommandId = commandId;
    }
  }
}

void handleCommand(String input, bool deferred) {
  String results[5];
  int resultCount = 0;
  if (input == "") {
    return;
  }
  int commandNumber = sizeof(commands) / sizeof(commands[0]);
  for (int i = 0; i < commandNumber; i++) {
    yield();
    if (parseCommand(input, commands[i].name, results, resultCount, commands[i].maxArgs)) {
      uint8_t cfg = commands[i].configuration;
      // 🚫 Capability checks
      if ((cfg & CFG_REQ_RTC) && !(ci[RTC_ADDRESS] > 0)) {
        textOut(F("Error: RTC capability required\n"));
        return;
      }
      if ((cfg & CFG_REQ_FRAM) && !(ci[FRAM_ADDRESS] > 0)) {
        textOut(F("Error: FRAM capability required\n"));
        return;
      }
      if ((cfg & CFG_REQ_SLAVE) && !(ci[SLAVE_I2C] > 0)) {
        textOut(F("Error: Slave capability required\n"));
        return;
      }
      if ((cfg & CFG_REQ_IR) && !(ci[IR_PIN] > -1)) {
        textOut(F("Error: IR capability required\n"));
        return;
      }
      /*
      // ⏳ Deferred requirement check (if you want it enforced)
      if ((cfg & CFG_REQ_DEFER) && !deferred) {
        textOut(F("Error: Command must be deferred\n"));
        return;
      }
      */
      // ✅ All checks passed, execute
      commands[i].handler(results, resultCount, deferred);
      return;
    }
  }
  textOut(F("Command '") + input + F("' does not exist\n"));
}

void notYetDeferred(const char * commandText, int commandId, int commandType){
  if(lastCommandLogId > 0 || commandId <0) {
    saveCommandState(lastCommandLogId, VERSION, commandId, commandType);
  }
  String command;
  if (commandText == nullptr) {
    return;
  }
  size_t len = strlen(commandText);
  if (deferredCommand != nullptr) {
    delete[] deferredCommand;
  }
  deferredCommand = new char[len + 1];  // +1 for null terminator
  strcpy(deferredCommand, commandText);
  if(commandId == -1) {
    //our command is via serial, so handle deferred commands immediately
    runCommand(deferredCommand, true);
  }
  return;
}

bool parseCommand(const String& input, const String& command, String* results, int& resultCount, int maxResults) {
    resultCount = 0;
    String s = input;
    s.trim();
    // Check if it starts with the command
    if (!s.startsWith(command)) {
        return false;
    }
    // Remove the command part
    s = s.substring(command.length());
    s.trim();
    bool inQuotes = false;
    String current = "";
    for (unsigned int i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '"') {
            inQuotes = !inQuotes;
            continue; // don't include the quote
        }
        if (c == ' ' && !inQuotes) {
            if (current.length() > 0) {
                if (resultCount < maxResults) {
                    results[resultCount++] = current;
                }
                current = "";
            }
        } else {
            current += c;
        }
    }
    // Add last token
    if (current.length() > 0 && resultCount < maxResults) {
        results[resultCount++] = current;
    }
    return true;
}

bool commandRequiresDeferment(String input) {
  String results[5];
  int resultCount = 0;
  if (input == "") {
    return false;
  }
  int commandNumber = sizeof(commands) / sizeof(commands[0]);
  for (int i = 0; i < commandNumber; i++) {
    yield();

    if (parseCommand(input, commands[i].name, results, resultCount, commands[i].maxArgs)) {
      return (commands[i].configuration & CFG_REQ_DEFER) != 0;
    }
  }
  // Command not found → no deferment requirement (you could argue either way)
  return false;
}

void saveCommandState(uint32_t lastCommandLogId, uint16_t version, int32_t commandId, int32_t commandType) {
    File file = LittleFS.open(F("/commandstate.txt"), "w");
    if (!file) {
        // optional: Serial.println("Failed to open file for writing");
        return;
    }
    file.println(lastCommandLogId);
    file.println(version);
    file.println(commandId);
    file.println(commandType);
    file.close();
}

int32_t loadCommandStateVersion(int dataToReturn) {
    File file = LittleFS.open("/commandstate.txt", "r");
    if (!file) {
        // optional: Serial.println("Failed to open file for reading");
        return 0;
    }
    int lineCount = 4;
    int32_t lines[4];
    for(int lineNumber = 0; lineNumber<lineCount; lineNumber++){
      lines[lineNumber] = (int32_t)file.readStringUntil('\n').toInt();
    }
    return lines[dataToReturn];
}

///////////////////////////////////////////////
//end of command functions
///////////////////////////////////////////////


void sendIr(String rawDataStr) {
  irsend.begin();
  //Example input string
  //rawDataStr = "500,1500,500,1500,1000";
  size_t rawDataLength = 0;

  // Count commas to determine array length
  for (size_t i = 0; i < rawDataStr.length(); i++) {
    if (rawDataStr[i] == ',') rawDataLength++;
  }
  rawDataLength++; // Account for the last value
  // Allocate array
  uint16_t* rawData = (uint16_t*)malloc(rawDataLength * sizeof(uint16_t));

  // Parse the string into the array
  size_t index = 0;
  int start = 0;
  for (size_t i = 0; i <= rawDataStr.length(); i++) {
    if (rawDataStr[i] == ',' || rawDataStr[i] == '\0') {
      rawData[index++] = rawDataStr.substring(start, i).toInt();
      start = i + 1;
    }
  }
  // Send the parsed raw data
  irsend.sendRaw(rawData, rawDataLength, 38);
  if(ci[DEBUG] > 1) {
    Serial.println(F("IR signal sent!"));
  }
  free(rawData); // Free memory
}

void rebootEsp() {
  textOut(F("Rebooting ESP\n"));
  ESP.restart();
}

void rebootMoxee() {  //moxee hotspot is so stupid that it has no watchdog.  so here i have a little algorithm to reboot it.
  if(ci[MOXEE_POWER_SWITCH] > -1) {
    digitalWrite(ci[MOXEE_POWER_SWITCH], LOW);
    delay(7000);
    digitalWrite(ci[MOXEE_POWER_SWITCH], HIGH);
  }
  delay(5000);
  if(knownMoxeePhase == 0) {
    knownMoxeePhase = 1;
  } else if (knownMoxeePhase == 1) {
    knownMoxeePhase = 0;
  }
  moxeePhaseChangeCount++;

  if(moxeePhaseChangeCount > 12) { 
    //if it's been more than twelve phase changes since the last connection, 
    //then we really don't know what phase we are in, so back to uncertain
    knownMoxeePhase = -1;
  }
  //only do one reboot!  it usually takes two, but this thing can be made to cycle so fast that this same function can handle both reboots, which is important if the reboot happens to 
  //be out of phase with the cellular hotspot
  shiftArrayUp(moxeeRebootTimes,  timeClient.getEpochTime(), 10);
  moxeeRebootCount++;
  if(moxeeRebootCount > 9) { //don't bother with offlineode if we cat log data  // && ci[FRAM_ADDRESS] > 0
    offlineMode = true;
    moxeeRebootCount = 0;
  } else if(moxeeRebootCount > ci[NUMBER_OF_HOTSPOT_REBOOTS_OVER_LIMITED_TIMEFRAME_BEFORE_ESP_REBOOT]) { //kind of a watchdog
    rebootEsp();
  }
}

//takes "12,14,1,2,6" and returns an array of those ints
int splitAndParseInts(const char* input, int* outputArray, int maxCount) {
  int count = 0;
  char buffer[128]; // adjust to max expected string length
  strncpy(buffer, input, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0'; // ensure null termination

  char* token = strtok(buffer, ",");
  while (token != nullptr && count < maxCount) {
    outputArray[count++] = atoi(token);
    token = strtok(nullptr, ",");
  }

  return count;
}

//SETUP----------------------------------------------------
void setup(){
  Wire.begin();
  yield();
  //Serial.begin(115200);    
  //setSerialRate((byte)ci[BAUD_RATE_LEVEL]); 
  //Serial.setRxBufferSize(1024);
  //Serial.setDebugOutput(true);
  delay(10);
  initMasterDefaults();
  setSerialRate((byte)ci[BAUD_RATE_LEVEL]); 
  if(loadAllConfig(0, 0) != 1) {
    if(ci[DEBUG] > 0) {
      Serial.println(F("\nNo config found in storage"));
    }
    initMasterDefaults();
  } else {
    if(ci[DEBUG] > 0) {
      Serial.print(F("\nConfiguration retrieved from "));
      if(ci[CONFIG_PERSIST_METHOD] == 1) {
        Serial.println(F("slave EEPROM"));
      } else {
        Serial.println(F("local flash"));
      }
    }
  }
  //set specified pins to start low immediately, keeping devices from turning on
  int pinsToStartLow[10];
  int numToStartLow = splitAndParseInts(cs[PINS_TO_START_LOW], pinsToStartLow, 10);
  for(int i=0; i<numToStartLow; i++) {
    if((int)pinsToStartLow[i] == -1) {
      break;
    }
    pinMode((int)pinsToStartLow[i], OUTPUT);
    digitalWrite(pinsToStartLow[i], LOW);
  }
  if(ci[MOXEE_POWER_SWITCH] > -1) {
    pinMode(ci[MOXEE_POWER_SWITCH], OUTPUT);
    digitalWrite(ci[MOXEE_POWER_SWITCH], HIGH);
  }
  
  //Serial.setRxBufferSize(256);  
  Serial.setDebugOutput(false);
  textOut("\n\nJust started up...\n");
 
  
  //Wire.setClock(50000); // Set I2C speed to 100kHz (default is 400kHz)
  //Wire.setClock(100000);
  startWeatherSensors(ci[SENSOR_ID],  ci[SENSOR_SUB_TYPE], ci[SENSOR_I2C], ci[SENSOR_DATA_PIN], ci[SENSOR_POWER_PIN]);
  wiFiConnect();
  server.on("/", handleRoot);      //Displays a form where devices can be turned on and off and the outputs of sensors
  server.on("/readLocalData", localShowData);
  server.on("/weatherdata", handleWeatherData);
  server.on("/writeLocalData", localSetData);
  server.onNotFound(handleFileRequest);
  server.begin(); 
  textOut("HTTP server started\n");
  
  //initialize NTP client
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(0);
  if(ci[INA219_ADDRESS] > 0) {
    ina219 = new Adafruit_INA219(ci[INA219_ADDRESS]);
    if (!ina219->begin()) {
      if(ci[DEBUG] > 0) {
        Serial.println(F("Failed to find INA219 chip"));
      }
    } else {
      ina219->setCalibration_16V_400mA();
    }
  }
  if(ci[FRAM_ADDRESS] > 0) {
    if (!fram.begin(ci[FRAM_ADDRESS])) {
      if(ci[DEBUG] > 0) {
        Serial.println(F("Could not find FRAM (or EEPROM)."));
      }
    } else {
      if(ci[DEBUG] > 0) {
        Serial.println(F("FRAM or EEPROM found"));
      }
    }
      currentRecordCount = readRecordCountFromFRAM();
      if(lastRecordSize == 0) {
        lastRecordSize = getRecordSizeFromFRAM(0xFFFF);
      }
  }
  if(ci[IR_PIN] > -1) {
    //irsend.begin(); //do this elsewhere?
  }
  if (!LittleFS.begin()) {
    if(ci[DEBUG] > 0) {
      Serial.println(F("LittleFS mount failed!"));
    }
  }
  //clearFramLog();
  //displayAllFramRecords();
  //do an initial pet of the watchdog just to set its unix time and make sure it does not bite us

}

void flashUpdateFeedback(uint32_t nowTime) {
  //this function does nothing but give me feedback and the end of a reboot caused by a flash update
  int32_t preRebootCommandId = loadCommandStateVersion(0);
  uint32_t oldVersion = loadCommandStateVersion(1);
  int32_t oldCommandId = loadCommandStateVersion(2);
  int32_t commandType = loadCommandStateVersion(3);
  String versionMessage;
  versionMessage.reserve(80);  // preallocate once
  versionMessage = String(F("After reboot: version: ")) + VERSION  + "\n";
  if(oldVersion > 0) {
    versionMessage = String(F("Update of firmware was successful; version ") + String(oldVersion) + F(" changed to version ")) + VERSION + "\n";
  }
  if((preRebootCommandId > 0 || oldCommandId < 0) && commandType > 0 && lastCommandLogId == 0  && deviceName != "" && remoteState == RS_IDLE) {
    //Serial.println("alpha");
    //we rebooted and came up from it, so let's cap that command with something:
    if(millisAtPossibleReboot < nowTime  && millisAtPossibleReboot > 0 && possibleEndingMessage != "") { 
      //Serial.println("beta");
      //we didn't actually reboot!
      //though i don't know if this scenario ever happens
      String versionMessage = possibleEndingMessage + "\n";
      if(oldCommandId == -1) {
        textOut(versionMessage);
      } else {
        startRemoteTask(versionMessage + preRebootCommandId, "commandout", 0xFFFF);
      }
    } else {
      //Serial.println("gamma");
      String commandToSend = versionMessage + preRebootCommandId;
      if(oldCommandId == -1) {
        textOut(versionMessage);
      } else {
        startRemoteTask(commandToSend, "commandout", 0xFFFF);
      }
      //setSlaveLong(0,0);
    }
    saveCommandState(0, 0, 0, 0);
  } else if (preRebootCommandId > 0  && deviceName != "" && remoteState == RS_IDLE) {
    //Serial.println("delta");
    if(possibleEndingMessage == "") {
      possibleEndingMessage = versionMessage;
    }
    String stringToSend = possibleEndingMessage + "\n" + preRebootCommandId;
    startRemoteTask(stringToSend, "commandout", 0xFFFF);
    saveCommandState(0, 0, 0, 0);
  }
}

//LOOP----------------------------------------------------
void loop(){
  yield();
  doSerialCommands();
  yield();
  runRemoteTask();
  //runSlaveUpdater();
  yield();
  //Serial.println("");
  //Serial.print("KNOWN MOXEE PHASE: ");
  //Serial.println(knownMoxeePhase);
  //Serial.print("Last command log id: ");
  //Serial.println(lastCommandLogId);
  unsigned long nowTime = millis() + timeOffset;
  yield();
  /*
  uint8_t testHi = 0;//(millis() >> 8) & 0xFF;
  uint8_t testLow = 4;// millis() & 0xFF;
  noInterrupts();
  delay(5);
  fram.write(28674, &testHi, 1);  // Write test byte to address 1000
  fram.write(28674+1, &testLow, 1);  // Write test byte to address 1000
  interrupts();
  uint8_t readBack;
  Serial.print(" Original: ");
  Serial.print(testHi, HEX);
  Serial.println(testLow, HEX);
  Serial.print(" Read back: ");
  fram.read(28674, &readBack, 1);  // Read it back
  Serial.print(readBack, HEX);
  fram.read(28674 +1, &readBack, 1);  // Read it back
  Serial.println(readBack, HEX);
  */
  //displayFramRecord(147);
  yield();
  if(!offlineMode) {
    for(int i=0; i < ci[LOCAL_WEB_SERVICE_RESPONSIVENESS]; i++) { //doing this four times here is helpful to make web service reasonably responsive. once is not enough
      server.handleClient();          //Handle client requests
      yield();
    }
  }
  //dumpMemoryStats(122);
  //was doing an experiment:
  if(nowTime - wifiOnTime > 20000) {
    //WiFi.disconnect(true);
  }
  cleanup();
  if(lastCommandLogId > 0 || responseBuffer != "") {
    String stringToSend = responseBuffer + "\n" + lastCommandLogId;
    startRemoteTask(stringToSend, "commandout", 0xFFFF);
  }
  timeClient.update();
  yield();
  int granularityToUse = ci[POLLING_GRANULARITY];
  if(connectionFailureMode) {
    if(knownMoxeePhase == 0) {
      granularityToUse = ci[GRANULARITY_WHEN_IN_MOXEE_PHASE_0];// used to be granularity_when_in_connection_failure_mode;
    } else { //if unknown or operational, let's go slowly!
      granularityToUse = ci[GRANULARITY_WHEN_IN_MOXEE_PHASE_1];
    }
  }
  //if we've been up for a week or there have been lots of moxee reboots in a short period of time, reboot esp8266
  if(nowTime > 1000 * 86400 * 7 || nowTime < ci[HOTSPOT_LIMITED_TIME_FRAME] * 1000  && moxeeRebootCount >= ci[NUMBER_OF_HOTSPOT_REBOOTS_OVER_LIMITED_TIMEFRAME_BEFORE_ESP_REBOOT]) {
    //let's not do this anymore
    //Serial.print("MOXEE REBOOT COUNT: ");
    //Serial.print(moxeeRebootCount);
    //Serial.println();
    //rebootEsp();
  }
  //Serial.print(granularityToUse);
  //Serial.print(" ");
  //Serial.println(connectionFailureTime);
  yield();
  if(nowTime < granularityToUse * 1000 || (nowTime - lastPoll)/1000 > granularityToUse || connectionFailureTime>0 && connectionFailureTime + ci[CONNECTION_FAILURE_RETRY_SECONDS] * 1000 > nowTime) {  //send data to backend server every <ci[POLLING_GRANULARITY]> seconds or so
    //Serial.println(granularityToUse);
    //Serial.print("Connection failure time: ");
    //Serial.println(connectionFailureTime);
    //Serial.print("  Connection failure calculation: ");
    //Serial.print(connectionFailureTime>0 && connectionFailureTime + ci[CONNECTION_FAILURE_RETRY_SECONDS] * 1000);
    //Serial.println("Epoch time:");
    //Serial.println(timeClient.getEpochTime());
    //compileAndSendDeviceData(String weatherdata, String wherewhen, String powerdata, bool doPinCursorChanges, uint16_t fRAMOrdinal)
    yield();
    if(ci[DEBUG] > 20) {
      Serial.println(F("about to compileAndSendDeviceData"));
    }
    compileAndSendDeviceData("", "", "", true, 0xFFFF);
    lastPoll = nowTime;
  }
  yield();
  
  lookupLocalPowerData();
  yield();
  if(offlineMode || ci[FRAM_ADDRESS] < 1){
    if(nowTime - lastOfflineReconnectAttemptTime > 1000 * ci[OFFLINE_RECONNECT_INTERVAL]) {
      //time to attempt a reconnection
     if(WiFi.status() != WL_CONNECTED) {
      wiFiConnect();
     }
      
    }
  } else {
    if(ci[RTC_ADDRESS] > 0) { //if we have an RTC and we are not offline, sync RTC
      if (nowTime - lastRtcSyncTime > 86400000 || lastRtcSyncTime == 0) {
        syncRTCWithNTP();
        lastRtcSyncTime = nowTime;
      }
    }
  }
  if(ci[SLAVE_PET_WATCHDOG_COMMAND] > 0) {
    unsigned long timeoutSeconds = 1;
    for (uint8_t i = 200; i < ci[SLAVE_PET_WATCHDOG_COMMAND]; i++) {
      timeoutSeconds *= 10;
    }
    uint32_t unixTime = timeClient.getEpochTime();
    if ((nowTime - lastPet > timeoutSeconds * 900UL) || (unixTime > 1000 && slaveUnpetted)) { //pet every 90% of the time that will lead to a bite
      petWatchDog((uint8_t)ci[SLAVE_PET_WATCHDOG_COMMAND], unixTime);
      yield();
      slaveUnpetted = false;
      //feedbackPrint("pet\n");
    }
  }
  yield();
  
  if(haveReconnected) {
    //try to send stored records to the backend
    if(ci[FRAM_ADDRESS] > 0){
      //dumpMemoryStats(101);
      sendAStoredRecordToBackend();
    } else {
      haveReconnected = false;
    }
  }
  yield();
  if(ci[DEBUG] > 20) {
    Serial.println(F("about to flashUpdateFeedback"));
  }
  flashUpdateFeedback(nowTime);
  if(canSleep) {
    //this will only work if GPIO16 and EXT_RSTB are wired together. see https://www.electronicshub.org/esp8266-deep-sleep-mode/
    if(ci[DEEP_SLEEP_TIME_PER_LOOP] > 0) {
      textOut("sleeping...\n");
      ESP.deepSleep(ci[DEEP_SLEEP_TIME_PER_LOOP] * 1e6); 
    }
    //this will only work if GPIO16 and EXT_RSTB are wired together. see https://www.electronicshub.org/esp8266-deep-sleep-mode/
    if(ci[LIGHT_SLEEP_TIME_PER_LOOP] > 0) {
      textOut("snoozing...\n");
      sleepForSeconds(ci[LIGHT_SLEEP_TIME_PER_LOOP]);
      textOut("awakening...\n");
      wiFiConnect();
    }
  }
}

void doSerialCommands() {
  static String command;
  yield();
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (command.length() > 0) {
        if (ci[DEBUG] > 0) {
          Serial.print(F("Serial command: "));
          Serial.println(command);
        }
        String fullCommand = "!-1|" + command;
        runCommand(fullCommand.c_str(), false);
        command = "";   // reset AFTER parsing
        return;
      }
    } else {
      command += c;
    }
  }
}

void sleepForSeconds(int seconds) {
    wifi_set_opmode(NULL_MODE);            // Turn off Wi-Fi for lower power
    wifi_set_sleep_type(LIGHT_SLEEP_T);    // Enable Light Sleep Mode

    uint32_t sleepEndTime = millis() + seconds * 1000;
    while (millis() < sleepEndTime) {
        delay(10); // Short delays allow CPU to periodically enter light sleep
    }
    // GPIO states are preserved during this periodd
}

//this is the easiest way I could find to read querystring parameters on an ESP8266. ChatGPT was suprisingly unhelpful
void localSetData() {
  localChangeTime = millis();
  String id = "";
  int onValue = 0;
  for (int i = 0; i < server.args(); i++) {
    if(server.argName(i) == "id") {
      id = server.arg(i);
      textOut(id);
      textOut( " : ");
    } else if (server.argName(i) == "on") {
      onValue = (int)(server.arg(i) == "1");  
    } else if (server.argName(i) == "ipaddress") {
      ipAddressAffectingChange = (String)server.arg(i);  
    }
    textOut(String(onValue));
    textOut("\n");
  } 
  for (int i = 0; i < pinMap.size(); i++) {
    String key = pinList[i];
    textOut(key);
    textOut(" ?= ");
    textOut(String(id) + "\n");
    if(key == id) {
      pinMap.erase(key);
      pinMap[key] = onValue;
      if(ci[DEBUG] > 0) {
        Serial.print(F("LOCAL SOURCE TRUE :"));
        Serial.println(onValue);
      }
      localSource = true; //sets the NodeMCU into a mode it cannot get out of until the server sends back confirmation it got the data
    }
  }
  server.send(200, "text/plain", "Data received");
}

void localShowData() {
  if(millis() - localChangeTime < 1000) {
    return;
  }
  String out = "{\"device\":\"" + deviceName + "\", \"pins\": [";
  for (int i = 0; i < pinMap.size(); i++) {
    out = out + "{\"id\": \"" + pinList[i] +  "\",\"name\": \"" + pinName[i] +  "\", \"value\": \"" + (String)pinMap[pinList[i]] + "\"}";
    if(i < pinMap.size()-1) {
      out = out + ", ";
    }
  }
  out += "]}";
  server.send(200, "text/plain", out); 
}

void printHexBytes(uint32_t value) {
    textOut(String(((value >> 24) & 0xFF), HEX));
    textOut(String(((value >> 16) & 0xFF), HEX));
    textOut(String(((value >> 8) & 0xFF), HEX));
    textOut(String((value & 0xFF), HEX));
}

byte decToBcd(byte val){
  return ( (val/10*16) + (val%10) );
}

// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val){
  return ( (val/16*10) + (val%16) );
}


//RTC functions
const int _ytab[2][12] = {
{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

#define DAYSPERWEEK (7)
#define DAYSPERNORMYEAR (365U)
#define DAYSPERLEAPYEAR (366U)
#define SECSPERDAY (86400UL) /* == ( 24 * 60 * 60) */
#define SECSPERHOUR (3600UL) /* == ( 60 * 60) */
#define SECSPERMIN (60UL) /* == ( 60) */
#define LEAPYEAR(year)          (!((year) % 4) && (((year) % 100) || !((year) % 400)))

void syncRTCWithNTP() {
  timeClient.update();
  time_t rawTime = timeClient.getEpochTime();
  struct tm *timeInfo = gmtime(&rawTime);  // Convert to UTC struct
  byte second      = timeInfo->tm_sec;
  byte minute      = timeInfo->tm_min;
  byte hour        = timeInfo->tm_hour;
  byte dayOfWeek   = timeInfo->tm_wday == 0 ? 7 : timeInfo->tm_wday; // Convert Sunday (0) to 7
  byte dayOfMonth  = timeInfo->tm_mday;
  byte month       = timeInfo->tm_mon + 1; // tm_mon is 0-based
  byte year        = timeInfo->tm_year - 100; // Convert from years since 1900
  char buffer[120];
  sprintf(buffer, "Setting RTC: %02d:%02d:%02d %02d/%02d/%02d\n", hour, minute, second,  month,  dayOfMonth, year + 2000);
  textOut(String(buffer));
  setDateDs1307(second, minute, hour, dayOfWeek, dayOfMonth, month, year);
}

/****************************************************
* Class:Function    : getSecsSinceEpoch
* Input     : uint16_t epoch date (ie, 1970)
* Input     : uint8 ptr to returned month
* Input     : uint8 ptr to returned day
* Input     : uint8 ptr to returned years since Epoch
* Input     : uint8 ptr to returned hour
* Input     : uint8 ptr to returned minute
* Input     : uint8 ptr to returned seconds
* Output        : uint32_t Seconds between Epoch year and timestamp
* Behavior      :
*
* Converts MM/DD/YY HH:MM:SS to actual seconds since epoch.
* Epoch year is assumed at Jan 1, 00:00:01am.
* looks like epoch has to be a date like 1900 or 2000 with two zeros at the end.  1984 will not work!
****************************************************/
uint32_t getSecsSinceEpoch(uint16_t epoch, uint8_t month, uint8_t day, uint8_t years, uint8_t hour, uint8_t minute, uint8_t second){
  uint32_t secs = 0;
  int countleap = 0;
  int i;
  int dayspermonth;
  secs = years  * (SECSPERDAY * 365);
  if(epoch == 1970) {
    secs = secs + 946684800;
  }
  for (i = 0; i < (years - 1); i++){   
      if (LEAPYEAR((epoch + i)))
        countleap++;
  }
  secs += (countleap * SECSPERDAY);
  secs += second;
  secs += (hour * SECSPERHOUR);
  secs += (minute * SECSPERMIN);
  secs += ((day - 1) * SECSPERDAY);
  if (month > 1){
      dayspermonth = 0;
      if (LEAPYEAR((epoch + years))){ // Only counts when we're on leap day or past it
          if (month > 2){
              dayspermonth = 1;
          } else if (month == 2 && day >= 29) {
              dayspermonth = 1;
          }
      }
      for (i = 0; i < month - 1; i++){   
          secs += (_ytab[dayspermonth][i] * SECSPERDAY);
      }
  }
  return secs;
}

// 1) Sets the date and time on the ds1307
// 2) Starts the clock
// 3) Sets hour mode to 24 hour clock
// Assumes you're passing in valid numbers
void setDateDs1307(byte second,        // 0-59
                   byte minute,        // 0-59
                   byte hour,          // 1-23
                   byte dayOfWeek,     // 1-7
                   byte dayOfMonth,    // 1-28/29/30/31
                   byte month,         // 1-12
                   byte year)          // 0-99
{
  /*
  Serial.print(year);
  Serial.print("-");
  Serial.print(month);
  Serial.print("-");
  Serial.print(dayOfMonth);
  Serial.print(" ");
  
  Serial.print(hour);
  Serial.print(":");
  Serial.print(minute);
  Serial.print(":");
  Serial.print(second);
  Serial.print("/");
  Serial.print(dayOfWeek);
  Serial.println(" ");
   */
   Wire.beginTransmission(ci[RTC_ADDRESS]);
   Wire.write(0);
   Wire.write(decToBcd(second));    // 0 to bit 7 starts the clock
   Wire.write(decToBcd(minute));
   Wire.write(decToBcd(hour));      // If you want 12 hour am/pm you need to set
                                   // bit 6 (also need to change readDateDs1307)
   Wire.write(decToBcd(dayOfWeek));
   Wire.write(decToBcd(dayOfMonth));
   Wire.write(decToBcd(month));
   Wire.write(decToBcd(year));
   Wire.endTransmission();
}

// Gets the date and time from the ds1307
void getDateDs1307(byte *second,
          byte *minute,
          byte *hour,
          byte *dayOfWeek,
          byte *dayOfMonth,
          byte *month,
          byte *year){
  // Reset the register pointer
  Wire.beginTransmission(ci[RTC_ADDRESS]);
  Wire.write(0);
  Wire.endTransmission();
  
  Wire.requestFrom(ci[RTC_ADDRESS], 7);

  // A few of these need masks because certain bits are control bits
  *second     = bcdToDec(Wire.read() & 0x7f);
  *minute     = bcdToDec(Wire.read());
  *hour       = bcdToDec(Wire.read() & 0x3f);  // Need to change this if 12 hour am/pm
  *dayOfWeek  = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month      = bcdToDec(Wire.read());
  *year       = bcdToDec(Wire.read());
}

void printRTCDate(){
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  textOut(String(year));
  textOut("-");
  textOut(String(month));
  textOut("-");
  textOut(String(dayOfMonth));
  textOut(" ");
  textOut(String(hour));
  textOut(":");
  textOut(String(minute));
  textOut(":");
  textOut(String(second));
  textOut("\n");
}

uint32_t currentRTCTimestamp(){
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  //Serial.println(dayOfMonth);
  //Serial.println(month);
  //Serial.println(year);
  return getSecsSinceEpoch((uint16_t)1970, (uint8_t)month,  (uint8_t)dayOfMonth,   (uint8_t)year,  (uint8_t)hour,  (uint8_t)minute,  (uint8_t)second);
}

void cleanup(){
  ESP.getFreeHeap(); // This sometimes triggers internal cleanup
  yield();    
}

//FRAM functions 

void writeRecordToFRAM(const std::vector<std::tuple<uint8_t, uint8_t, double>>& record) {
  //Serial.println(currentRecordCount);
  if (currentRecordCount >= ci[FRAM_INDEX_SIZE]) {
    currentRecordCount = 0;
  }
  uint16_t recordStartAddress = (currentRecordCount == 0) 
    ? framIndexAddress + (ci[FRAM_INDEX_SIZE] * 2)  // Leave space for the index table
    : read16(framIndexAddress + uint16_t((currentRecordCount -1) * 2)) + lastRecordSize + 1; //record.size is 4 now?
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
    if(ci[DEBUG] > 0) {
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
  if(ci[DEBUG] > 0) {
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

/////////////////////////////////////////////
//config routines
/////////////////////////////////////////////
int loadAllConfig(int mode, uint16_t param){
  if(ci[CONFIG_PERSIST_METHOD] == CONFIG_PERSIST_METHOD_I2C_SLAVE) {
    return loadAllConfigFromEEPROM(mode, param);
  } else if (ci[CONFIG_PERSIST_METHOD] == CONFIG_PERSIST_METHOD_FLASH) {
    return loadAllConfigFromFlash(mode, param);
  } else {
    return 0;
  }
  return 0;
}


/////////////////////////////////////////////
//file system routines
/////////////////////////////////////////////

bool downloadFile(const char* url, const char* localPath) {
  if (!LittleFS.begin()) {
    textOut("LittleFS mount failed\n");
    return false;
  }
  WiFiClient client;
  HTTPClient http;
  textOut("Downloading: ");
  textOut(String(url) + "\n");
  if (!http.begin(client, url)) {
    textOut("HTTP begin failed\n");
    return false;
  }
  int httpCode = http.GET();
  //Serial.println(http.header("Content-Encoding"));
  //Serial.println(http.header("Content-Type"));
  if (httpCode != HTTP_CODE_OK) {
    textOut("HTTP GET failed, code: " + String(httpCode) + "\n");
    http.end();
    return false;
  }
  File file = LittleFS.open(localPath, "w");
  if (!file) {
    textOut("Failed to open local file for writing\n");
    http.end();
    return false;
  }
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[128];
  int len = http.getSize();
  int total = 0;
  while (http.connected() || stream->available()) {
    size_t available = stream->available();
    if (available) {
      int c = stream->readBytes(buffer, min(available, sizeof(buffer)));
      file.write(buffer, c);
      total += c;
    } else {
      delay(1);  // give network time to breathe
    }
    yield();
  }
  file.close();
  http.end();
  textOut("Downloaded " + String(total) + " bytes\n");
  return true;
}

bool makeDir(const char* path) {
  if (!LittleFS.begin()) {
    textOut("LittleFS mount failed\n");
    return false;
  }
  // Ensure leading slash
  if (path[0] != '/') {
    textOut("Path must start with /\n");
    return false;
  }
  // Check if already exists
  if (LittleFS.exists(path)) {
    textOut("Folder already exists (or a file blocks it): ");
    textOut(path);
    textOut("\n");
    return true;  // treat as success
  }
  // Create placeholder file to force the directory to exist
  String placeholder = String(path) + "/.placeholder";
  File f = LittleFS.open(placeholder.c_str(), "w");
  if (f) {
    f.close();
    textOut("Created folder with placeholder: ");
    textOut(path);
    textOut("\n");
    return true;
  } else {
    textOut("Failed to create folder placeholder: ");
    textOut(path);
    textOut("\n");
    return false;
  }
}

bool deleteFile(const char* path) {
  if (!LittleFS.begin()) {
    textOut("LittleFS mount failed\n");
    return false;
  }
  if (!LittleFS.exists(path)) {
    textOut("file does not exist: ");
    textOut(path);
    textOut("\n");
    return false;
  }
  if (LittleFS.remove(path)) {
    textOut("deleted: ");
    textOut(path);
    textOut("\n");
    return true;
  } else {
    textOut("failed to delete: ");
    textOut(path);
    textOut("\n");
    return false;
  }
}

void dumpFile(const char* filename) {
    if (!LittleFS.begin()) {
        return;
    }
    File f = LittleFS.open(filename, "r");
    if (!f) {
        textOut("File open failed\n");
        return;
    }
    const int BUF_SIZE = 128;
    char buffer[BUF_SIZE + 1];
    textOut("\n");
    while (f.available()) {

        size_t n = f.readBytes(buffer, BUF_SIZE);
        buffer[n] = 0;  // null terminate

        textOut(buffer);  // ?? send this chunk immediately
    }
    textOut("\n");
    f.close();
}

void formatFileSystem() {
  LittleFS.format();
  LittleFS.begin();
  textOut("File system formatted\n");
}

void listFiles() {
  if(!LittleFS.begin()) {
    return;
  }
  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    textOut("FILE: " + String(dir.fileName()) + " (" + String(dir.fileSize()) + " bytes)\n");
    yield();
  }
}

int loadAllConfigFromFlash(int mode, uint16_t param) { //can also be used to recover values from FLASH
    if (!LittleFS.begin()) {
        return 0;
    }
    File f = LittleFS.open("/config.cfg", "r");
    if (!f) {
        return 0;
    }
    int*  activeCi;
    char** activeCs;
    int totalConfigItems       = CONFIG_TOTAL_COUNT;
    int totalStringConfigItems = CONFIG_STRING_COUNT;
    //Serial.println("----------SIZES");
    //Serial.println(totalConfigItems);
    //Serial.println(totalStringConfigItems);
    activeCi = ci;
    activeCs = cs;
    // ============================================================
    // 1. Read marker (must be "DATA")
    // ============================================================
    char marker[5];
    if (f.readBytes(marker, 5) != 5) {
        f.close();
        return 0;
    }
    
    marker[4] = '\0';
 
    if (strcmp(marker, "DATA") != 0) {
        f.close();
        return 0;
    }
 
    // ============================================================
    // 2. Read all integer configs (int16)
    // ============================================================
    for (int i = 0; i < totalConfigItems; i++) {
        uint8_t lo = f.read();
        uint8_t hi = f.read();
        int16_t v = (hi << 8) | lo;
        //Serial.print("value for #" + String(i) + ": ");
        //Serial.println(v);
        if (mode == 0) {
            activeCi[i] = v;
        } else if (mode == 1) {
            textOut(String(v));
            textOut("\n");
        }
        yield();
    }

    // ============================================================
    // 3. Read all string configs
    // ============================================================
    for (int i = 0; i < totalStringConfigItems; i++) {
        char buffer[128];
        int pos = 0;

        while (pos < 127 && f.available()) {
            char c = f.read();
            buffer[pos++] = c;
            if (c == 0) break;
        }
        buffer[127] = 0;
        yield();
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
            //Serial.print("string value for #" + String(i) + ": ");
            //Serial.println(buffer);
        } else if (mode == 1) {
            textOut(buffer);
            textOut("\n");
        }
    }

    f.close();

    if (mode == 0) return 1;
    return 0;
}

void saveAllConfigToFlash(uint16_t param) {
    int* activeCi;
    char** activeCs;
    uint32_t slaveConfigLocation = requestLong(ci[SLAVE_I2C], 164);
    int totalConfigItems = CONFIG_TOTAL_COUNT;
    int totalStringConfigItems = CONFIG_STRING_COUNT;
    activeCi = ci;
    activeCs = cs;
    if(false && param >= slaveConfigLocation) {
        totalConfigItems = CONFIG_SLAVE_TOTAL_COUNT;
        totalStringConfigItems = CONFIG_SLAVE_STRING_COUNT;
        activeCi = cis;
        activeCs = css;
    }
    if(!LittleFS.begin()) {
        return;
    }
    File f = LittleFS.open("/config.cfg", "w");
    if(!f) {
        return;
    }

    // ============================================================
    // 1. Write marker "DATA\0"
    // ============================================================

    f.write((const uint8_t*)"DATA", 4);
    f.write((uint8_t)0);

    // ============================================================
    // 2. Write integer configs (2 bytes each)
    // ============================================================

    for(int i = 0; i < totalConfigItems; i++) {
        //Serial.println(i);
        int16_t v = activeCi[i];

        uint8_t lo = v & 0xFF;
        uint8_t hi = (v >> 8) & 0xFF;

        f.write(lo);
        f.write(hi);
        yield();
    }
    // ============================================================
    // 3. Write strings (null terminated)
    // ============================================================
    for(int i = 0; i < totalStringConfigItems; i++) {
        //Serial.println(i);
        const char* s = activeCs[i];
        if(s == NULL) s = "";
    
        //Serial.print("Value: ");
        //Serial.println(s);
        if(s == NULL) {
          s = "";
        }
        size_t len = strlen(s);
        f.write((const uint8_t*)s, len);
        f.write((uint8_t)0);
        yield();
    }
    f.close();
}

//serve a file from the LittleFS file system via http 
void handleFileRequest() {
  String path = server.uri(); // "/file.txt"
  if (path.startsWith("/")) {
    path = path.substring(1); // "file.txt"
  }
  if (path.length() == 0 || !LittleFS.exists("/" + path)) {
    // File doesn't exist, pass control to default 404
    server.send(404, "text/plain", "Not Found");
    return;
  }
  File file = LittleFS.open("/" + path, "r");
  if (!file) {
    server.send(500, "text/plain", "Failed to open file");
    return;
  }
  // Basic MIME type detection
  String contentType = "text/plain";
  if (path.endsWith(".html")) {
    contentType = "text/html";
  } else if (path.endsWith(".css")){
    contentType = "text/css";
  } else if (path.endsWith(".js")) {
    contentType = "application/javascript";
  } else if (path.endsWith(".png")) {
    contentType = "image/png";
  } else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) {
    contentType = "image/jpeg";
  }
  server.streamFile(file, contentType);
  file.close();
}
/////////////////////////////////////////////
//processor-specific
/////////////////////////////////////////////
int freeMemory() {
    return ESP.getFreeHeap();
}
