 
#include "config.h"
#include "globals.h"
#include "i2cslave.h"
#include "utilities.h"
#include "slaveupdate.h"
#include "rootfunctions.h"
#include "filefunctions.h"
#include "framfunctions.h"

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

#include <Arduino.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266WebServer.h>




#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <Adafruit_INA219.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_FRAM_I2C.h>
 

#include <tuple>
#include <cmath> 
#include <math.h> // for isnan, NAN

#include <LittleFS.h>

void cmdDeferredReboot(String* param, int argCount, bool deferred);
void cmdLocalUpdateFirmware(String* param, int argCount, bool deferred); 
void cmdUpdateFirmware(String* param, int argCount, bool deferred);

void cmdInitSensors(String* param, int argCount, bool deferred);
void cmdVersion(String* param, int argCount, bool deferred);
void cmdRunSlaveSketch(String* param, int argCount, bool deferred);
void cmdRunSlaveBootloader(String* param, int argCount, bool deferred);
void cmdPetWatchdog(String* param, int argCount, bool deferred);
void cmdGetWeatherSensors(String* param, int argCount, bool deferred);
void cmdRebootEsp(String* param, int argCount, bool deferred);
void cmdOnePinAtATime(String* param, int argCount, bool deferred);
void cmdClearLatencyAverage(String* param, int argCount, bool deferred);
void cmdIr(String* param, int argCount, bool deferred);
void cmdClearFram(String* param, int argCount, bool deferred);
void cmdDumpFram(String* param, int argCount, bool deferred);
void cmdDumpFramHex(String* param, int argCount, bool deferred);
void cmdDumpFramHexAt(String* param, int argCount, bool deferred);
void cmdSwapFram(String* param, int argCount, bool deferred);
void cmdDumpFramRecord(String* param, int argCount, bool deferred);
void cmdGetFramIndex(String* param, int argCount, bool deferred);
void cmdRebootSlave(String* param, int argCount, bool deferred);
void cmdRebootMasterFromSlave(String* param, int argCount, bool deferred);
void cmdSetDate(String* param, int argCount, bool deferred);
void cmdGetDate(String* param, int argCount, bool deferred);
void cmdGetWatchdogInfo(String* param, int argCount, bool deferred);
void cmdGetWatchdogData(String* param, int argCount, bool deferred);
void cmdListFiles(String* param, int argCount, bool deferred);
void cmdSaveMasterConfig(String* param, int argCount, bool deferred);
void cmdSaveSlaveConfig(String* param, int argCount, bool deferred);
void cmdInitMasterDefaults(String* param, int argCount, bool deferred);
void cmdInitSlaveDefaults(String* param, int argCount, bool deferred);
void cmdGetUptime(String* param, int argCount, bool deferred);
void cmdGetWifiUptime(String* param, int argCount, bool deferred);
void cmdGetLastpoll(String* param, int argCount, bool deferred);
void cmdGetLastdatalog(String* param, int argCount, bool deferred);
void cmdMemory(String* param, int argCount, bool deferred);
void cmdDumpSerialPacket(String* param, int argCount, bool deferred);
void cmdFormatFileSystem(String* param, int argCount, bool deferred);

void cmdAnomalyLogTest(String* param, int argCount, bool deferred);
void cmdRenameFile(String* param, int argCount, bool deferred);
void cmdDel(String* param, int argCount, bool deferred);
void cmdDownload(String* param, int argCount, bool deferred);
void cmdUpload(String* param, int argCount, bool deferred);
void cmdCat(String* param, int argCount, bool deferred);
void cmdReadSlaveEeprom(String* param, int argCount, bool deferred);
void cmdResetSerial(String* param, int argCount, bool deferred);
void cmdConfigEeprom(String* param, int argCount, bool deferred);
void cmdDumpSlaveEeprom(String* param, int argCount, bool deferred);
void cmdSendSlaveSerial(String* param, int argCount, bool deferred);
void cmdSetSlaveTime(String* param, int argCount, bool deferred);
void cmdGetSlaveTime(String* param, int argCount, bool deferred);
void cmdInitSlaveSerial(String* param, int argCount, bool deferred);
void getSlaveSerial(String* param, int argCount, bool deferred);
void getSlaveParsedDatum(String* param, int argCount, bool deferred);
void updateSlaveFirmware(String* param, int argCount, bool deferred);
void getMasterEepromUsed(String* param, int argCount, bool deferred);
void getSlaveEepromUsed(String* param, int argCount, bool deferred);
void getSlave(String* param, int argCount, bool deferred);
void setSlaveParserBasis(String* param, int argCount, bool deferred);
void setSlaveBasis(String* param, int argCount, bool deferred);
void setSlave(String* param, int argCount, bool deferred);
void runSlave(String* param, int argCount, bool deferred);
void cmdSet(String* param, int argCount, bool deferred);
void cmdGet(String* param, int argCount, bool deferred);
