/*
 * ESP8266 Remote Control. Also sends weather data from multiple kinds of sensors (configured in config.c) 
 * originally built on the basis of something I found on https://circuits4you.com
 * reorganized and extended by Gus Mueller, April 24 2022 - June 22 2024
 * Also resets a Moxee Cellular hotspot if there are network problems
 * since those do not include watchdog behaviors
 */

 //note:  this needs to be compiled in the Arduino environment alongside:
 //config.cpp
 //config.h
 //i2cslave.cpp
 //i2cslave.h
 //index.h
 
#include "i2cslave.h"


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
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

#include "index.h" //Our HTML webpage contents with javascriptrons



//i created 12 of each sensor object in case we added lots more sensors via device_features
//amusingly, this barely ate into memory at all
//since many I2C sensors only permit two sensors per I2C bus, you could reduce the size of these object arrays
//and so i've dropped some of these down to 2
Adafruit_ADT7410 adt7410[4];
DHT* dht[4];
Adafruit_AHTX0 AHT[2];
SFE_BMP180 BMP180[2];
BME680_Class BME680[2];
Adafruit_BMP085 BMP085d[2];
Generic_LM75 LM75[2];
Adafruit_BMP280 BMP280[2];
IRsend irsend(ci[IR_PIN]);
Adafruit_INA219* ina219;
Adafruit_VL53L0X lox[4];
Adafruit_FRAM_I2C fram;

uint16_t framIndexAddress = 0;   
uint16_t  currentRecordCount = 0;

WiFiUDP ntpUDP; //i guess i need this for time lookup
NTPClient timeClient(ntpUDP, "pool.ntp.org");

bool localSource = false; //turns true when a local edit to the data is done. at that point we have to send local upstream to the server
byte justDeviceJson = 1;
unsigned long connectionFailureTime = 0;
unsigned long lastDataLogTime = 0;
unsigned long localChangeTime = 0;
unsigned long lastPoll = 0;
 
int pinTotal = 12;
String pinList[12]; //just a list of pins
String pinName[12]; //for friendly names
String ipAddress;
String ipAddressAffectingChange;
int changeSourceId = 0;
String deviceName = "";
String additionalSensorInfo; //we keep it stored in a delimited string just the way it came from the server and unpack it periodically to get the data necessary to read sensors
float measuredVoltage = 0;
float measuredAmpage = 0;
bool canSleep = false;
bool deferredCanSleep = false;
long latencySum = 0;
long latencyCount = 0;
bool offlineMode = false;
unsigned long lastOfflineLog = 0;
uint8_t lastRecordSize = 0;

unsigned long lastOfflineReconnectAttemptTime = 0;
bool haveReconnected = true; //we need to start reconnected in case we reboot and there are stored FRAM records
uint16_t fRAMRecordsSkipped = 0;
uint32_t lastRtcSyncTime = 0;
uint32_t wifiOnTime = 0;
bool debug = true;
uint8_t outputMode = 0;
String responseBuffer = "";
static unsigned long lastPet = 0;
int currentWifiIndex = 0;

//https://github.com/spacehuhn/SimpleMap
SimpleMap<String, int> *pinMap = new SimpleMap<String, int>([](String &a, String &b) -> int {
  if (a == b) return 0;      // a and b are equal
  else if (a > b) return 1;  // a is bigger than b
  else return -1;            // a is smaller than b
});
SimpleMap<String, int> *sensorObjectCursor = new SimpleMap<String, int>([](String &a, String &b) -> int {
  if (a == b) return 0;      // a and b are equal
  else if (a > b) return 1;  // a is bigger than b
  else return -1;            // a is smaller than b
});

uint32_t moxeeRebootTimes[] = {0,0,0,0,0,0,0,0,0,0,0};
int moxeeRebootCount = 0;
int timeOffset = 0;
long lastCommandId = 0;
char * deferredCommand = "";
 
bool onePinAtATimeMode = false; //used when the server starts gzipping data and we can't make sense of it
char requestNonJsonPinInfo = 1; //use to get much more compressed data double-delimited data from data.php if 1, otherwise if 0 it requests JSON
int pinCursor = -1;
bool connectionFailureMode = true;  //when we're in connectionFailureMode, we check connection much more than ci[POLLING_GRANULARITY]. otherwise, we check it every ci[POLLING_GRANULARITY]

int knownMoxeePhase = -1;  //-1 is unknown. 0 is stupid "show battery level", 1 is operational
int moxeePhaseChangeCount = 0;
uint32_t lastCommandLogId = 0;

ESP8266WebServer server(80); //Server on port 80

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

