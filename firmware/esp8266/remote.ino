/*
 * ESP8266 Remote Control. Also sends weather data from multiple kinds of sensors (configured in config.c) 
 * originally built on the basis of something I found on https://circuits4you.com
 * reorganized and extended by Gus Mueller, April 24 2022 - January 10 2026
 * Also resets a Moxee Cellular hotspot if there are network problems
 * since those do not include watchdog behaviors
 */

 //note:  this needs to be compiled in the Arduino environment alongside other files in the repo directory
 

#include "globals.h"
#include "i2cslave.h"
#include "utilities.h"
#include "slaveupdate.h"
#include "commandhandlers.h"
#include "commandregistry.h"
#include "filefunctions.h"
#include "framfunctions.h"
#include "rtcfunctions.h"

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
#include <Adafruit_TMAG5273.h>

#include <WebSocketsClient.h>

#include <NTPClient.h>
#include <WiFiUdp.h>
 

#include <Adafruit_BME680.h>   
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


/////////////////
//serial parser setup:

//uint32_t serialParsedData[PARSED_SERIAL_MAX]; //needed to be in global.cpp
 
//uint8_t blockCount = 0; had to move to globals.cpp
int8_t activeBlock = -1;
uint32_t lastDataParseMillis = 0;

//////////////////////////////
//safe mode implementation:

uint32_t rtcChecksum(const RTCBootInfo &d) {
  return d.magic ^ d.lastMillis ^ d.rebootCount ^ d.lastCommandLogId ^ d.lastVersion ^ d.lastCommandId ^ d.lastCommandType  ^ d.useHardcodedConfig  ^ d.flagValue;
}

bool rtcRead(RTCBootInfo &out) {
  ESP.rtcUserMemoryRead(0, (uint32_t*)&out, sizeof(out));
  if (out.magic != RTC_MAGIC) {
    return false;
  }
  uint32_t check = rtcChecksum(out);
  return (check == out.checksum);
}

void rtcWrite(RTCBootInfo &d) {
  d.magic = RTC_MAGIC;
  d.checksum = 0;             // Reset to 0 so it doesn't affect the XOR
  d.checksum = rtcChecksum(d); // Now calculate
  bool ok = ESP.rtcUserMemoryWrite(0, (uint32_t*)&d, sizeof(d));
  //Serial.printf("RTC write: %s\n", ok ? "OK" : "FAIL");
}

void rtcInitOnBoot() {
  if (!rtcRead(rtc)) {
    if(ci[DEBUG] > 0) {
      //Serial.println(F("Had to re-init RTC data"));
    }   
    // Fresh start (power loss or garbage)
    rtc.magic = RTC_MAGIC;
    rtc.lastMillis = 0;
    rtc.rebootCount = 0;
    rtc.lastCommandLogId = 0;
    rtc.lastVersion = 0;
    rtc.lastCommandId = 0;
    rtc.lastCommandType = 0;
    rtc.flagValue = 16000;
    rtcWrite(rtc);
  }
  // Each boot increments this
  rtc.rebootCount++;
  // Start fresh heartbeat
  //rtc.lastMillis = 0;
}

void rtcMarkStable() {
  rtc.rebootCount = 0;
  rtcWrite(rtc);
}

void rtcUpdateHeartbeat() {
  rtc.flagValue = 17000;
  rtc.lastMillis = millis();
  //Serial.println("heartbeat city");
  rtcWrite(rtc);
}
////////////////////////


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
  if(mode == "commandout") {
    consecutiveCommandOuts++;
  } else {
    consecutiveCommandOuts = 0;
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

void stopWebSocket(){
  //lastGoodKey = "";
  if(outputMode == 3) {
    responseBuffer = fastResponseBuffer; //anything heading to the output now has to take the responseBuffer route
    fastResponseBuffer = "";
    //Serial.println("OUTPUTMODE CHANGEO1: " + String(outputMode) + " TO " + String(oldOutputMode));
    outputMode = oldOutputMode;
    lastTimeOutputModeChanged = millis();

    //Serial.println("---------------");
    //Serial.println(outputMode);
    saveCommandState(0, 0, 0, 0);
  }
  webSocket.disconnect();
}

void startWebSocket(int origin){
  //origin only for debugging
  if(ci[DEBUG] > 2) {
    Serial.println(F("Origin: ") + String(origin));
  }
  //if we have a pending lastCommandLogId we need to stay out of here
  if(lastCommandLogId > 0) {
    return;
  }

  if(outputMode != 3) {
    oldOutputMode = outputMode;
    //oldOutputMode = 0;
    lastTimeOutputModeChanged = millis();
    //Serial.println("OUTPUTMODE CHANGEO2: " + String(outputMode) + " TO " + String(3));
  }
  outputMode = 3;
  if(webSocket.isConnected()) {
    return;
  }
  saveCommandState(0, VERSION, -3, 0);
  wiFiConnect();
 
  char socketUrl[128];
  snprintf(
    socketUrl,
    sizeof(socketUrl),
    "/?k2=%s&type=device&device_id=%d",
    lastGoodKey.c_str(),
    ci[DEVICE_ID]
  );

  if(ci[DEBUG] > 1) {
    Serial.println(socketUrl);
  }
  webSocket.begin(cs[HOST_GET], 8080, socketUrl);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length){
    if(outputMode != 3) {
      return;
    }
    switch(type)
    {
        case WStype_CONNECTED:
            webSocketConnected = true;
            //textOut("WebSocket connected\n");
            if(ci[DEBUG] > 1) {
              Serial.println(F("WebSocket connected"));
            }
            //webSocket.sendTXT("ESP8266 connected");
            break;

        case WStype_TEXT:
            doFastCommands(String((char*)payload));
            //textOut("Received : ");
            //textOut(String((char*)payload));
            //Serial.println(String((char*)payload));
            //textOut("\n");
            
            break;

        case WStype_DISCONNECTED:
             webSocketConnected = false;
            if(ci[DEBUG] > 1) {
              Serial.println(F("WebSocket disconnected"));
            }
            startWebSocket(2);
            //textOut("WebSocket disconnected\n");
            break;
    }
}

void doFastCommands(String payload) {
  static String command;
  yield();
  String fullCommand = "!-3|" + payload;
  runCommand(fullCommand.c_str(), false);
  return;
}


void maintainWebSocket() {
  if(outputMode != 3) {
    return;
  }
  if(millis() - lastWebSocketCheck < 6000) {
    return;
  }
  lastWebSocketCheck = millis();
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi down, reconnecting...");
    wiFiConnect();
    return;
  }
  // no manual websocket restart here
  // library handles it automatically
}

//ESP8266's home page:----------------------------------------------------
void handleRoot(){
 String s = MAIN_page; //Read HTML contents
 server.send(200, "text/html", s); //Send web page
}

