#ifndef GLOBALS_H
#define GLOBALS_H

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

// Sensor objects
extern Adafruit_ADT7410 adt7410[4];
extern DHT* dht[4];
extern Adafruit_AHTX0 AHT[2];
extern SFE_BMP180 BMP180[2];
extern BME680_Class BME680[2];
extern Adafruit_BMP085 BMP085d[2];
extern Generic_LM75 LM75[2];
extern Adafruit_BMP280 BMP280[2];
extern IRsend irsend; // note: IRsend requires a pin in constructor, can handle in cpp
extern Adafruit_INA219* ina219;
extern Adafruit_VL53L0X lox[4];
extern Adafruit_FRAM_I2C fram;

//state machine globals

enum RemoteState {
  RS_IDLE,
  RS_PREPARE,            // construct URL + init
  RS_CONNECTING,
  RS_CONNECT_WAIT,       // spacing between connect attempts
  RS_SENDING_REQUEST,
  RS_WAITING_FOR_REPLY,  // waiting for any data (with timeout)
  RS_READING_REPLY,      // drain available bytes into buffer
  RS_SOCKET_CLOSED,      // socket closed by remote or finished reading
  RS_PROCESSING_REPLY,   // now do the heavy processing (commands, FRAM, etc.)
  RS_DONE
};


extern const unsigned long CONNECT_RETRY_SPACING_MS;
extern const unsigned long CONNECT_TIMEOUT_MS;
extern const unsigned long REPLY_AVAIL_TIMEOUT_MS;
extern const unsigned long MAX_RESPONSE_SIZE;


// Other globals
extern uint16_t framIndexAddress;
extern uint16_t currentRecordCount;

extern WiFiUDP ntpUDP;
extern NTPClient timeClient;

extern bool localSource;
extern byte justDeviceJson;
extern unsigned long connectionFailureTime;
extern unsigned long lastDataLogTime;
extern unsigned long localChangeTime;
extern unsigned long lastPoll;

extern int pinTotal;
extern String pinList[12];
extern String pinName[12];
extern String ipAddress;
extern String ipAddressAffectingChange;
extern int changeSourceId;
extern String deviceName;
extern String additionalSensorInfo;
extern float measuredVoltage;
extern float measuredAmpage;
extern bool canSleep;
extern bool deferredCanSleep;
extern long latencySum;
extern long latencyCount;
extern bool offlineMode;
extern unsigned long lastOfflineLog;
extern uint8_t lastRecordSize;

extern unsigned long lastOfflineReconnectAttemptTime;
extern bool haveReconnected;
extern uint16_t fRAMRecordsSkipped;
extern uint32_t lastRtcSyncTime;
extern uint32_t wifiOnTime;
extern bool debug;
extern uint8_t outputMode;
extern String responseBuffer;
extern unsigned long lastPet;
extern int currentWifiIndex;

extern SimpleMap<String, int> *pinMap;
extern SimpleMap<String, int> *sensorObjectCursor;

extern uint32_t moxeeRebootTimes[];
extern int moxeeRebootCount;
extern int timeOffset;
extern long lastCommandId;
extern char * deferredCommand;

extern bool onePinAtATimeMode;
extern char requestNonJsonPinInfo; 
extern int pinCursor;
extern bool connectionFailureMode;

extern int knownMoxeePhase;
extern int moxeePhaseChangeCount;
extern uint32_t lastCommandLogId;

extern ESP8266WebServer server;

#endif