// Safe, non-fragmenting rewrite of weatherDataString()
// NOTE: This returns a String at the end only to keep compatibility with the rest of your code.
// That single String alloc does not fragment the heap like repeated concatenations did.
String weatherDataString(
  int sensorId, int sensorSubtype, int dataPin, int powerPin, int i2c,
  int deviceFeatureId, char objectCursor, String sensorName,
  int ordinalOfOverwrite, int consolidateAllSensorsToOneRecord
) {
  // --- locals ---
  double humidityFromSensor = NAN;
  double temperatureFromSensor = NAN;
  double pressureFromSensor = NAN;
  double gasFromSensor = NAN;
  String sensorValueStr = "";

  if (deviceFeatureId == 0) objectCursor = 0;
  
  // sensor-reading branches (unchanged)
  if (ci[SENSOR_ID] == 1) {
    if (powerPin > -1) digitalWrite(powerPin, HIGH);
    delay(10);
    if (i2c) sensorValueStr = String(getPinValueOnSlave((char)i2c, (char)dataPin));
    else sensorValueStr = String(analogRead(dataPin));
    if (powerPin > -1) digitalWrite(powerPin, LOW);
  } else if (ci[SENSOR_ID] == 53) {
    VL53L0X_RangingMeasurementData_t measure;
    lox[objectCursor].rangingTest(&measure, false);
    if (measure.RangeStatus != 4) sensorValueStr = String(measure.RangeMilliMeter);
    else sensorValueStr = "-1";
  } else if (sensorId == 680) {
    int32_t humidityRaw = 0, temperatureRaw = 0, pressureRaw = 0, gasRaw = 0;
    BME680[objectCursor].getSensorData(temperatureRaw, humidityRaw, pressureRaw, gasRaw);
    humidityFromSensor = (double)humidityRaw / 1000.0;
    temperatureFromSensor = (double)temperatureRaw / 100.0;
    pressureFromSensor = (double)pressureRaw / 100.0;
    gasFromSensor = (double)gasRaw / 100.0;
  } else if (sensorId == 2301) {
    if (powerPin > -1) digitalWrite(powerPin, HIGH);
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
  for (int i = 0; i < FIELDS; ++i) fields[i][0] = '\0';

  // fill fields (check bounds)
  if (!isnan(temperatureFromSensor)) snprintf(fields[0], FIELD_MAX, "%.2f", temperatureFromSensor);
  if (!isnan(pressureFromSensor))    snprintf(fields[1], FIELD_MAX, "%.2f", pressureFromSensor);
  if (!isnan(humidityFromSensor))    snprintf(fields[2], FIELD_MAX, "%.2f", humidityFromSensor);
  if (!isnan(gasFromSensor))         snprintf(fields[3], FIELD_MAX, "%.2f", gasFromSensor);

  if (ordinalOfOverwrite >= 0 && ordinalOfOverwrite < FIELDS) {
    String useVal = sensorValueStr;
    if (useVal.length() == 0) {
      if (sensorSubtype == 1) useVal = String(pressureFromSensor);
      else if (sensorSubtype == 2) useVal = String(humidityFromSensor);
      else if (sensorSubtype == 3) useVal = String(gasFromSensor);
      else useVal = String(temperatureFromSensor);
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
      if (pos + 1 < bufSize) tx[pos++] = '*';
      else { tx[bufSize - 1] = '\0'; return String(tx); }
    }
    if (fields[i][0] != '\0') {
      int n = snprintf(tx + pos, bufSize - pos, "%s", fields[i]);
      if (n < 0) { tx[bufSize - 1] = '\0'; return String(tx); }
      pos += (size_t)n;
      if (pos >= bufSize) { tx[bufSize - 1] = '\0'; return String(tx); }
    }
  }

  // append reserved stars (13 chars)
  if (pos + 13 < bufSize) {
    memcpy(tx + pos, "*************", 13);
    pos += 13;
  } else {
    tx[bufSize - 1] = '\0';
    return String(tx);
  }

  // offline FRAM logging (keeps existing behavior)
  if (offlineMode) {
    if (millis() - lastOfflineLog > 1000 * ci[OFFLINE_LOG_GRANULARITY]) {
      unsigned long millisVal = millis();
      if (ci[FRAM_ADDRESS] > 0) {
        std::vector<std::tuple<uint8_t, uint8_t, double>> framWeatherRecord;
        if (!isnan(temperatureFromSensor)) addOfflineRecord(framWeatherRecord, 0, 5, temperatureFromSensor);
        if (!isnan(pressureFromSensor))    addOfflineRecord(framWeatherRecord, 1, 5, pressureFromSensor);
        if (!isnan(humidityFromSensor))    addOfflineRecord(framWeatherRecord, 2, 5, humidityFromSensor);
        if (ci[RTC_ADDRESS] > 0) {
          addOfflineRecord(framWeatherRecord, 32, 2, currentRTCTimestamp());
          Serial.println(currentRTCTimestamp());
        } else {
          addOfflineRecord(framWeatherRecord, 32, 2, timeClient.getEpochTime());
        }
        writeRecordToFRAM(framWeatherRecord);
        Serial.println("Saved a record to FRAM.");
        // print trimmed tx for info
        tx[(pos < bufSize - 1) ? pos : bufSize - 1] = '\0';
        Serial.print(tx);
        Serial.println(millisVal);
      }
      lastOfflineLog = millis();
    }
  }

  // append sensor id, device feature id, name, consolidate flag
  int n = snprintf(tx + pos, bufSize - pos, "%ld*%d*%s*%d",
                   (long)ci[SENSOR_ID], deviceFeatureId, sensorName.c_str(), consolidateAllSensorsToOneRecord);
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
      Serial.println(F("Failed to boot VL53L0X"));
    } else {
      Serial.print(F("VL53L0X at "));
      Serial.println(i2c);
    }
  } else if(sensorIdLocal == 680) {
    Serial.print(F("Initializing BME680 sensor...\n"));
    if (!BME680[objectCursor].begin((uint8_t)i2c)) {  // Start B DHTME680 using I2C, use first device found
      Serial.print(F(" - Unable to find BME680.\n"));
    } 
    Serial.print(F("- Setting 16x oversampling for all sensors\n"));
    BME680[objectCursor].setOversampling(TemperatureSensor, Oversample16);  // Use enumerated type values
    BME680[objectCursor].setOversampling(HumiditySensor, Oversample16);     // Use enumerated type values
    BME680[objectCursor].setOversampling(PressureSensor, Oversample16);     // Use enumerated type values
    //Serial.print(F("- Setting IIR filter to a value of 4 samples\n"));
    BME680[objectCursor].setIIRFilter(IIR4);  // Use enumerated type values
    //Serial.print(F("- Setting gas measurement to 320\xC2\xB0\x43 for 150ms\n"));  // "?C" symbols
    BME680[objectCursor].setGas(320, 150);  // 320?c for 150 milliseconds
  } else if (sensorIdLocal == 2301) {
    Serial.print(F("Initializing DHT AM2301 sensor at pin: "));
    if(powerPin > -1) {
      pinMode(powerPin, OUTPUT);
      digitalWrite(powerPin, LOW);
    }
    dht[objectCursor] = new DHT(pinNumber, sensorSubTypeLocal);
    dht[objectCursor]->begin();
  } else if (sensorIdLocal == 2320) { //AHT20
    if (AHT[objectCursor].begin()) {
      Serial.println("Found AHT20");
    } else {
      Serial.println("Didn't find AHT20");
    }  
  } else if (sensorIdLocal == 7410) { //adt7410
    adt7410[objectCursor].begin(i2c);
    adt7410[objectCursor].setResolution(ADT7410_16BIT);
  } else if (sensorIdLocal == 180) { //BMP180
    BMP180[objectCursor].begin();
  } else if (sensorIdLocal == 85) { //BMP085
    Serial.print(F("Initializing BMP085...\n"));
    BMP085d[objectCursor].begin();
  } else if (sensorIdLocal == 280) {
    Serial.print("Initializing BMP280 at i2c: ");
    Serial.print((int)i2c);
    Serial.print(" objectcursor:");
    Serial.print((int)objectCursor);
    Serial.println();
    if(!BMP280[objectCursor].begin(i2c)){
      Serial.println("Couldn't find BMX280!");
    }
  }

  sensorObjectCursor->put((String)sensorIdLocal, objectCursor + 1); //we keep track of how many of a particular ci[SENSOR_ID] we use
}