void lookupLocalPowerData(){//sets the globals with the current reading from the ina219
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

String weatherDataString(int sensorId, int sensorSubtype, int dataPin, int powerPin, int i2c, int deviceFeatureId, char objectCursor, String sensorName,  int* ordinalOfOverwrite, int consolidateAllSensorsToOneRecord) {
  if(ci[POLLING_SKIP_LEVEL] < 10) {
    return "";
  }
  // --- locals ---
  double humidityFromSensor = NAN;
  double temperatureFromSensor = NAN;
  double pressureFromSensor = NAN;
  double gasFromSensor = NAN;
  double xFromSensor = NAN;
  double yFromSensor = NAN;
  double zFromSensor = NAN;
  double angleFromSensor = NAN;
  double magnitudeFromSensor = NAN;
  String sensorValueStr[4];
  for(int i=0; i<4; i++) {
    sensorValueStr[i] = 0;
  }
  if (deviceFeatureId == 0) {
    objectCursor = 0;
  }
  // sensor-reading branches (unchanged)
  if (sensorId == 1) {
    if (powerPin > -1) {
      digitalWrite(powerPin, HIGH);
    }
    delay(10);
    if (i2c) {
      sensorValueStr[0] = String(getPinValueOnSlave((char)i2c, (char)dataPin));
    } else {
      sensorValueStr[0] = String(analogRead(dataPin));
    }
    //Serial.println("+++++++++++++++++++++");
    //Serial.println(sensorValueStr);
    
    if (powerPin > -1) {
      digitalWrite(powerPin, LOW);
    }
  } else if(sensorId == 5273) {
    xFromSensor = tmag[objectCursor].readMagneticX();
    yFromSensor = tmag[objectCursor].readMagneticY();
    zFromSensor = tmag[objectCursor].readMagneticZ();
    tmag[objectCursor].setAngleCalculation(TMAG5273_ANGLE_XY);
    temperatureFromSensor = (double)tmag[objectCursor].getTemperature();
    angleFromSensor = (double)tmag[objectCursor].readAngle();
    magnitudeFromSensor = (double)tmag[objectCursor].readMagnitudeMT();
    //Serial.print(angleFromSensor);
    //Serial.print("; ");
    //Serial.println(magnitudeFromSensor);
    //temperature already handled
    sensorValueStr[1] = String(angleFromSensor);
    sensorValueStr[2] = String(magnitudeFromSensor);
  } else if (sensorId == 53) {
    VL53L0X_RangingMeasurementData_t measure;
    lox[objectCursor].rangingTest(&measure, false);
    if (measure.RangeStatus != 4) {
      sensorValueStr[0] = String(measure.RangeMilliMeter);
    } else {
      sensorValueStr[0] = "-1";
    }
  } else if (sensorId == 680) {
    int32_t humidityRaw = 0, temperatureRaw = 0, pressureRaw = 0, gasRaw = 0;
    BME680[objectCursor].performReading();
 
    humidityFromSensor = (double)BME680[objectCursor].humidity;
    temperatureFromSensor = (double)BME680[objectCursor].temperature;
    pressureFromSensor = (double)BME680[objectCursor].pressure/100;
    gasFromSensor = (double)BME680[objectCursor].gas_resistance;
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
  if (!isnan(temperatureFromSensor)) { 
    sensorValueStr[0] = temperatureFromSensor;
  }
  if (!isnan(pressureFromSensor)) { 
    sensorValueStr[1] = pressureFromSensor;
  }
  if (!isnan(humidityFromSensor)) { 
    sensorValueStr[2] = humidityFromSensor;
  }
  if (!isnan(gasFromSensor)) { 
    sensorValueStr[3] = gasFromSensor;
  }
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t maxBlock = ESP.getMaxFreeBlockSize();
  uint32_t fragPct = 100 - (maxBlock * 100 / freeHeap);

  String out="";
  for(int fieldCounter = 0; fieldCounter<16; fieldCounter++) {
    bool wasSpecifiedInSensorString = false;
    if (ordinalOfOverwrite != nullptr) {
      for(int i=0; i<4; i++) {
        if(ordinalOfOverwrite[i] == fieldCounter) {
          if(ordinalOfOverwrite[i] > -1) {
            out +=  sensorValueStr[i];
            wasSpecifiedInSensorString = true;
            continue;
          }
        } 
      }
    }
    if(!wasSpecifiedInSensorString) {
      if (ci[SEND_TELEMETRY_TYPE_IN_RESERVED] == TELEMETRY_TYPE_MEMORY) {
        if (fieldCounter == 12) {
          out += String(freeHeap);
  
        } else if (fieldCounter == 13) {
          out += String(maxBlock);
  
        } else if (fieldCounter == 14) {
          out += String(fragPct);
        }
      } else if (ci[SEND_TELEMETRY_TYPE_IN_RESERVED] == TELEMETRY_TYPE_TIMING) {
        if (fieldCounter == 12 && loopCount > 0) {
          out += String(millis()/loopCount);
  
        } else if (fieldCounter == 13 && loopCount > 0) {
          out += String(serialByteCount/loopCount);
  
        } else if (fieldCounter == 14  && connectionCount > 0) {
          out += String(millisecondsConnecting/connectionCount);
        } else if (fieldCounter == 15) {
          out += String(serialByteCount);
        } 
      }
      if(fieldCounter == 0) {
        out += nullifyOrNumber(temperatureFromSensor);
      } else if (fieldCounter == 1){
        out += nullifyOrNumber(pressureFromSensor);
      } else if (fieldCounter == 2) {
        out += nullifyOrNumber(humidityFromSensor);
      } else if (fieldCounter == 3) {
        out += nullifyOrNumber(gasFromSensor);
      } else if (fieldCounter == 4) {
        out += nullifyOrNumber(angleFromSensor);
      } else if (fieldCounter == 5) {
        out += nullifyOrNumber(magnitudeFromSensor);
      }
    }
    out += "*";
  }
 
  // offline FRAM logging  
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
          if(ci[DEBUG] > 1) {
            textOut(String(currentRTCTimestamp()));
            textOut("\n");
          }
        } else {
          addOfflineRecord(framWeatherRecord, 32, 2, timeClient.getEpochTime());
        }
        writeRecordToFRAM(framWeatherRecord);
        if(ci[DEBUG] > 1) {
          textOut(F("Saved a record to FRAM.\n"));
        }
        // print trimmed tx for info
        if(ci[DEBUG] > 1) {
          textOut(out);
          textOut(String(millisVal));
          textOut("\n");
        }
      }
      lastOfflineLog = millis();
    }
  }
  out += String(sensorId) + "*" + deviceFeatureId +  "*" + sensorName + "*" + String(consolidateAllSensorsToOneRecord); 
  return out;
}

void startWeatherSensors(int sensorIdLocal, int sensorSubTypeLocal, int i2c, int pinNumber, int powerPin) {
  //i've made all these inputs generic across different sensors, though for now some apply and others do not on some sensors
  //for example, you can set the i2c address of a BME680 or a BMP280 but not a BMP180.  you can specify any GPIO as a data pin for a DHT
  int objectCursor = 0;
 
  auto it = sensorObjectCursor.find(String(sensorIdLocal));
  if (it != sensorObjectCursor.end()) {
      objectCursor = it->second;
  }
  int attemptCount = 0;
  if(sensorIdLocal == 5273) {
    if (!tmag[objectCursor].begin()) {
      textOut(F("Failed to find TMAG5273 sensor!"));
      while (attemptCount < 20) {
        delay(10 * attemptCount/5);
      }
    }
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
      if(ci[DEBUG] > 1) {
        textOut(F("Failed to boot VL53L0X\n"));
      }
    } else {
      if(ci[DEBUG] > 1) {
        textOut(F("VL53L0X at "));
        textOut(String(i2c));
        textOut("\n");
      }
    }
  } else if(sensorIdLocal == 680) {
    if (ci[DEBUG] > 1) {
      textOut(F("Initializing BME680 sensor...\n"));
    }
    
    if (!BME680[objectCursor].begin(i2c)) {
      if (ci[DEBUG] > 1) {
        textOut(F(" - Unable to find BME680.\n"));
      }
    } else {
      //consult this page: https://github.com/pchwalek/STM32_BME680/blob/master/bme68x_defs.h
      //to get the actual integers you might want to use for the ci[SENSOR_PARAM_X] values
      if (ci[DEBUG] > 1) {
        textOut(F("- Setting oversampling for all sensors: "));
        textOut(String(ci[SENSOR_PARAM_1]) + ", " + String(ci[SENSOR_PARAM_2]) + ", " + String(ci[SENSOR_PARAM_3]) + ", " + String(ci[SENSOR_PARAM_4]) + ", " + String(ci[SENSOR_PARAM_5]) + ", " + String(ci[SENSOR_PARAM_6]));
        textOut("\n");
      }
      BME680[objectCursor].setTemperatureOversampling(ci[SENSOR_PARAM_1]);
      BME680[objectCursor].setHumidityOversampling(ci[SENSOR_PARAM_2]);
      BME680[objectCursor].setPressureOversampling(ci[SENSOR_PARAM_3]);
      if (ci[DEBUG] > 1) {
        textOut(F("- Setting IIR filter to ") + String(ci[SENSOR_PARAM_4]) + F(" samples\n"));
      }
      BME680[objectCursor].setIIRFilterSize(ci[SENSOR_PARAM_4]);
      if (ci[DEBUG] > 1) {
        textOut(F("- Setting gas measurement to ") + String(ci[SENSOR_PARAM_5]) + F("°C at ") + String(ci[SENSOR_PARAM_6]) + F(" milliseconds\n"));
      }
      BME680[objectCursor].setGasHeater(ci[SENSOR_PARAM_5], ci[SENSOR_PARAM_6]);
    }
  } else if (sensorIdLocal == 2301) {
    if(ci[DEBUG] > 1) {
      textOut(F("Initializing DHT AM2301 sensor at pin: "));
      textOut(String(pinNumber));
      textOut("\n");
    }
      
    if(powerPin > -1) {
      pinMode(powerPin, OUTPUT);
      digitalWrite(powerPin, LOW);
    }
    dht[objectCursor] = new DHT(pinNumber, sensorSubTypeLocal);
    dht[objectCursor]->begin();
  } else if (sensorIdLocal == 2320) { //AHT20
    if (AHT[objectCursor].begin()) {
      if(ci[DEBUG] > 1) {
        textOut(F("Found AHT20\n"));
      }
    } else {
      if(ci[DEBUG] > 1) {
        textOut(F("Didn't find AHT20\n"));
      }
    }  
  } else if (sensorIdLocal == 7410) { //adt7410
    adt7410[objectCursor].begin(i2c);
    adt7410[objectCursor].setResolution(ADT7410_16BIT);
  } else if (sensorIdLocal == 180) { //BMP180
    BMP180[objectCursor].begin();
  } else if (sensorIdLocal == 85) { //BMP085
    if(ci[DEBUG] > 1) {
      textOut(F("Initializing BMP085...\n"));
    }
    BMP085d[objectCursor].begin();
  } else if (sensorIdLocal == 280) {
    if(ci[DEBUG] > 1) {
      textOut(F("Initializing BMP280 at i2c: "));
      textOut(String(i2c));
      textOut(F(" objectcursor:"));
      textOut(String(objectCursor));
      textOut("\n");
    }
    if(!BMP280[objectCursor].begin(i2c)){
      if(ci[DEBUG] > 1) {
        textOut(F("Couldn't find BMX280!\n"));
      }
    }
  }
  sensorObjectCursor[String(sensorIdLocal)] = objectCursor + 1;//we keep track of how many of a particular ci[SENSOR_ID] we use
}

