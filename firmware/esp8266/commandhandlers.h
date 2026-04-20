 
#include "config.h"
#include "globals.h"
#include "i2cslave.h"
#include "utilities.h"
#include "slaveupdate.h"
#include "rootfunctions.h"


#include <Arduino.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266WebServer.h>
#include <Adafruit_ADT7410.h>
#include <DHT.h>
#include <Adafruit_AHTX0.h>
#include <SFE_BMP180.h>
#include "Zanshin_BME680.h"  // Include the BME680 Sensor library
#include <Adafruit_BMP085.h>
#include <Temperature_LM75_Derived.h>
#include <Adafruit_BMP280.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <Adafruit_INA219.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_FRAM_I2C.h>
#include "SimpleMap.h" // assuming you have this header

#include <tuple>
#include <cmath> 
#include <math.h> // for isnan, NAN

#include <LittleFS.h>



void cmdVersion(String* param, int argCount);
void cmdRunSlaveSketch(String* param, int argCount);
void cmdRunSlaveBootloader(String* param, int argCount);
void cmdPetWatchdog(String* param, int argCount);
void cmdGetWeatherSensors(String* param, int argCount);
void cmdRebootEsp(String* param, int argCount);
void cmdOnePinAtATime(String* param, int argCount);
void cmdClearLatencyAverage(String* param, int argCount);
void cmdIr(String* param, int argCount);
void cmdClearFram(String* param, int argCount);
void cmdDumpFram(String* param, int argCount);
void cmdDumpFramHex(String* param, int argCount);
void cmdDumpFramHexAt(String* param, int argCount);
void cmdSwapFram(String* param, int argCount);
void cmdDumpFramRecord(String* param, int argCount);
void cmdGetFramIndex(String* param, int argCount);
void cmdRebootSlave(String* param, int argCount);
void cmdSetDate(String* param, int argCount);
void cmdGetDate(String* param, int argCount);
void cmdGetWatchdogInfo(String* param, int argCount);
void cmdGetWatchdogData(String* param, int argCount);
void cmdListFiles(String* param, int argCount);
void cmdSaveMasterConfig(String* param, int argCount);
void cmdSaveSlaveConfig(String* param, int argCount);
void cmdInitMasterDefaults(String* param, int argCount);
void cmdInitSlaveDefaults(String* param, int argCount);
void cmdGetUptime(String* param, int argCount);
void cmdGetWifiUptime(String* param, int argCount);
void cmdGetLastpoll(String* param, int argCount);
void cmdGetLastdatalog(String* param, int argCount);
void cmdMemory(String* param, int argCount);
void cmdDumpSerialPacket(String* param, int argCount);
void cmdFormatFileSystem(String* param, int argCount);



void cmdDel(String* param, int argCount);
void cmdDownload(String* param, int argCount);
void cmdUpload(String* param, int argCount);
void cmdCat(String* param, int argCount);
void cmdReadSlaveEeprom(String* param, int argCount);
void cmdResetSerial(String* param, int argCount);
void cmdConfigEeprom(String* param, int argCount);
void cmdDumpSlaveEeprom(String* param, int argCount);
void cmdSendSlaveSerial(String* param, int argCount);
void cmdSetSlaveTime(String* param, int argCount);
void cmdGetSlaveTime(String* param, int argCount);
void cmdInitSlaveSerial(String* param, int argCount);
void getSlaveSerial(String* param, int argCount);
void getSlaveParsedDatum(String* param, int argCount);
void updateSlaveFirmware(String* param, int argCount);
void getMasterEepromUsed(String* param, int argCount);
void getSlaveEepromUsed(String* param, int argCount);
void getSlave(String* param, int argCount);
void setSlaveParserBasis(String* param, int argCount);
void setSlaveBasis(String* param, int argCount);
void setSlave(String* param, int argCount);
void runSlave(String* param, int argCount);
void cmdSet(String* param, int argCount);
void cmdGet(String* param, int argCount);