void handleWeatherData() {
  String transmissionString = weatherDataString(ci[SENSOR_ID], ci[SENSOR_SUB_TYPE], ci[SENSOR_DATA_PIN], ci[SENSOR_POWER_PIN], ci[SENSOR_I2C], NULL, 0, deviceName, -1, ci[CONSOLIDATE_ALL_SENSORS_TO_ONE_RECORD]);
  server.send(200, "text/plain", transmissionString); //Send values only to client ajax request
}

void compileAndSendDeviceData(
    const String& weatherData,
    const String& whereWhenData,
    const String& powerData,
    bool doPinCursorChanges,
    uint16_t fRAMOrdinal
) {
    // Large fixed buffer (adjust as needed)
    static char tx[2048];
    int pos = 0;

    // --- WEATHER DATA -------------------------------------------------
    if (weatherData.length() > 0) {
        pos += snprintf(tx + pos, sizeof(tx) - pos, "%s", weatherData.c_str());
    } else if (ci[SENSOR_ID] > -1) {
        String s = weatherDataString(
            ci[SENSOR_ID], ci[SENSOR_SUB_TYPE],
            ci[SENSOR_DATA_PIN], ci[SENSOR_POWER_PIN],
            ci[SENSOR_I2C], NULL, 0,
            deviceName, -1, ci[CONSOLIDATE_ALL_SENSORS_TO_ONE_RECORD]
        );
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
        pos += snprintf(tx + pos, sizeof(tx) - pos, "|*%lu*%lu*",
                        millis(), timeClient.getEpochTime());
    }

    // latency
    if (latencyCount > 0) {
        pos += snprintf(tx + pos, sizeof(tx) - pos, "%u",
                        (1000 * latencySum) / latencyCount);
    }

    // latitude*longitude*elevation*velocity*uncertainty placeholder
    pos += snprintf(tx + pos, sizeof(tx) - pos, "*****");

    // slave I2C data
    if (ci[SLAVE_I2C] > 0) {
        String s = slaveData();
        pos += snprintf(tx + pos, sizeof(tx) - pos, "*%s", s.c_str());
    }

    // --- POWER DATA ----------------------------------------------------
    if (powerData.length() > 0) {
        pos += snprintf(tx + pos, sizeof(tx) - pos, "|%s", powerData.c_str());
    } else {
        pos += snprintf(tx + pos, sizeof(tx) - pos, "|*%f*%f",
                        measuredVoltage, measuredAmpage);
    }

    // future expansion
    pos += snprintf(tx + pos, sizeof(tx) - pos, "|||");

    // --- EXTRA INFO ----------------------------------------------------
    pos += snprintf(tx + pos, sizeof(tx) - pos, "|");

    // clean up IP address
    String ipToUse = ipAddress;
    int sp = ipToUse.indexOf(' ');
    if (sp > 0) ipToUse.remove(sp);
 
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
        String s = joinMapValsOnDelimiter(pinMap, "*");
        pos += snprintf(tx + pos, sizeof(tx) - pos, "|%s", s.c_str());
    }

    // moxee reboot info
    {
        String s = joinValsOnDelimiter(moxeeRebootTimes, "*", 10);
        pos += snprintf(tx + pos, sizeof(tx) - pos, "|%s", s.c_str());
    }

    // --- SEND OUT ------------------------------------------------------
    if (!offlineMode) {
        sendRemoteData(String(tx), "getDeviceData", fRAMOrdinal);
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

  for (int attempt = 0; attempt < NUM_WIFI_CREDENTIALS; attempt++) {
    int ssidIndex = (currentWifiIndex + attempt) % NUM_WIFI_CREDENTIALS;
    const char* wifiSsid = cs[WIFI_SSID + ssidIndex * 2];
    const char* wifiPassword = cs[WIFI_PASSWORD + ssidIndex * 2];

    Serial.printf("\nAttempting WiFi connection to: %s\n", wifiSsid);

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
      unsigned long now = millis();

      // print dot every second
      if (now - lastDotTime >= 1000) {
        Serial.print(".");
        lastDotTime = now;
        wiFiSeconds++;
      }

      // print asterisk and retry every 10 seconds
      if (now - lastOfflineReconnectAttemptTime > 10000) {
        WiFi.disconnect();
        WiFi.begin(wifiSsid, wifiPassword);
        lastOfflineReconnectAttemptTime = now;
        Serial.print("*");
      }

      if (WiFi.status() == WL_NO_SSID_AVAIL) {
        Serial.printf("\nSSID not found: %s\n", wifiSsid);
        break; // try next SSID
      }

      // timeout handling
      uint32_t wifiTimeoutToUse = ci[WIFI_TIMEOUT];
      if (knownMoxeePhase == 0)
        wifiTimeoutToUse = ci[GRANULARITY_WHEN_IN_MOXEE_PHASE_0];
      else
        wifiTimeoutToUse = ci[GRANULARITY_WHEN_IN_MOXEE_PHASE_1];

      if (wiFiSeconds > wifiTimeoutToUse) {
        Serial.println("");
        Serial.println("WiFi taking too long");
        if(ci[MOXEE_POWER_SWITCH] > 0) {
          Serial.println(", rebooting Moxee");
          rebootMoxee();
        }
        Serial.println(", trying another");
        wiFiSeconds = 0;
        initialAttemptPhase = false;
        break; // move to next SSID
      }

      if (!initialAttemptPhase &&
          wiFiSeconds > (ci[WIFI_TIMEOUT] / 2) &&
          ci[FRAM_ADDRESS] > 0) {
        offlineMode = true;
        haveReconnected = false;
        return;
      }

      yield(); // keep Wi-Fi background tasks alive
    }

    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      currentWifiIndex = (ssidIndex + 1) % NUM_WIFI_CREDENTIALS; // next SSID next time
      Serial.printf("\nConnected to %s, IP: %s\n", wifiSsid,
                    WiFi.localIP().toString().c_str());
      ipAddress = WiFi.localIP().toString();
      break;
    }
  }

  if (!connected) {
    Serial.println("\nAll WiFi attempts failed.");
    if (ci[FRAM_ADDRESS] > 0)
      offlineMode = true;
    haveReconnected = false;
  } else {
    offlineMode = false;
    haveReconnected = true;
  }
}