void handleWeatherData() {
  String transmissionString = weatherDataString(ci[SENSOR_ID], ci[SENSOR_SUB_TYPE], ci[SENSOR_DATA_PIN], ci[SENSOR_POWER_PIN], ci[SENSOR_I2C], NULL, 0, deviceName, nullptr, ci[CONSOLIDATE_ALL_SENSORS_TO_ONE_RECORD]);
  server.send(200, "text/plain", transmissionString); //Send values only to client ajax request
}

void compileAndSendDeviceData(const String& weatherData, const String& whereWhenData, const String& powerData, bool doPinCursorChanges, uint16_t fRAMOrdinal) {
    if(ci[POLLING_SKIP_LEVEL] < 8){
      return;
    }
    // Large fixed buffer (adjust as needed)
    /*
    if(weatherData.length() >0) {
      Serial.println("...............");
      Serial.println(fRAMOrdinal);
      Serial.println(weatherData);
    }
    */
    static char tx[2048];
    int pos = 0;
    // --- WEATHER DATA -------------------------------------------------
    if (weatherData.length() > 0) {
        pos += snprintf(tx + pos, sizeof(tx) - pos, "%s", weatherData.c_str());
    } else if (ci[SENSOR_ID] > -1) {
        String s = weatherDataString(ci[SENSOR_ID], ci[SENSOR_SUB_TYPE], ci[SENSOR_DATA_PIN], ci[SENSOR_POWER_PIN], ci[SENSOR_I2C], NULL, 0, deviceName, nullptr, ci[CONSOLIDATE_ALL_SENSORS_TO_ONE_RECORD]);
        //add solar data here if known
        
        
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
    pos += snprintf(tx + pos, sizeof(tx) - pos, "|");
     
    //doing it a different way now
    //we don't want to transmit serial parsed data until we actually have some and don't want to transmit such data more than 30 seconds stale
    if(serialDataParsed > 30  && (millis() - lastDataParseMillis)/1000 < 30) { 
      pos += snprintf(tx + pos, sizeof(tx) - pos, joinValsOnDelimiter(serialParsedData, "*", MAX_PARSED_SERIAL_VALUES).c_str());
    }

    //bytesToHex(parsedBuf, parsedStringPacketLen, 0x00, parsedSerial);
    //pos += snprintf(tx + pos, sizeof(tx) - pos, parsedSerial);
    // future expansion
    pos += snprintf(tx + pos, sizeof(tx) - pos, "||");
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
        "%d*%d*%d*%s*%d*%d*%d*%d",
        lastCommandId, pinCursor, (int)localSource,
        ipToUse.c_str(), (int)requestNonJsonPinInfo,
        (int)justDeviceJson, changeSourceId, outputMode
    );
    // pinMap
    {
        String s = joinPinMapValsOnDelimiter("*");
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
  if(WiFi.status() == WL_CONNECTED) {
    return;
  }
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
      if(ci[DEBUG] > 1) {
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
          if(ci[DEBUG] > 1) {
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
          if(ci[DEBUG] > 1) {
            Serial.print("*");
          }
        }
        if (WiFi.status() == WL_NO_SSID_AVAIL) {
          if(ci[DEBUG] > 1) {
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
          if(ci[DEBUG] > 1) {
            Serial.println("");
            Serial.println(F("WiFi taking too long"));
          }
          if(ci[MOXEE_POWER_SWITCH] > 0) {
            if(ci[DEBUG] > 1) {
              Serial.println(F("rebooting Moxee"));
            }
            rebootMoxee();
          }
          if(ci[DEBUG] > 1) {
            Serial.println(F("trying another"));
          }
          wiFiSeconds = 1;
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
      if(ci[DEBUG] > 1) {
        Serial.printf("\nConnected to %s, IP: %s\n", wifiSsid, WiFi.localIP().toString().c_str());
      }
      ipAddress = WiFi.localIP().toString();
      break;
    }
  }
  if (!connected) {
    if(validWiFiAttempts > 0) {
      if(ci[DEBUG] > 1) {
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
  static String encryptedStoragePassword;
  switch(remoteState) {
    case RS_IDLE:
      if(ci[DEBUG] > 20) {
        textOut(F("RS_IDLE\n"));
      }
      return;

    // -------------------- prepare the URL and initial decisions --------------------
    case RS_PREPARE: {
      millisOneConnection = millis();
      if(ci[DEBUG] > 10) {
        textOut(F("RS_PREPARE\n"));
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
      encryptedStoragePassword = encryptStoragePassword(remoteDatastring);
      // build URL exactly like before
      remoteURL = String(cs[URL_GET]) + "?k2=" + encryptedStoragePassword + "&device_id=" + ci[DEVICE_ID] + "&mode=" + remoteMode + "&data=" + urlEncode(remoteDatastring, true);
      if(additionalUrlParams != "") {
        remoteURL += "&" + additionalUrlParams;
      }
      if(ci[DEBUG] > 1) {
        textOut(remoteURL);
        textOut("\n");
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
        textOut(F("RS_CONNECTING\n"));
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
          textOut(F("RS_CONNECTING...connected\n"));
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
          textOut(F("RS_CONNECTING...port 80\n"));
        }
        remoteState = RS_SENDING_REQUEST;
        stateStartMs = millis();
        connectionFailureMode = false;
        yield();
      } else {
         if(ci[DEBUG] > 20) {
          textOut(F("RS_CONNECTING...otherwise\n"));
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
          if(ci[DEBUG] > 2) {
            textOut("\n");
            textOut(F("Connection failed (host): "));
            textOut(cs[HOST_GET]);
            textOut("\n");
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
        textOut(F("RS_CONNECT_WAIT\n"));
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
        textOut(F("RS_SENDING_REQUEST\n"));
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
        textOut(F("RS_WAITING_FOR_REPLY\n"));
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
        textOut(F("RS_READING_REPLY\n"));
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
        textOut(F("RS_PROCESSING_REPLY\n"));
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
        //Serial.println("(((((((((((((((((((");
        //Serial.println(line);
        //backend confirmation logic (heap-safe version)
        // check: line does NOT contain "\"error\":"
        bool hasError = (strstr(line, "\"error\":") != NULL);
        // check: first character
        char firstChar = line[0];
        bool validStart = (firstChar == '{' || firstChar == '*' || firstChar == '|' || firstChar == '=');
        // check: remoteMode (still a String, but not created in loop)
        bool validMode = (remoteMode == F("saveData") || remoteMode == F("commandout") || remoteMode == F("savePacket"));
        if (!hasError && remoteMode == F("getInitialDeviceInfo") && validStart) {
          //Serial.println("hmmm: " + String(remoteMode) + ": " + String(outputMode) + String(": ") + lastGoodKey + String(": ") + encryptedStoragePassword );
          if(lastGoodKey == "") {
            //Serial.println("well");
            lastGoodKey = encryptedStoragePassword;//set the global
          }
        } else if (!hasError && validMode && validStart) {
          if(remoteFRAMordinal != 0xFFFF) {
            changeDelimiterOnRecord(remoteFRAMordinal, 0xFE); //chatgpt in one of its suggested refactors totally forgot to do this; the robots will never replace us
          }
          permissionErrorCount = 0;
          if(remoteMode == F("saveData")) {//we really want data regularly
            lastDataLogTime = millis();
          }
          moxeeRebootCount = 0;
          for (int i = 0; i < 11; i++){
            moxeeRebootTimes[i] = 0;
          }
          if (lastCommandLogId == 0 && responseBuffer == "" && outputMode == 0) {
            canSleep = true;
          }
          if (remoteMode == F("commandout") || outputMode == 2) {
            lastCommandLogId = 0;
            
            if(outputMode == 2){
              //Serial.println("OUTPUTMODE CHANGEO3: " + String(2) + " TO " + String(oldOutputMode));
              outputMode = oldOutputMode;
              if(oldOutputMode != 2) { 
                lastTimeOutputModeChanged = millis();
              }
              responseBuffer = "";
            }
          }
          // run deferred command safely
          if (deferredCommand && deferredCommand[0] != '\0') {
            yield();
            runCommand(deferredCommand, true);
            
          }
          /*
          if(oldOutputMode != outputMode) {
                Serial.println("=======");
                Serial.print(oldOutputMode);
                Serial.print(" ");
                Serial.println(outputMode);
                
                oldOutputMode = outputMode;
                Serial.print(oldOutputMode);
                Serial.print(" ");
                Serial.println(outputMode);
       
          }
          */
 
        } else if(hasError) {
          permissionErrorCount++;
        }
        if(permissionErrorCount > 10) {
          if(ci[DEBUG] > 1) {
            textOut(F("Alterable configuration not working; trying hardcoded defaults\n"));
          }
          initMasterDefaults(); //if we come up with the wrong configuration stored in alterable storage, we will have repeated permission errors, so, we need to use the baked-in configuration
        }
        // ============================
        // HANDLE CASES 
        // ============================
        if (first == '*') {
          if (ci[DEBUG] > 1) {
            textOut(F("Initial: "));
            textOut(String(line));
            textOut("\n");
          }
          //format of sensor_config string:
          //pinNumber*powerPin*sensorId*sensorSubType*i2c*deviceFeatureId*sensorName*ordinalOfOverwrite0*ordinalOfOverwrite1*ordinalOfOverwrite2*ordinalOfOverwrite3*consolidateAllRecords
          if (cs[SENSOR_CONFIG_STRING] != "") {
            // ?? this still uses String, but outside tight loop frequency
            String tmp = line;
            if (ci[DEBUG] > 41) {
              textOut(F("About to do a replaceFirstOccurrenceAtChar\n"));
            }
            tmp = replaceFirstOccurrenceAtChar(tmp, String(cs[SENSOR_CONFIG_STRING]), '|');
            if (ci[DEBUG] > 41) {
              textOut(F("About to do a strncpy\n"));
            }
            int newLen = tmp.length();
            char *newBuf = (char*)realloc(buf, newLen + 1);
            if (newBuf == nullptr) {
                // handle allocation failure
                if (ci[DEBUG] > 1) {
                  textOut("ALLOCATION FAILURE\n");
                } 
            } else {  
                buf = newBuf;
                line = buf;
                len = newLen + 1;  // capacity INCLUDING null
            }
            snprintf(line, len, "%s", tmp.c_str());
            //strncpy(line, tmp.c_str(), len); // copy back
            //line[len - 1] = '\0';
            if (ci[DEBUG] > 41) {
              textOut(F("Did a strncpy\n"));
            }
            //still crashing right about here
          }
          additionalSensorInfo = String(line);
          if (ci[DEBUG] > 40) {
            textOut(F("About to handle device name\n"));
          }
          handleDeviceNameAndAdditionalSensors((char *)additionalSensorInfo.c_str(), true);
          if (ci[DEBUG] > 40) {
            textOut(F("Handled device name\n"));
          }
          break;
        } else if (first == '{') {
          if (ci[DEBUG] > 1) {
            textOut(F("JSON: "));
            textOut(String(line));
            textOut("\n");
          }
          receivedDataJson = true;
          break;
        } else if (first == '|') {
          if (ci[DEBUG] > 1) {
            textOut(F("delimited: "));
            textOut(String(line));
            textOut("\n");
          }
          // split in-place on '!'
          char *parts[3] = {0};
          int part = 0;
          parts[part++] = line;
          if (ci[DEBUG] > 12) {
            textOut(F("before line parse loop\n"));
          }
          for (char *p = line; *p && part < 3; p++) {
            yield();
            if (*p == '!') {
              *p = '\0';
              parts[part++] = p + 1;
            }
          }
          if (ci[DEBUG] > 12) {
            textOut(F("before set local hardware\n"));
          }
          setLocalHardwareToServerStateFromNonJson(parts[0]);
          if (ci[DEBUG] > 12) {
            textOut(F("after set local hardware\n"));
          }
          if (part > 1 && strlen(parts[1]) > 5) {
            if (ci[DEBUG] > 3) {
              textOut(F("COMMAND: "));
              textOut(String(parts[1]));
              textOut("\n");
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
            //Serial.println("1:---------------------");
            //Serial.println(cmdBuf);
            runCommand(cmdBuf, false);
          }
          receivedDataJson = true;
          break;
        } else if (first == '!') {
          if (strchr(line, '|')) {
            //instant commands coming from the backend will have a commandId set to -2.  the absolute value of that, 2, ends up being the outputMode
            //Serial.println("2:---------------------");
            //Serial.println(line);
            runCommand(line, false);
            break;
          } else {
            fileUploadPosition = atoi(line + 1);
            //Serial.println(fileUploadPosition);
            File f = LittleFS.open(fileToUpload, "r");
            if (!f) {
              textOut(fileToUpload + F(": file not found\n"));
              fileToUpload = "";
              free(buf);
              return;
            }
            uint32_t totalFileSize = f.size();
            //Serial.print(totalFileSize);
            //Serial.print(" ?> ");
            //Serial.println(fileUploadPosition);
            if (totalFileSize <= fileUploadPosition) {
              textOut(fileToUpload + F(" has finished uploading: "));
              textOut(String(totalFileSize));
              textOut(F(" bytes\n"));
              fileToUpload = "";
            } else {
              String pct = String((100 * fileUploadPosition) / totalFileSize) + "%\n";
              textOut(pct);
            }
            break;
          }
        } else {
          if (ci[DEBUG] > 3) {
            textOut(F("web data: "));
            textOut(String(line));
            textOut("\n");
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
      int loopCount = 0;
      connectionCount++;
      millisecondsConnecting += millis() - millisOneConnection;
      millisOneConnection = 0;
      if(ci[DEBUG] > 10) {
        textOut(F("RS_DONE\n"));
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
  String specificSensorData[12];
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
  int ordinalOfOverwrite[4];
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
      //Serial.println("((((((((((((((((((((((((((((((((");
      //Serial.println(sensorDatum);
      int partCount = splitString(sensorDatum, '*', specificSensorData, 12); //pinNumber*powerPin*sensorId*sensorSubType*i2c*deviceFeatureId*sensorName*ordinalOfOverwrite0*ordinalOfOverwrite1*ordinalOfOverwrite2*ordinalOfOverwrite3*consolidateAllRecords
      pinNumber = specificSensorData[0].toInt();
      powerPin = specificSensorData[1].toInt();
      sensorIdLocal = specificSensorData[2].toInt();
      sensorSubTypeLocal = specificSensorData[3].toInt();
      i2c = specificSensorData[4].toInt();
      deviceFeatureId = specificSensorData[5].toInt();
      sensorName = specificSensorData[6];
      ordinalOfOverwrite[0] = specificSensorData[7].toInt();
      if(partCount > 8) {
        ordinalOfOverwrite[1] = specificSensorData[8].toInt();
        ordinalOfOverwrite[2] = specificSensorData[9].toInt();
        ordinalOfOverwrite[3] = specificSensorData[10].toInt();
        consolidateAllSensorsToOneRecord = specificSensorData[11].toInt();
      } else {
        consolidateAllSensorsToOneRecord = specificSensorData[8].toInt();
        for(int i=1; i<4; i++) {
          ordinalOfOverwrite[i] = -1;
        }
      }
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

bool hardwareControlStringInvalid(const char *str) {
  while (*str) {
    char c = *str;

    bool isValid =
      (c >= 'A' && c <= 'Z') ||   // uppercase
      (c >= 'a' && c <= 'z') ||   // lowercase
      (c >= '0' && c <= '9') ||   // digits
      (c == '*') ||
      (c == ' ') ||
      (c == '.') ||
      (c == ',') ||
      (c == '-');

    if (!isValid) {
      return true; // found a forbidden character
    }

    str++;
  }

  return false; // all characters are acceptable citizens
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
    if(ci[DEBUG] == 7) {
      textOut(F("before hardware loop\n"));
    }
    for (int i = beginLoop; i < endLoop; i++) {
        yield();
        if(ci[DEBUG] == 7) {
          textOut(String(i));
        }
        strncpy(nonJsonDatum, nonJsonPinArray[i], sizeof(nonJsonDatum) - 1);
        if(ci[DEBUG] == 7) {
          textOut(":");
        }
        nonJsonDatum[sizeof(nonJsonDatum) - 1] = '\0';
        if(ci[DEBUG] == 7) {
          textOut("+");
        }
        if (!strchr(nonJsonDatum, '*')){
          continue;
        }

        if(hardwareControlStringInvalid(nonJsonDatum)) { //this will handle almost any junked up control string
          continue;
        }

        if(i > 8) {
          //anomalyLog(String(nonJsonDatum));
          //Serial.println("\n~" + String(nonJsonDatum) + "~");
        }
        if(ci[DEBUG] == 7) {
          textOut("*");
        }

        splitStringToCharArrays(nonJsonDatum, '*', nonJsonPinDatum, 5);
        if(ci[DEBUG] == 7) {
          textOut("|");
        }
        yield();

        // ----------------------------
        // SAFE COPY (DO NOT MUTATE ORIGINAL)
        // ----------------------------
        if(ci[DEBUG] == 7) {
          textOut("-");
        }
        strncpy(pinIdCopy, nonJsonPinDatum[1], sizeof(pinIdCopy) - 1);
        if(ci[DEBUG] == 7) {
          textOut("=");
        }
        pinIdCopy[sizeof(pinIdCopy) - 1] = '\0';
        if(ci[DEBUG] == 7) {
          textOut("^");
        }
        char *dotPos = strchr(pinIdCopy, '.');
        if (dotPos) {
            *dotPos = '\0';
            i2c = atoi(pinIdCopy);
            pinNumber = atoi(dotPos + 1);
        } else {
            i2c = 0;
            pinNumber = atoi(pinIdCopy);
        }
        if(ci[DEBUG] == 7) {
          textOut("$");
        }
        value = atoi(nonJsonPinDatum[2]);
        canBeAnalog = atoi(nonJsonPinDatum[3]);
        serverSaved = atoi(nonJsonPinDatum[4]);
        if(ci[DEBUG] == 7) {
          textOut("@");
        }
        strncpy(friendlyPinName, nonJsonPinDatum[0], sizeof(friendlyPinName) - 1);
        friendlyPinName[sizeof(friendlyPinName) - 1] = '\0';

        // ----------------------------
        // IMPORTANT: KEEP ORIGINAL KEY EXACTLY AS BEFORE
        // ----------------------------
        char *rawKey = nonJsonPinDatum[1];

        pinName[foundPins] = friendlyPinName;
        pinList[foundPins] = rawKey;
        if(ci[DEBUG] == 7) {
          textOut("#");
        }
        // ----------------------------
        // SAFE MAP ACCESS (NO operator[] INSERT SIDE EFFECTS)
        // ----------------------------
        int existingValue = -1;
        auto it = pinMap.find(rawKey);
        if (it != pinMap.end()) {
            existingValue = it->second;
        }
        if(ci[DEBUG] == 7) {
          textOut("(");
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
                if(ci[DEBUG] == 7) {
                  textOut("[");
                }
                
                pinMap.erase(rawKey);
                if(strlen(rawKey) < 8 && (it != pinMap.end()  || initialPinMapSize==0)) { //only insert if initialPinMapSize is 0 or pinMap key was found
                  pinMap.insert({rawKey, value});
                }
                if(ci[DEBUG] == 7) {
                  textOut("]");
                }
                //Serial.println(pinMap.size());
            }
        }
        if(ci[DEBUG] == 7) {
          textOut(")");
        }
        //where we actually change GPIO states on both master and any slaves, but only if there has been an actual change
        if (existingValue != value || resendSlavePinInfo) {
            if (i2c > 0) {
                if(ci[DEBUG] == 7) {
                  textOut(F("before setting slave hardware\n"));
                }
                setPinValueOnSlave(i2c, (char)pinNumber, (char)value);
            } else {
                pinMode(pinNumber, OUTPUT);
                if(ci[DEBUG] == 7) {
                  textOut(F("before setting local hardware\n"));
                }
                if (canBeAnalog) {
                    analogWrite(pinNumber, value);
                } else {
                    digitalWrite(pinNumber, value > 0 ? HIGH : LOW);
                }
            }
        }

        foundPins++;
    }
    if(ci[DEBUG] == 7) {
      textOut(F("about to leave the hardware setting function\n"));
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
  if(commandId < -1) {
    /*
      Serial.println("***********");
      Serial.print(oldOutputMode);
      Serial.print(" ");
      Serial.println(outputMode);
     */
    //this can occasionally switch us to fast commmand mode simply by sending a command
    if(outputMode != abs(commandId)) {
      oldOutputMode = outputMode;
      lastTimeOutputModeChanged = millis();
      /*
      Serial.print("Changing outputmode from ");
      Serial.print(outputMode);
      Serial.print(" to ");
      Serial.println(abs(commandId));
      */
      //Serial.println("OUTPUTMODE CHANGEO5: " + String(outputMode) + " TO " + String(abs(commandId)));
      outputMode = abs(commandId);
    }
    /*
      Serial.print(oldOutputMode);
      Serial.print(" ");
      Serial.println(outputMode);
      */
  }
  String* results[4];
  int resultCount;
  command.trim();
  if(commandId) {
    //Serial.println("------------");
    //Serial.println(command);
    handleCommand(command, deferred);
    if(!deferred && (commandRequiresDeferment(command))) {
      //Serial.println("--------+"  + command + "*-----");
      notYetDeferred(commandText, commandId, (int32_t)(command.indexOf(F("update firmware"))>-1));
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
  int commandNumber = commandCount;//from global//sizeof(commands) / sizeof(commands[0]);
  for (int i = 0; i < commandNumber; i++) {
    yield();
    if (parseCommand(input, commands[i].name, results, resultCount, commands[i].maxArgs)) {
      uint8_t cfg = commands[i].configuration;
      // ?? Capability checks
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
      // ? Deferred requirement check (if you want it enforced)
      if ((cfg & CFG_REQ_DEFER) && !deferred) {
        textOut(F("Error: Command must be deferred\n"));
        return;
      }
      */
      // ? All checks passed, execute
      commands[i].handler(results, resultCount, deferred);
      return;
    }
  }
  textOut(F("Command '") + input + F("' does not exist\n"));
}

void notYetDeferred(const char * commandText, int commandId, int commandType){
  //Serial.println("^^^^^^^^^^^^^^");
  //Serial.print(lastCommandLogId);
  //Serial.print(" $ ");
  //Serial.println(commandId);
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
                    current = numericEquivalents(current);
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
        current = numericEquivalents(current);
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
  int commandNumber = commandCount;
  for (int i = 0; i < commandNumber; i++) {
    yield();

    if (parseCommand(input, commands[i].name, results, resultCount, commands[i].maxArgs)) {
      return (commands[i].configuration & CFG_REQ_DEFER) != 0;
    }
  }
  // Command not found ? no deferment requirement (you could argue either way)
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
/*
//i had wanted to save command state in RTC memory, but it turns out that that memory does not survive firmware updates, so i couldn't use it. 
//this is what the functions would look like if i could use it:
void saveCommandState(uint32_t lastCommandLogId, uint16_t version, int32_t commandId, int32_t commandType) {
    //Serial.println("saving command state");
    rtc.lastCommandLogId = lastCommandLogId;
    rtc.lastVersion = (uint32_t) version;
    rtc.lastCommandId = commandId;
    rtc.lastCommandType = commandType;
    rtcWrite(rtc);
}
    
int32_t loadCommandStateVersion(int dataToReturn) {
    if(dataToReturn == 1){
      return rtc.lastVersion;
    } else if(dataToReturn == 2){
      return rtc.lastCommandId;
    } else if(dataToReturn == 3){
      return rtc.lastCommandType;
    } else {
      return rtc.lastCommandLogId;
    }
}
*/
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
  if(ci[DEBUG] > 2) {
    textOut(F("IR signal sent!\n"));
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
    fsRebootLog("moxee count");
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
  bool useHardcodedConfig = false;
  rtcInitOnBoot();
  Wire.begin();
  if(ci[I2C_SPEED] > 0) {
    Wire.setClock(ci[I2C_SPEED] * 1000);
  }
  yield();
  //Serial.begin(115200);    
  //setSerialRate((byte)ci[BAUD_RATE_LEVEL]); 
  //Serial.setRxBufferSize(1024);
  //Serial.setDebugOutput(true);
  delay(10);
  initMasterDefaults();
  yield();
  setSerialRate((byte)ci[BAUD_RATE_LEVEL]); 
  yield();
  delay(100);
  if(ci[DEBUG] > 0) {
    textOut("\n\n");
  }
  
  //Serial.println("-----------------------");
  //Serial.println(rtc.lastMillis);
  //Serial.println(rtc.rebootCount);
  //Serial.println(rtc.flagValue);
  if (rtc.lastMillis < 5000 && rtc.rebootCount > 1 || rtc.useHardcodedConfig > 0) {
    if(ci[DEBUG] > 0) {
      if(rtc.useHardcodedConfig > 0) {
        textOut(F("Using hardcoded config\n"));
      } else {
        textOut(F("Last uptime was only ") + String(rtc.lastMillis) + F(" milliseconds; entering safe mode\n"));
      }
    }
    // ?? likely boot loop ? skip external config
    useHardcodedConfig = true;
  }
  if(ci[FRAM_ADDRESS] > 0) {
    if (!fram.begin(ci[FRAM_ADDRESS])) {
      if(ci[DEBUG] > 0) {
        textOut(F("Could not find FRAM (or EEPROM).\n"));
      }
    } else {
      if(ci[DEBUG] > 0) {
        textOut(F("FRAM or EEPROM found\n"));
      }
    }
      currentRecordCount = readRecordCountFromFRAM();
      if(lastRecordSize == 0) {
        lastRecordSize = getRecordSizeFromFRAM(0xFFFF);
      }
  }
  rtcUpdateHeartbeat();
  yield();
  int loadResult = -1;
  if(!useHardcodedConfig) {
    loadResult = loadAllConfig(0, 0);
  }
  if(loadResult < 0) {
    if(ci[DEBUG] > 0) {
      if(useHardcodedConfig) {
        textOut(F("In safe mode; using defaults\n"));
      } else{
        textOut(F("No config found anywhere\n"));
      }
      
    }
    initMasterDefaults();
  } else {
    if(ci[DEBUG] > 0) {
      textOut(F("Configuration retrieved from "));
      if(loadResult == CONFIG_PERSIST_METHOD_I2C_SLAVE) {
        textOut(F("slave EEPROM\n"));
      } else if(loadResult == CONFIG_PERSIST_METHOD_FRAM) {
        textOut(F("FRAM\n"));
      } else {
        textOut(F("local flash\n"));
      }
    }
  }
  textOut("Just started up...\n");
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
  
 
  
  //Wire.setClock(50000); // Set I2C speed to 100kHz (default is 400kHz)
  //Wire.setClock(100000);
  if(ci[SENSOR_ID] > 0) {
    startWeatherSensors(ci[SENSOR_ID], ci[SENSOR_SUB_TYPE], ci[SENSOR_I2C], ci[SENSOR_DATA_PIN], ci[SENSOR_POWER_PIN]);
  }
  wiFiConnect();
  server.on("/", handleRoot);      //Displays a form where devices can be turned on and off and the outputs of sensors
  server.on("/readLocalData", localShowData);
  server.on("/weatherdata", handleWeatherData);
  server.on("/writeLocalData", localSetData);
  server.onNotFound(handleFileRequest);
  server.begin(); 
  textOut("HTTP server started\n");

  //startup websocket stuff
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(3000);
  
  
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
        textOut(F("Failed to find INA219 chip\n"));
      }
    } else {
      if(ci[DEBUG] > 0) {
        textOut(F("INA219 chip found\n"));
      }
      ina219->setCalibration_16V_400mA();
    }
  }
  if(ci[IR_PIN] > -1) {
    //irsend.begin(); //do this elsewhere?
  }
  if (!LittleFS.begin()) {
    if(ci[DEBUG] > 1) {
      textOut(F("LittleFS mount failed!\n"));
    }
  } 

  
  if(ci[SERIAL_FOR_COMMANDS_ONLY] == 0) {
    //serialSwap(1); //let's let this be something else
    initSerialParser();
  }
  serialSwap(ci[SERIAL_SWAP]);
  //clearFramLog();
  //displayAllFramRecords();
  //do an initial pet of the watchdog just to set its unix time and make sure it does not bite us

}

//serial.swap() does not take an absolute state like we need it to, so we set or clear the global serialSwapped
void serialSwap(int8_t value) {
  if(!serialSwapped) {
    if(value != 0) {
      serialSwapped = 1;
      Serial.swap();
    }
  } else {
     if(value == 0) {
      serialSwapped = 0;
      Serial.swap();
    }
  }
}

void flashUpdateFeedback(uint32_t nowTime) {
  //this function does nothing but give me feedback and the end of a reboot caused by a flash update
  int32_t preRebootCommandId = loadCommandStateVersion(0);
  uint32_t oldVersion = loadCommandStateVersion(1);
  int32_t oldCommandId = loadCommandStateVersion(2);
  int32_t commandType = loadCommandStateVersion(3);
  String versionMessage;
  String earlyPreamble = F("After reboot: ");
  versionMessage.reserve(80);  // preallocate once
  if(millis() < 10000) {
    versionMessage += earlyPreamble;
  }
  versionMessage += String(F("Version: ")) + VERSION  + "\n";

  if(oldVersion > 0  && commandType > 0) {
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
      if(oldCommandId == -1 || oldCommandId == -3) {
        textOut(versionMessage);
      } else {
        startRemoteTask(versionMessage + preRebootCommandId, "commandout", 0xFFFF);
      }
    } else {
      //Serial.println("gamma");
      String commandToSend = versionMessage + preRebootCommandId;
      if(oldCommandId == -1 || oldCommandId == -3) {
        textOut(versionMessage);
      } else {
        startRemoteTask(commandToSend, "commandout", 0xFFFF);
      }
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
  if(oldCommandId == -3 && deviceName != "" && outputMode != 3){
    startWebSocket(1);
  } 
}


void fsRebootLog(String type) {
  String rebootString;
  String filenameToUse = "reboot.log"; //serialLoggingFileName is a global set by command cmdSetSerialLogging
  rebootString = String(timeClient.getEpochTime()) + ": " + type;
  File file = LittleFS.open(filenameToUse, "a");
  file.println(rebootString);
  file.close();
  rebootString = "";
}

void logAnySerial() {
  static unsigned long lastWrite = 0;
  static String pendingLog;
  String filenameToUse = serialLoggingFileName; //serialLoggingFileName is a global set by command cmdSetSerialLogging
  if(serialLoggingFileName == "") {
    filenameToUse = "serial.log"; //default serial log file name if one is not set
  }
  while (Serial.available()) {
    char serialChar  = Serial.read();
    pendingLog += serialChar;
    serialByteCount++; //update a global
  }
  if (pendingLog.length() > 1024 || (pendingLog.length() > 0 && millis() - lastWrite > 2000)) {
    File file = LittleFS.open(filenameToUse, "a");
    
    file.print(pendingLog);
    file.close();
    pendingLog = "";
    lastWrite = millis();
  }
}

//LOOP----------------------------------------------------
void loop(){
  loopCount++;
  rtcUpdateHeartbeat();
  yield();
  webSocket.loop();
  maintainWebSocket();
  if(serialLogging == 1) {
   logAnySerial(); 
  } else {
    if(ci[SERIAL_FOR_COMMANDS_ONLY] == 1) {
      doSerialCommands();
    } else {
      processSerialStream();
      int millisParseLoopEntered = millis();
 
      if(activeBlock > -1) {
        while(activeBlock > -1  &&  millis() - millisParseLoopEntered < 5000) {//give up on that block after 5 seconds
          processSerialStream();
        }
        activeBlock = -1;  
      }
    }
  }

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
  if(lastCommandLogId > 0 || responseBuffer != "" || fastResponseBuffer != "") {
    if(outputMode == 3) { //the websocket output mode
      if(webSocket.isConnected()) {
        webSocket.sendTXT(fastResponseBuffer);
        fastResponseBuffer = "";
        //have to handle websocket deferred command here
        if (deferredCommand && deferredCommand[0] != '\0') {
          yield();
          runCommand(deferredCommand, true);
          //normally i would think we should also delete deferredCommand, but in this case it always produces a restart
        }
        //lastCommandLogId = 0;//if we had a lastCommandLogId, zero it out. might need to revisit --we revisited and decided not to
      }
    } else {
      if(lastCommandLogId > 0) {
        String stringToSend = responseBuffer + "\n" + lastCommandLogId;
        if(consecutiveCommandOuts > 10) { //too many commandout; something bad happened with current command
          stringToSend = F("Command auto-aborted\n") +  String(lastCommandLogId);
          //Serial.println("....................");
          //Serial.println(stringToSend);
          consecutiveCommandOuts = 0;
        }
        startRemoteTask(stringToSend, "commandout", 0xFFFF);
      } else {
        responseBuffer = "";
      }
    }
 
    
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
      textOut(F("about to compileAndSendDeviceData\n"));
    }
    //this logic here ensures that, if we have a serial parser, sending weather data isn't attempted until after some serial data has been received. then, if serial parsing is going on for more than 20 seconds,
    //we switch back to sending weather data. the baton is passed back and forth 
    //between serial and sending weather data via didSomeSerialProcessing
    if(deviceName == ""  || ci[SERIAL_FOR_COMMANDS_ONLY] == 1 || didSomeSerialProcessing || (nowTime - lastPoll)/1000 > 20 ) { //if it lingers more than 20 seconds parsing serial, force a poll
      bool sentARecord = false;
      if(ci[FRAM_ADDRESS] > 0){
        //dumpMemoryStats(101);
        if(haveReconnected) {
          sendAStoredRecordToBackend();
          haveReconnected = false;
          sentARecord = true;
        }
      }  
      if(!sentARecord) {
        compileAndSendDeviceData("", "", "", true, 0xFFFF);
      }


      //zeta
      /*
      Serial.print("lastcommandLogid: ");
      Serial.print(lastCommandLogId);
      Serial.print("; old outputmode: ");
      Serial.print(oldOutputMode);
      Serial.print("; current outputmode: ");
      Serial.print(outputMode);
      Serial.print("; how stale: ");
      Serial.println(millis()-lastTimeOutputModeChanged );
      */
      //the following code helps unstick us from outputMode 2 in some cases
      if(lastCommandLogId == 0 && outputMode == 2) {
        //Serial.println("OUTPUTMODE CHANGEO6: " + String(outputMode) + " TO " + String(oldOutputMode));
        outputMode = oldOutputMode;
        lastTimeOutputModeChanged = millis();
      }
      lastPoll = nowTime;
      didSomeSerialProcessing = false;
    }   
    
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
  
 
  yield();
  if(ci[DEBUG] > 20) {
    textOut(F("about to flashUpdateFeedback\n"));
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
    serialByteCount++;
    if (c == '\r' || c == '\n') {
      if (command.length() > 0) {
        if (ci[DEBUG] > 1) {
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
      if(ci[DEBUG] > 1) {
        textOut(F("LOCAL SOURCE TRUE :"));
        textOut(String(onValue));
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

/////////////////////////////////////////////
//config routines
/////////////////////////////////////////////
//returns -1 if config not found or invalid, otherwise returns persist method (which might be 0 and valid if Flash)
int loadAllConfig(int mode, uint16_t param){
  if(ci[CONFIG_PERSIST_METHOD] == CONFIG_PERSIST_METHOD_I2C_SLAVE) {
    return loadAllConfigFromEEPROM(mode, param);
  } else if (ci[CONFIG_PERSIST_METHOD] == CONFIG_PERSIST_METHOD_FLASH) {
    return loadAllConfigFromFlash(mode, param);
  } else if (ci[CONFIG_PERSIST_METHOD] == CONFIG_PERSIST_METHOD_FRAM) {
    return loadAllConfigFromFRAM(mode, param);
  } else {
    return -1;
  }
  return -1;
}


/////////////////////////////////////////////
//processor-specific
/////////////////////////////////////////////
void cleanup(){
  ESP.getFreeHeap(); // This sometimes triggers internal cleanup
  yield();    
}

int freeMemory() {
    return ESP.getFreeHeap();
}



//////////////////////////////////////////////
//parsing serial:



int initSerialParser() {
  //char cfg[MAX_BLOCKS][MAX_CFG_LEN];
  File file = LittleFS.open("/serialparser.cfg", "r");
  if (!file) {
    if(ci[DEBUG] > 0) {
      textOut("Failed to open serial parser config for reading\n");
      return 0;
    }
  }
  while (file.available()) {
    String line = file.readStringUntil('\n');
    parseConfigString(line.c_str(), blocks[blockCount]);
    //dumpConfigBlock(blocks[i]);
    blockCount++;
  }
  return 1;
}
 
//only for debugging:
void dumpConfigBlock(const ConfigBlock &b) {
  textOut(F("=== ConfigBlock ===\n"));
  textOut(F("start: "));
  textOut(String(b.start));
  textOut("\n");
  textOut(F("end:   "));
  textOut(String(b.end));
  textOut("\n");
  textOut(F("addrCount: "));
  textOut(String(b.addrCount));
  textOut("\n");
  for (uint8_t i = 0; i < b.addrCount; i++) {
    textOut(F("  addr["));
    textOut(String(i));
    textOut(F("]: "));
    textOut(String(b.addr[i]));

    textOut(F("    offsets ("));
    textOut(String(b.offsetCount[i]));
    textOut(F("): "));

    for (uint8_t j = 0; j < b.offsetCount[i]; j++) {
      textOut(String(b.offsets[i][j]));
      if (j + 1 < b.offsetCount[i]) {
        textOut(F(", "));
      }
    }
    textOut("\n");
  }

  textOut(F("===================\n"));
}

void parseConfigString(const char *cfg, ConfigBlock &out) {
  char buf[128];
  //Serial.println(cfg);
  //Serial.println(strlen(cfg));
  strncpy(buf, cfg, sizeof(buf));
  buf[sizeof(buf) - 1] = 0;

  char *tok = strtok(buf, ";");
  if (!tok) {
    return;
  }
  strncpy(out.start, tok, sizeof(out.start));
  //Serial.println("-----");
  //Serial.println( out.start);
  //Serial.println( out.end);
  tok = strtok(NULL, ";");
  if (!tok) return;
  strncpy(out.end, tok, sizeof(out.end));

  out.addrCount = 0;
  uint8_t curAddr = 255;

  while ((tok = strtok(NULL, ";"))) {
    trim(tok);
    //Serial.println(tok);
    if (!isInteger(tok)) { //any token in the config that is not explicitly a decimal integer is a string to be searched for
      curAddr = out.addrCount++;
      
      //Serial.println(tok);
      strncpy(out.addr[curAddr], tok, sizeof(out.addr[curAddr]));
      out.offsetCount[curAddr] = 0;
    } else if (curAddr != 255) {
      //Serial.print(out.offsetCount[curAddr] );
      //Serial.print(": ");
      //Serial.println(tok );
      if (out.offsetCount[curAddr] < MAX_OFFSETS) {
        out.offsets[curAddr][out.offsetCount[curAddr]++] = atoi(tok);
        //increment the global containing the parsed packet size. since each 16 bit value in the packet gets a pair of offsets, 
        //if we increment with every offset we get the correct number of bytes in the packet 
        //Serial.println(parsedStringPacketLen);
      }
    }
  }
  //dumpConfigBlock(out);
  //delay(100);
}

void trim(char *s) {
  // trim leading whitespace
  while (isspace((unsigned char)*s)) {
    memmove(s, s + 1, strlen(s));
  }
  // trim trailing whitespace
  int len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[len - 1])) {
    s[--len] = '\0';
  }
}


// Append 16-bit little-endian to global buffer
inline void appendU16(uint16_t v,
                      uint16_t bytePacketStart,
                      int8_t  blockIdx,
                      uint8_t addrIdx,
                      uint8_t offsetIdx,
                      uint8_t off1,
                      uint8_t off2,
                      uint8_t byteCount)
{
  //Serial.print(parsedStringPacketLen + 2);
  //Serial.print(": ");
  //Serial.print(PARSED_SERIAL_MAX);

  uint8_t lo = (uint8_t)(v & 0xFF);
  uint8_t hi = (uint8_t)(v >> 8);
  serialParsedData[bytePacketStart/2] = v;
  serialDataParsed++;
  logParseOperation(F("assembling packet"), F("bytePacketStart: ") + String(bytePacketStart) + String("\nblockIdx: ") + String(blockIdx)  + String(", addrIdx: ") + String(addrIdx) + String(F(", offsetIdx: ")) + String(offsetIdx) + String(F(", value: ")) + String(v) + String(F(", hi: ")) + String(hi), 49);
  /*
  // ---- DEBUG TRACE ----
  Serial.print(F("[PARSE] blk="));
  Serial.print(blockIdx);

  Serial.print(F(" loc="));
  Serial.print(bytePacketStart);

  Serial.print(F(" addr="));
  Serial.print(addrIdx);

  Serial.print(F(" offIdx="));
  Serial.print(offsetIdx);

  Serial.print(F(" offs="));
  Serial.print(off1);
  if (off1 != off2) {
    Serial.print(',');
    Serial.print(off2);
  }

  Serial.print(F(" / "));
  Serial.print(byteCount);

  Serial.print(F("  bytes="));
  if (lo < 16) Serial.print('0');
  Serial.print(lo, HEX);
  Serial.print(' ');
  if (hi < 16) Serial.print('0');
  Serial.print(hi, HEX);

  Serial.print(F("  val="));
  Serial.println(v);
  */
}



// Fill bytes[] with numeric values from a line, ignoring text tokens
uint8_t extractHexBytes(const char *line,
                             uint8_t *out,
                             uint8_t maxOut)
{
  uint8_t count = 0;
  while (*line && count < maxOut) {
    // must start with hex digit
    if (isxdigit(line[0]) &&
        isxdigit(line[1]) &&
        // word boundary before
        (line == 0 || !isxdigit(line[-1])) &&
        // word boundary after
        !isxdigit(line[2]))
    {
      char tmp[3] = { line[0], line[1], 0 };
      out[count++] = (uint8_t)strtoul(tmp, nullptr, 16);
      line += 2;
    } else {
      line++;
    }
  }
  return count;
}



bool readSerialLine(char *line, size_t maxLen)
{
    static size_t idx = 0;
    static bool overflow = false;
    if (!line || maxLen < 2) {
        return false;
    }
    while (Serial.available()) {
        char c = Serial.read();
        serialByteCount++; //update a global
        // Ignore carriage returns
        if (c == '\r') {
            continue;
        }
        // End of line
        if (c == '\n') {
            // Always terminate safely
            line[idx] = '\0';
            bool validLine = !overflow;
            // Reset parser state
            idx = 0;
            overflow = false;
            return validLine;
        }
        // If already overflowed, discard until newline
        if (overflow) {
            continue;
        }
        // Normal append
        if (idx < (maxLen - 1)) {
            line[idx++] = c;
        } else {
            // Buffer full
            overflow = true;
            // Force safe termination
            line[maxLen - 1] = '\0';
        }
    }
    return false;
}

bool readValueAtOffset(
    const char *line,
    const char *addrStr,
    uint8_t off1,
    uint8_t off2,
    uint8_t parsingStyle,
    uint16_t &outValue)
{
  outValue = 0;
  const char *addrPos = strstr(line, addrStr);
  if (!addrPos) {
    return false;
  }

  addrPos += strlen(addrStr);

  const char *p1 = nullptr;
  const char *p2 = nullptr;

  /* ---- OFFSET RESOLUTION ---- */

  if (parsingStyle & PS_CHAR_OFFSET) {
    // character-based offset
    p1 = addrPos + off1;
    p2 = addrPos + off2;
  } else {
    // token-based offset
    p1 = findNthToken(addrPos, off1);
    p2 = findNthToken(addrPos, off2);
  }

  if (!p1) {
    return false;
  }
  if (off2 != off1 && !p2) {
    return false;
  }
  uint8_t b0 = 0;
  uint8_t b1 = 0;

  /* ---- VALUE INTERPRETATION ---- */

  if (parsingStyle & PS_ASCII_VALUE) {
    // raw ASCII characters
    b0 = (uint8_t)p1[0];
    b1 = (off2 != off1 && p2) ? (uint8_t)p2[0] : 0;
  } else {
    // hex interpretation
    if (!hexByteAt(p1, b0)) {
      return false;
    }
    if (off2 != off1 && !hexByteAt(p2, b1)) return false;
  }

  /* ---- ENDIANNESS ---- */

  if (off1 == off2) {
    outValue = b0;
  } else if (parsingStyle & PS_BIG_ENDIAN) {
    outValue = ((uint16_t)b0 << 8) | b1;
  } else {
    outValue = ((uint16_t)b1 << 8) | b0;
  }
 
  logParseOperation(F("reading value at offset"), F("line: ") + String(line) + String("\nparsingStyle: ") + String(parsingStyle)  + String(", addr: ") + String(addrStr) + String(F(", off1: ")) + String(off1) + String(F(", off2: ")) + String(off2) + String(F(", outVal: ")) + String(outValue), 49);
  return true;
}

bool hexByteAt(const char *p, uint8_t &out)
{
  if (!isxdigit(p[0]) || !isxdigit(p[1])) {
    return false;
  }
  char tmp[3] = { p[0], p[1], 0 };
  out = (uint8_t)strtoul(tmp, nullptr, 16);
  return true;
}

const char *findNthToken(const char *s, uint8_t n){
  uint8_t count = 0;
  while (*s) {
    while (*s == ' ') s++;
    if (!*s) break;
    if (count++ == n) {
      return s;
    }
    while (*s && *s != ' ') s++;
  }
  return nullptr;
}

void logParseOperation(String type, String value, int threshold) {
  if(ci[SERIAL_DEBUG_LEVEL] > threshold  && activeBlock > -1  || ci[SERIAL_DEBUG_LEVEL] > 109) {
    String filenameToUse = F("parse.log");
    File file = LittleFS.open(filenameToUse, "a");
    String lineToLog = type + ": " + value;
    file.println(lineToLog);
    file.close();
  }
}


void processSerialStream()
{
  static char line[256];
  while (readSerialLine(line, sizeof(line))) {
    bool nextWhileIteration = false;
    logParseOperation(F("lineToParse"), line, 99);
    /* ---- BLOCK START DETECTION ---- */
    //logParseOperation(F("event"), F("blockCount: ") + String(blockCount), 99);
    for (uint8_t i = 0; i < blockCount; i++) {
      logParseOperation(F("event"), F("tryParse: ") + String(blocks[i].start), 119);
      if (strlen(blocks[i].start) > 0 && strstr(line, blocks[i].start)) {
  
        activeBlock = i;
        logParseOperation(F("event"), F("start: ") + String(blocks[i].start) + F(" found"), 99);
        nextWhileIteration = true;
        break;
        //return;   // wait for data lines
      }
    }
  
    if (activeBlock < 0) {
      logParseOperation(F("event"), F("nothing active"), 99);
      continue;
      //return;
    }
  
    /* ---- BLOCK END DETECTION ---- */
    if (strlen(blocks[activeBlock].end) > 0 && strstr(line, blocks[activeBlock].end)) {
      logParseOperation(F("event"), F("ending found on active block"), 99);
      activeBlock = -1;
      continue;
      //return;
    }
  
    /* ---- ADDRESS LINES ---- */
    ConfigBlock &blk = blocks[activeBlock];
    for (uint8_t a = 0; a < blk.addrCount; a++) {
      // fast reject if address not present
      if (!strstr(line, blk.addr[a])) {
        logParseOperation(F("event"), F("address: ") + String(blk.addr[a]) + F(" not found in line"), 99);
        continue;
      }
      /* ---- OFFSET PROCESSING ---- */
      for (uint8_t o = 0; o + 1 < blk.offsetCount[a]; o += 2) {
        uint8_t off1 = blk.offsets[a][o];
        uint8_t off2 = blk.offsets[a][o + 1];
  
        uint16_t v;
  
        if (!readValueAtOffset(line, blk.addr[a], off1, off2, ci[SERIAL_PARSE_MODE], v)){
          logParseOperation(F("event"), String(off1) + ", " + String(off2) + ": " + F("nothing found between offsets"), 99);
          continue;   // parse failed for this offset pair
        }
        lastDataParseMillis = millis();
        logParseOperation(F("event"), String(off1) + ", " + String(off2) + ": " + F("good things"), 99);
        uint16_t parsedOutIndex = calculateOffsetIndex(blocks, blockCount, activeBlock, a);
        logParseOperation(F("got an offset"),  F("Offset: " ) + String(parsedOutIndex) + F(", o: ") + String(o), 49);
        appendU16(v, parsedOutIndex + o, (int8_t)activeBlock,
          a,      // address index
          o,      // offset rule index
          off1,
          off2,
          0       // byteCount no longer relevant here
        );
        didSomeSerialProcessing = true;
      }
    }
    if (nextWhileIteration) {
        continue;
    }
  }
}

uint16_t calculateOffsetIndex(const ConfigBlock *blocks, uint8_t blockCount, uint8_t blk, uint8_t addr)
{
  uint16_t sum = 0;
  // 1. Sum all offsets in all previous blocks
  for (uint8_t b = 0; b < blk && b < blockCount; b++) {
    for (uint8_t a = 0; a < blocks[b].addrCount; a++) {
      sum += blocks[b].offsetCount[a];
    }
  }
  // 2. Sum offsets in previous addresses of this block
  if (blk < blockCount) {
    for (uint8_t a = 0; a < addr && a < blocks[blk].addrCount; a++) {
      sum += blocks[blk].offsetCount[a];
    }
  }
  return sum;
}
 