//SEND DATA TO A REMOTE SERVER TO STORE IN A DATABASE----------------------------------------------------
void sendRemoteData(String datastring, String mode, uint16_t fRAMordinal) {
  WiFiClient clientGet;
  const int httpGetPort = 80;
  String url;
  //String mode = "getDeviceData";
  //most of the time we want to getDeviceData, not saveData. the former picks up remote control activity. the latter sends sensor data
  if(mode == "getDeviceData") {
    if(millis() - lastDataLogTime > ci[DATA_LOGGING_GRANULARITY] * 1000 || lastDataLogTime == 0) {
      mode = "saveData";
    }
    if(deviceName == "") {
      mode = "getInitialDeviceInfo";
    }
  }

  String encryptedStoragePassword = encryptStoragePassword(datastring);
  url = (String)cs[URL_GET] + "?k2=" + encryptedStoragePassword + "&device_id=" + ci[DEVICE_ID] + "&mode=" + mode + "&data=" + urlEncode(datastring, true);
 
  if(debug) {
    Serial.println("\r>>> Connecting to host: ");
  }
  //Serial.println(cs[HOST_GET]);
  int attempts = 0;
  while(!clientGet.connect(cs[HOST_GET], httpGetPort) && attempts < ci[CONNECTION_RETRY_NUMBER]) {
    attempts++;
    cleanup();
    delay(200);
  }
  if(debug) {
    Serial.print("Connection attempts:  ");
    Serial.print(attempts);
    Serial.println();
  }
  cleanup();
  if (attempts >= ci[CONNECTION_RETRY_NUMBER]) {
    Serial.print("Connection failed, moxee rebooted: ");
    connectionFailureTime = millis();
    connectionFailureMode = true;
    rebootMoxee();

    if(debug) {
      Serial.print(cs[HOST_GET]);
      Serial.println();
    }
 
  } else {

     connectionFailureTime = 0;
     connectionFailureMode = false;
     if(debug) {
      Serial.println(url);
     }
     clientGet.println("GET " + url + " HTTP/1.1");
     clientGet.print("Host: ");
     clientGet.println(cs[HOST_GET]);
     clientGet.println("User-Agent: ESP8266/1.0");
     clientGet.println("Accept-Encoding: identity");
     clientGet.println("Connection: close\r\n\r\n");
     unsigned long timeoutP = millis();
     while (clientGet.available() == 0) {
       if (millis() - timeoutP > 10000) {
        //let's try a simpler connection and if that fails, then reboot moxee
        //clientGet.stop();
        if(clientGet.connect(cs[HOST_GET], httpGetPort)){
         clientGet.println("GET / HTTP/1.1");
         clientGet.print("Host: ");
         clientGet.println(cs[HOST_GET]);
         clientGet.println("User-Agent: ESP8266/1.0");
         clientGet.println("Accept-Encoding: identity");
         clientGet.println("Connection: close\r\n\r\n");
        }//if (clientGet.connect(
        //clientGet.stop();
        return;
       } //if( millis() -  
     }
    yield();
    delay(1); //see if this improved data reception. OMG IT TOTALLY WORKED!!!
    bool receivedData = false;
    bool receivedDataJson = false;
    if(clientGet.available() && ipAddressAffectingChange != "") { //don't turn these globals off until we have data back from the server
       ipAddressAffectingChange = "";
       changeSourceId = 0;
    }
    while(clientGet.available()){
      receivedData = true;
      yield();
      String retLine = clientGet.readStringUntil('\n');
      retLine.trim();
      //Here the code is designed to be able to handle either JSON or double-delimited data from data.php
      //I started with just JSON, but that's a notoriously bulky data format, what with the names of all the
      //entities embedded and the overhead of quotes and brackets.  This is a problem because when the 
      //amount of data being sent by my server reached some critical threshold (I'm not sure what it is!)
      //it automatically gzipped the data, which I couldn't figure out how to unzip on a ESP8266.
      //So then I made a system of sending only some of the data at a time via JSON.  That introduced a lot of
      //complexity and also made the system less responsive, since you now had to wait for the device_feature to
      //get its turn in a fairly slow round-robin (on a slow internet connection, it would take ten seconds per item).
      //So that's why I implemented the non-JSON data format, which can easily specify the values for all 
      //device_features in one data object (assuming it's not too big). The ESP8266 still can respond to data in the
      //JSON format, which it will assume if the first character of the data is a '{' -- but if the first character
      //is a '|' then it assumes the data is non-JSON. Otherwise it assumes it's HTTP boilerplate and ignores it.
      if(retLine.indexOf("\"error\":") < 0 && (mode == "saveData" || mode=="commandout") && (retLine.charAt(0)== '{' || retLine.charAt(0)== '*' || retLine.charAt(0)== '|')) {
        if(debug) {
          Serial.println("can sleep because: ");
          Serial.println(retLine);
          Serial.println(retLine.indexOf("error:"));
        }
        lastDataLogTime = millis();
        moxeeRebootCount = 0; //might as well reset this
        for (int i = 0; i < 11; i++) {
            moxeeRebootTimes[i] = 0;//and these
        }
        //Serial.print("last command log id (before sleep test): ");
        //Serial.println(lastCommandLogId);
        if(lastCommandLogId == 0 && responseBuffer == "" && outputMode == 0) {
          //Serial.println("can sleep!!");
          canSleep = true; //canSleep is a global and will not be set until all the tasks of the device are finished.
        }  
        if(mode=="commandout"  || outputMode == 2) {
          lastCommandLogId = 0;
        }
        if(deferredCommand != "") {  //here is where we run commands we cannot recover from
          //deferred command has everything we need to reboot or whatever
          runCommandsFromNonJson(deferredCommand, true);
        }
        
 
        //also we can switch outputMode to 0 and clear responseBuffer
        outputMode = 0;
        responseBuffer = "";
        //responseBuffer.reserve(0);
      }
      if(retLine.charAt(0) == '*') { //getInitialDeviceInfo
        if(debug) {
          Serial.print("Initial Device Data: ");
          Serial.println(retLine);
        }
        //set the global string; we'll just use that to store our data about addtional sensors
        if(cs[SENSOR_CONFIG_STRING] != "") {
          retLine = replaceFirstOccurrenceAtChar(retLine, String(cs[SENSOR_CONFIG_STRING]), '|');
          //retLine = retLine + "|" + String(cs[SENSOR_CONFIG_STRING]); //had been doing it this way; not as good!
        }
        additionalSensorInfo = retLine;
        //once we have it
        handleDeviceNameAndAdditionalSensors((char *)additionalSensorInfo.c_str(), true);
        break;
      } else if(retLine.charAt(0) == '{') {
        if(debug) {
          Serial.print("JSON: ");
          Serial.println(retLine);
        }
        //setLocalHardwareToServerStateFromJson((char *)retLine.c_str());
        receivedDataJson = true;
        break; 
      } else if(retLine.charAt(0) == '|') { 
        if(debug) {
          Serial.print("non-JSON: ");
          Serial.println(retLine);
        }
        String serverCommandParts[3];
        splitString(retLine, '!', serverCommandParts, 3);
        setLocalHardwareToServerStateFromNonJson((char *)serverCommandParts[0].c_str());
        if(retLine.indexOf("!") > -1) {
          if(serverCommandParts[1].length()>5) { //just has latency data
            if(debug) {
              Serial.print("COMMAND (beside pin data): ");
              Serial.println(serverCommandParts[1]);
            }
          } 
          if(lastCommandLogId == 0) {
            lastCommandLogId = strtoul(serverCommandParts[2].c_str(), NULL, 10);
            //Serial.print("last command log id: ");
            //Serial.println(lastCommandLogId);
            if(lastCommandLogId > 0) {
              canSleep = false; //now we have a pending command, cannot sleep!
              deferredCanSleep = true;
            } else if (deferredCanSleep) {
              canSleep = true; 
              deferredCanSleep = false;
            }
            runCommandsFromNonJson((char *)("!" + serverCommandParts[1]).c_str(), false);
          }
        }
        receivedDataJson = true;
        break;              
      } else if(retLine.charAt(0) == '!') { //it's a command, so an exclamation point seems right
        if(debug) {
          Serial.print("COMMAND: ");
          Serial.println(retLine);
        }
        runCommandsFromNonJson((char *)retLine.c_str(), false);
        break;      
      } else {
        if(debug) {
          Serial.print("non-readable line returned: ");
          Serial.println(retLine);
        }
      }
    }
    if(receivedData && !receivedDataJson) { //an indication our server is gzipping data needed for remote control.  So instead pull it down one pin at a time and hopefully get under the gzip cutoff
      onePinAtATimeMode = true;
    }
    if(debug) {
      //Serial.print("Fram ordinal: ");
      //Serial.println(fRAMordinal);
    }
    if(fRAMordinal != 0xFFFF) {
      dumpMemoryStats(99);
      yield();
      if(ci[FRAM_ADDRESS] > 0) {
        changeDelimiterOnRecord(fRAMordinal, 0xFE);
      }
    }
   
  } //if (attempts >= ci[CONNECTION_RETRY_NUMBER])....else....    
  clientGet.stop();
}

String handleDeviceNameAndAdditionalSensors(char * sensorData, bool intialize){
  String additionalSensorArray[12];
  String specificSensorData[8];
  int i2c;
  int pinNumber;
  int powerPin;
  int sensorIdLocal;
  int sensorSubTypeLocal;
  int deviceFeatureId;
  int consolidateAllSensorsToOneRecord = 0;
  String out = "";
  int objectCursor = 0;
  int oldSensorId = -1;
  int ordinalOfOverwrite = 0;
  String sensorName;
  splitString(sensorData, '|', additionalSensorArray, 12);
  deviceName = additionalSensorArray[0].substring(1);
 
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
void setLocalHardwareToServerStateFromNonJson(char * nonJsonLine){
  int pinNumber = 0;
  String key;
  int value = -1;
  int canBeAnalog = 0;
  int enabled = 0;
  int pinCounter = 0;
  int serverSaved = 0;
  String friendlyPinName = "";
  String nonJsonPinArray[12];
  String nonJsonDatumString;
  String nonJsonPinDatum[5];
  String pinIdParts[2];
  char i2c = 0;
  splitString(nonJsonLine, '|', nonJsonPinArray, 12);
  int foundPins = 0;
  //pinMap->clear();
  for(int i=1; i<12; i++) {
    nonJsonDatumString = nonJsonPinArray[i];
    if(nonJsonDatumString.indexOf('*')>-1) {  
      splitString(nonJsonDatumString, '*', nonJsonPinDatum, 5);
      friendlyPinName = nonJsonPinDatum[0];
      key = nonJsonPinDatum[1];
      value = nonJsonPinDatum[2].toInt();
      pinName[foundPins] = friendlyPinName;
      canBeAnalog = nonJsonPinDatum[3].toInt();
      serverSaved = nonJsonPinDatum[4].toInt();

      if(key.indexOf('.')>0) {
        splitString(key, '.', pinIdParts, 2);
        i2c = pinIdParts[0].toInt();
        pinNumber = pinIdParts[1].toInt();
      } else {
        pinNumber = key.toInt();
      }
      //Serial.println("!ABOUT TO TURN OFF localsource: " + (String)localSource +  " serverSAVED: " + (String)serverSaved);
      if(!localSource || serverSaved == 1){
        if(serverSaved == 1) {//confirmation of serverSaved, so localSource flag is no longer needed
          if(debug) {
            Serial.println("SERVER SAVED==1!!");
          }
          localSource = false;
        } else {
          pinMap->remove(key);
          pinMap->put(key, value);
          //Serial.print(key);
          //Serial.print(": ");
          //Serial.println(pinMap->getIndex(key));
        }
      }
      pinList[foundPins] = key;
      pinMode(pinNumber, OUTPUT);
      if(i2c > 0) {
        //Serial.print("Non-JSON i2c: ");
        //Serial.println(key);
        setPinValueOnSlave(i2c, (char)pinNumber, (char)value); 
      } else {
        if(canBeAnalog) {
          analogWrite(pinNumber, value);
        } else {
          //Serial.print("Non-JSON reg: ");
          //Serial.println(key);
          if(value > 0) {
            digitalWrite(pinNumber, HIGH);
          } else {
            digitalWrite(pinNumber, LOW);
          }
        }
      }
      foundPins++;
    }
    
  }
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
  delay(100); 
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

void runCommandsFromNonJson(char * nonJsonLine, bool deferred){
  //can change the default values of some config data for things like polling
  String command;
  int commandId;
  String commandData;
  String commandArray[4];
  int latency;
  //first get rid of the first character, since all it does is signal that we are receiving a command:
  nonJsonLine++;
  splitString(nonJsonLine, '|', commandArray, 3);
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
  if(commandId) {
    //Serial.println(command);
    if (command == "reboot slave") {
      if(ci[SLAVE_I2C] > 0) {
        requestLong(ci[SLAVE_I2C], 128);
        textOut("Slave rebooted\n");
      }
    } else if(command.indexOf("reboot") > -1){
      //can't do this here, so we defer it!
      if(!deferred) {
        nonJsonLine--; //get the ! back at the beginning
        size_t len = strlen(nonJsonLine);
        deferredCommand = new char[len + 1];  // +1 for null terminator
        strcpy(deferredCommand, nonJsonLine);
        textOut("Rebooting... \n");
      } else {
        if(command.indexOf("watchdog") > -1){
          if(ci[SLAVE_I2C] > 0) {
            requestLong(ci[SLAVE_I2C], 134);
          }
        } else {
         rebootEsp();
        }
      }
    } else if(command == "reboot now") {
      rebootEsp(); //only use in extreme measures -- as an instant command will produce a booting loop until command is manually cleared
    } else if(command == "one pin at a time") {
      onePinAtATimeMode = (boolean)commandData.toInt(); //setting a global.
    } else if(command == "sleep seconds per loop") {
      ci[DEEP_SLEEP_TIME_PER_LOOP] = commandData.toInt(); //setting a global.
    } else if(command == "snooze seconds per loop") {
      ci[LIGHT_SLEEP_TIME_PER_LOOP] = commandData.toInt(); //setting a global.
    } else if(command == "polling granularity") {
      ci[POLLING_GRANULARITY] = commandData.toInt(); //setting a global.
    } else if(command == "logging granularity") {
      ci[DATA_LOGGING_GRANULARITY] = commandData.toInt(); //setting a global.
    } else if(command == "clear latency average") {
      latencyCount = 0;
      latencySum = 0;
    } else if(command == "ir") {
      sendIr(commandData); //ir data must be comma-delimited
    } else if(command == "clear fram") {
      if(ci[FRAM_ADDRESS] > 0) {
        clearFramLog(); 
      }
    } else if(command == "dump fram") {
      if(ci[FRAM_ADDRESS] > 0) {
        displayAllFramRecords(); 
      }
    } else if(command == "dump fram hex") {
      if(ci[FRAM_ADDRESS] > 0) {
        if(!commandData || commandData == "") {
          hexDumpFRAM(2 * ci[FRAM_INDEX_SIZE], lastRecordSize, 15);
        } else {
          hexDumpFRAM(commandData.toInt(), lastRecordSize, 15);
        }
      }
    } else if(command == "dump fram hex#") {
      if(ci[FRAM_ADDRESS] > 0) {
        hexDumpFRAMAtIndex(commandData.toInt(), lastRecordSize, 15);
      }
    } else if(command == "swap fram") {
      if(ci[FRAM_ADDRESS] > 0) {
        swapFRAMContents(ci[FRAM_INDEX_SIZE] * 2, 554, lastRecordSize);
      }
    } else if(command == "dump fram record") {
      if(ci[FRAM_ADDRESS] > 0) {
        displayFramRecord((uint16_t)commandData.toInt()); 
      }
    } else if(command == "dump fram index") {
      if(ci[FRAM_ADDRESS] > 0) {
        dumpFramRecordIndexes();
      }
    } else if (command == "set date") {
      if(ci[RTC_ADDRESS] > 0) {
        String dateArray[7];
        splitString(commandData, ',', dateArray, 7);
        setDateDs1307((byte) dateArray[0].toInt(), 
                     (byte) dateArray[1].toInt(),     
                     (byte) dateArray[2].toInt(),  
                     (byte) dateArray[3].toInt(),  
                     (byte) dateArray[4].toInt(),   
                     (byte) dateArray[5].toInt(),      
                     (byte) dateArray[6].toInt()); 
      }
                   
    } else if (command ==  "get date") {
      if(ci[RTC_ADDRESS] > 0) {
        printRTCDate();
      }

    } else if(command == "get watchdog info") {
      if(ci[SLAVE_I2C] > 0) {
        long ms      = requestLong(ci[SLAVE_I2C], 129); // millis
        long lastReboot = requestLong(ci[SLAVE_I2C], 130); // last watchdog reboot time
        long rebootCount  = requestLong(ci[SLAVE_I2C], 131); // reboot count
        long lastWePetted  = requestLong(ci[SLAVE_I2C], 132);
        long lastPetAtBite  = requestLong(ci[SLAVE_I2C], 133);
        textOut("Watchdog millis: " + String(ms) + "; Last reboot at: " + String(lastReboot) + " (" + msTimeAgo(ms, lastReboot) + "); Reboot count: " + String(rebootCount) + "; Last petted: " + String(lastWePetted) + " (" + msTimeAgo(ms, lastWePetted) + "); Bit " + String(lastPetAtBite) + " seconds after pet\n"); 
      }
    } else if(command == "get watchdog data") {
      if(ci[SLAVE_I2C] > 0) {
        textOut(slaveData() + "\n");
      } 
    } else if (command ==  "save entire config") {
      if(ci[SLAVE_I2C] > 0) {
        saveAllConfigToEEPROM();
        textOut("Configuration saved\n");
      }
    } else if (command ==  "init config") {
      if(ci[SLAVE_I2C] > 0) {
        initConfig();
      }
    } else if (command ==  "get uptime") {
      textOut("Last booted: " + timeAgo("") + "\n");
    } else if (command ==  "get wifi uptime") {
      textOut("WiFi up: " + msTimeAgo(wifiOnTime) + "\n");
    } else if (command ==  "get lastpoll") {
      textOut("Last poll: " + msTimeAgo(lastPoll) + "\n");
    } else if (command ==  "get lastdatalog") {
      textOut("Last data: " + msTimeAgo(lastDataLogTime) + "\n");
    } else if (command == "get memory") {
      dumpMemoryStats(0);
    } else if (command == "set debug") {
      debug = true;
    } else if (command == "clear debug") {
      debug = false;
    } else if (command.startsWith("readeeprom")) {
      char buffer[500]; 
      readBytesFromSlaveEEPROM((uint16_t)commandData.toInt(), buffer, 500);
      textOut(String(buffer));
    } else if (command.startsWith("getslave")) { //setting items in the configuration
      String rest = command.substring(8);  // 8 = length of "getslave"
      rest.trim(); 
      long result = requestLong(ci[SLAVE_I2C], rest.toInt()); 
      textOut("Slave data for command " + rest + ": " + (String)result + "\n");
    } else if (command.startsWith("set")) { //setting items in the configuration
      String rest = command.substring(3);  // 3 = length of "set"
      rest.trim(); 
      String key, value;
      int spaceIndex = rest.indexOf(' '); // find first space
      if (spaceIndex != -1) {
        key = rest.substring(0, spaceIndex);        // everything before space
        value = rest.substring(spaceIndex + 1);    // everything after space
        value.trim(); // remove extra spaces
      } else {
        key = rest;   // no space found, all is key
        value = "";   // no value
      }   
      if(isInteger(key)) {
        int configIndex = key.toInt();
        if(configIndex>=CONFIG_STRING_COUNT) {
          ci[configIndex] = value.toInt();
        } else {   
          char buffer[50];    
          value.toCharArray(buffer, sizeof(buffer));
          cs[configIndex] = strdup(buffer); 
        }
        textOut("Configuration set to: " + value + "\n");
      } else {
        textOut("Configuration '" + key + "' does not exist:\n");
      }
    } else if (command.startsWith("get")) { //getting items in the configuration
      String rest = command.substring(3);  // 3 = length of "set"
      rest.trim(); 
      if(isInteger(rest)) {
        int configIndex = rest.toInt();
        String value;
        if(configIndex>=CONFIG_STRING_COUNT) {
          value = String(ci[configIndex]);
        } else {
          value = String(cs[configIndex]);
        }
        textOut("Sought configuration: " + value + "\n");
      } else {
        textOut("Configuration '" + rest + "' does not exist:\n");
      }
    } else {
      //lastCommandLogId = 0; //don't do this!!
    }
    if(commandId > 0) { //don't reset lastCommandId if the command came via serial port
      lastCommandId = commandId;
    }
  }
}

bool isInteger(const String &s) {
  if (s.length() == 0) return false;

  unsigned int i = 0;

  // Optional sign
  if (s[0] == '-' || s[0] == '+') {
    if (s.length() == 1) return false; // String is only "+" or "-"
    i = 1;
  }

  for (; i < s.length(); i++) {
    if (!isDigit(s[i])) return false;
  }

  return true;
}

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
  Serial.println("IR signal sent!");
  free(rawData); // Free memory
}

String slaveData() {
    long ms      = requestLong(ci[SLAVE_I2C], 129); // millis
    long lastReboot = requestLong(ci[SLAVE_I2C], 130); // last watchdog reboot time
    long rebootCount  = requestLong(ci[SLAVE_I2C], 131); // reboot count
    long lastWePetted  = requestLong(ci[SLAVE_I2C], 132);
    long lastPetAtBite  = requestLong(ci[SLAVE_I2C], 133);
    return String(ms) + "*" + String(lastReboot) + "*" + String(rebootCount) + "*" + String(lastWePetted) + "*" + String(lastPetAtBite);
}

void rebootEsp() {
  textOut("Rebooting ESP\n");
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
  if(moxeeRebootCount > 9 && ci[FRAM_ADDRESS] > 0) { //don't bother with offlineode if we cat log data
    offlineMode = true;
    moxeeRebootCount = 0;
  } else if(moxeeRebootCount > ci[NUMBER_OF_HOTSPOT_REBOOTS_OVER_LIMITED_TIMEFRAME_BEFORE_ESP_REBOOT]) { //kind of a watchdog
    rebootEsp();
  }
}

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
  delay(55);
  Wire.begin();
  delay(55);
  yield();    
  Serial.begin(115200, SERIAL_8N1, SERIAL_FULL);
  initConfig();
  if(!loadAllConfigFromEEPROM(false)) {
    Serial.println("\nNo config found in EEPROM");
    //initConfig();
  } else {
    Serial.println("\nConfiguration retrieved from slave EEPROM");
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
  
  Serial.setRxBufferSize(256);  
  Serial.setDebugOutput(false);
  textOut("\n\nJust started up...\n");
 
  
  //Wire.setClock(50000); // Set I2C speed to 100kHz (default is 400kHz)
  startWeatherSensors(ci[SENSOR_ID],  ci[SENSOR_SUB_TYPE], ci[SENSOR_I2C], ci[SENSOR_DATA_PIN], ci[SENSOR_POWER_PIN]);
  wiFiConnect();
  server.on("/", handleRoot);      //Displays a form where devices can be turned on and off and the outputs of sensors
  server.on("/readLocalData", localShowData);
  server.on("/weatherdata", handleWeatherData);
  server.on("/writeLocalData", localSetData);
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
      Serial.println("Failed to find INA219 chip");
    } else {
      ina219->setCalibration_16V_400mA();
    }
  }
  if(ci[FRAM_ADDRESS] > 0) {
    if (!fram.begin(ci[FRAM_ADDRESS])) {
      Serial.println("Could not find FRAM (or EEPROM).");
    } else {
      Serial.println("FRAM or EEPROM found");
    }
      currentRecordCount = readRecordCountFromFRAM();
      if(lastRecordSize == 0) {
        lastRecordSize = getRecordSizeFromFRAM(0xFFFF);
      }
  }
  if(ci[IR_PIN] > -1) {
    //irsend.begin(); //do this elsewhere?
  }

  //clearFramLog();
  //displayAllFramRecords();
 
}

//LOOP----------------------------------------------------
void loop(){
 

  //Serial.println("");
  //Serial.print("KNOWN MOXEE PHASE: ");
  //Serial.println(knownMoxeePhase);
  //Serial.print("Last command log id: ");
  //Serial.println(lastCommandLogId);

  unsigned long nowTime = millis() + timeOffset;

  if (ci[SLAVE_PET_WATCHDOG_COMMAND] > 0 && (nowTime - lastPet) > 20000) { 
  
    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write((uint8_t)ci[SLAVE_PET_WATCHDOG_COMMAND]);  // command ID for "pet watchdog"
    Wire.endTransmission();
    yield();
    //feedbackPrint("pet\n");
   
    lastPet = nowTime;
  }



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

  for(int i=0; i <4; i++) { //doing this four times here is helpful to make web service reasonably responsive. once is not enough
    server.handleClient();          //Handle client requests
  }
 
  //dumpMemoryStats(122);
  //was doing an experiment:
  if(nowTime - wifiOnTime > 20000) {
    //WiFi.disconnect(true);
  }
  cleanup();
 
  if (Serial.available()) 
  {
    doSerialCommands();
  }

 
  if(lastCommandLogId > 0 || responseBuffer != "") {
    String stringToSend = responseBuffer + "\n" + lastCommandLogId;
    sendRemoteData(stringToSend, "commandout", 0xFFFF);
  }
  timeClient.update();
  
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
  if(nowTime < granularityToUse * 1000 || (nowTime - lastPoll)/1000 > granularityToUse || connectionFailureTime>0 && connectionFailureTime + ci[CONNECTION_FAILURE_RETRY_SECONDS] * 1000 > nowTime) {  //send data to backend server every <ci[POLLING_GRANULARITY]> seconds or so
    //Serial.print("Connection failure time: ");
    //Serial.println(connectionFailureTime);
    //Serial.print("  Connection failure calculation: ");
    //Serial.print(connectionFailureTime>0 && connectionFailureTime + ci[CONNECTION_FAILURE_RETRY_SECONDS] * 1000);
    //Serial.println("Epoch time:");
    //Serial.println(timeClient.getEpochTime());

    //compileAndSendDeviceData(String weatherdata, String wherewhen, String powerdata, bool doPinCursorChanges, uint16_t fRAMOrdinal)
    compileAndSendDeviceData("", "", "", true, 0xFFFF);
    lastPoll = nowTime;
  }

  lookupLocalPowerData();

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
  if(haveReconnected) {
    //try to send stored records to the backend
    if(ci[FRAM_ADDRESS] > 0){
      //dumpMemoryStats(101);
      sendAStoredRecordToBackend();
    } else {
      haveReconnected = false;
    }
  }

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
  delay(5);
  char serialByte;
  String command = "!-1|";
  while(Serial.available()) {
    serialByte = Serial.read();
    if(serialByte == '\r' || serialByte == '\n') {
      if(debug) {
        Serial.print("Serial command: ");
        Serial.println(command);
      }
      runCommandsFromNonJson((char *) command.c_str(), false);
    } else {
      command = command + serialByte;
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
  for (int i = 0; i < pinMap->size(); i++) {
    String key = pinList[i];
    textOut(key);
    textOut(" ?= ");
    textOut(String(id) + "\n");
    if(key == id) {
      pinMap->remove(key);
      pinMap->put(key, onValue);
      Serial.print("LOCAL SOURCE TRUE :");
      Serial.println(onValue);
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
  for (int i = 0; i < pinMap->size(); i++) {
    out = out + "{\"id\": \"" + pinList[i] +  "\",\"name\": \"" + pinName[i] +  "\", \"value\": \"" + (String)pinMap->getData(i) + "\"}";
    if(i < pinMap->size()-1) {
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
byte bcdToDec(byte val)
{
  return ( (val/16*10) + (val%16) );
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
  for (i = 0; i < (years - 1); i++)
  {   
      if (LEAPYEAR((epoch + i)))
        countleap++;
  }
  secs += (countleap * SECSPERDAY);
  secs += second;
  secs += (hour * SECSPERHOUR);
  secs += (minute * SECSPERMIN);
  secs += ((day - 1) * SECSPERDAY);
  
  if (month > 1)
  {
      dayspermonth = 0;
  
      if (LEAPYEAR((epoch + years))) // Only counts when we're on leap day or past it
      {
          if (month > 2)
          {
              dayspermonth = 1;
          } else if (month == 2 && day >= 29) {
              dayspermonth = 1;
          }
      }
  
      for (i = 0; i < month - 1; i++)
      {   
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
          byte *year)
{
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

void printRTCDate()
{
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

uint32_t currentRTCTimestamp()
{
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
    if(debug) {
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
  if(debug) {
    Serial.print("Bytes per line: ");
    Serial.println(bytesPerLine);
    Serial.print("index in: ");
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
    record.emplace_back(std::make_tuple(ordinal, type, value));  // ✅ Safer
  }
}

/////////////////////////////////////////////
//processor-specific
/////////////////////////////////////////////
int freeMemory() {
    return ESP.getFreeHeap();
}

/////////////////////////////////////////////
//utility functions
/////////////////////////////////////////////
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
    uint8_t nibbles[16];  // 16 hex characters → 16 nibbles
    
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


long requestLong(byte slaveAddress, byte command) {
  Wire.beginTransmission(slaveAddress);
  Wire.write(command);    // send the command
  Wire.endTransmission();

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
