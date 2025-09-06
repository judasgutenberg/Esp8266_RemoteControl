 
/*
 * ESP8266 Remote Control. Also sends weather data from multiple kinds of sensors (configured in config.c) 
 * originally built on the basis of something I found on https://circuits4you.com
 * reorganized and extended by Gus Mueller, April 24 2022 - June 22 2024
 * Also resets a Moxee Cellular hotspot if there are network problems
 * since those do not include watchdog behaviors
 */
 

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
 
#include "config.h"

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
IRsend irsend(ir_pin);
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
bool connectionFailureMode = true;  //when we're in connectionFailureMode, we check connection much more than polling_granularity. otherwise, we check it every polling_granularity

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
  if(ina219_address < 1) { //if we don't have a ina219 then do not bother
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

//returns a "*"-delimited string containing weather data, starting with temperature and ending with deviceFeatureId,    a url-encoded sensorName, and consolidateAllSensorsToOneRecord
//we might send multiple-such strings (separated by "!") to the backend for multiple sensors on an ESP8266
//i've made this to handle all the weather sensors i have so i can mix and match, though of course there are many others
String weatherDataString(int sensor_id, int sensor_sub_type, int dataPin, int powerPin, int i2c, int deviceFeatureId, char objectCursor, String sensorName, int ordinalOfOverwrite, int consolidateAllSensorsToOneRecord) {
  double humidityValue = NULL;
  double temperatureValue = NULL;
  double pressureValue = NULL;
  double gasValue = NULL;
  int32_t humidityRaw = NULL;
  int32_t temperatureRaw = NULL;
  int32_t pressureRaw = NULL;
  int32_t gasRaw = NULL;
  int32_t alt  = NULL;
  static char buf[16];
  static uint16_t loopCounter = 0;  
  String transmissionString = "";
  String sensorValue;
  double humidityFromSensor = NULL;
  double temperatureFromSensor = NULL;
  double pressureFromSensor = NULL;
  double gasFromSensor = NULL;
 
  if(deviceFeatureId == NULL) {
    objectCursor = 0;
  }
  if(sensor_id == 1) { //simple analog input. we can use subType to decide what kind of sensor it is!
    //an even smarter system would somehow be able to put together multiple analogReads here
    if(powerPin > -1) {
      digitalWrite(powerPin, HIGH); //turn on sensor power. 
    }
    delay(10);
    double value = NULL;
    if(i2c){
      //i forget how we read a pin on an i2c slave. lemme see:
      sensorValue = String(getPinValueOnSlave((char)i2c, (char)dataPin));
    } else {
      sensorValue = String(analogRead(dataPin));
    }
    /*
    for(char i=0; i<12; i++){ //we have 12 separate possible sensor functions:
      //temperature*pressure*humidity*gas*windDirection*windSpeed*windIncrement*precipitation*reserved1*reserved2*reserved3*reserved4
      //if you have some particular sensor communicating through a pin and want it to be one of these
      //you set sensor_sub_type to be the 0-based value in that *-delimited string
      //i'm thinking i don't bother defining the reserved ones and just let them be application-specific and different in different implementations
      //a good one would be radioactive counts per unit time
      if((int)i == ordinalOfOverwrite) { //had been using sensor_sub_type
        transmissionString = transmissionString + nullifyOrNumber(value);
      }
      transmissionString = transmissionString + "*";
    }
    */
    //note, if temperature ends up being NULL, the record won't save. might want to tweak data.php to save records if it contains SOME data
    
    if(powerPin > -1) {
      digitalWrite(powerPin, LOW);
    }
  } else if (sensor_id == 53) { //distance sensor, does not produce weather data
    VL53L0X_RangingMeasurementData_t measure;
    lox[objectCursor].rangingTest(&measure, false); // pass in 'true' to get debug data printout!
    if (measure.RangeStatus != 4) {  // phase failures have incorrect data
      sensorValue = String(measure.RangeMilliMeter);
    } else {
      sensorValue = "-1";
    }
    
  } else if (sensor_id == 680) { //this is the primo sensor chip, so the trouble is worth it
    //BME680 code:
    BME680[objectCursor].getSensorData(temperatureRaw, humidityRaw, pressureRaw, gasRaw);
    //i'm not sure what all this is about, since i just copied it from the BME680 example:
    sprintf(buf, "%4d %3d.%02d", (loopCounter - 1) % 9999,  // Clamp to 9999,
            (int8_t)(temperatureRaw / 100), (uint8_t)(temperatureRaw % 100));   // Temp in decidegrees
    sprintf(buf, "%3d.%03d", (int8_t)(humidityRaw / 1000),
            (uint16_t)(humidityRaw % 1000));  // Humidity milli-pct
    sprintf(buf, "%7d.%02d", (int16_t)(pressureRaw / 100),
            (uint8_t)(pressureRaw % 100));  // Pressure Pascals
    sprintf(buf, "%4d.%02d\n", (int16_t)(gasRaw / 100), (uint8_t)(gasRaw % 100));  // Resistance milliohms
    humidityFromSensor = (double)humidityRaw/1000;
    temperatureFromSensor = (double)temperatureRaw/100;
    pressureFromSensor = (double)pressureRaw/100;
    gasFromSensor = (double)gasRaw/100;  //all i ever get for this is 129468.6 and 8083.7
  } else if (sensor_id == 2301) { //i love the humble DHT
    if(powerPin > -1) {
      digitalWrite(powerPin, HIGH); //turn on DHT power, in case you're doing that. 
    }
    delay(10);
    humidityFromSensor = (double)dht[objectCursor]->readHumidity();
    temperatureFromSensor = (double)dht[objectCursor]->readTemperature();
    if(powerPin > -1) {
      digitalWrite(powerPin, LOW);//turn off DHT power. maybe it saves energy, and that's why MySpool did it this way
    }
  } else if(sensor_id == 280) {
    temperatureFromSensor = BMP280[objectCursor].readTemperature();
    pressureFromSensor = BMP280[objectCursor].readPressure()/100;
  } else if(sensor_id == 2320) { //AHT20
    sensors_event_t humidity, temp;
    AHT[objectCursor].getEvent(&humidity, &temp);
    humidityFromSensor = humidity.relative_humidity;
    temperatureFromSensor = temp.temperature;
  } else if(sensor_id == 7410) {  
    temperatureFromSensor = adt7410[objectCursor].readTempC();
  } else if(sensor_id == 180) { //so much trouble for a not-very-good sensor 
    char status;
    double p0,a;
    status = BMP180[objectCursor].startTemperature();
    if (status != 0)
    {
      // Wait for the measurement to complete:
      delay(status);   
      // Retrieve the completed temperature measurement:
      // Note that the measurement is stored in the variable T.
      // Function returns 1 if successful, 0 if failure.
      status = BMP180[objectCursor].getTemperature(temperatureFromSensor);
      if (status != 0)
      {
        status = BMP180[objectCursor].startPressure(3);
        if (status != 0)
        {
          // Wait for the measurement to complete:
          delay(status);
          // Retrieve the completed pressure measurement:
          // Note that the measurement is stored in the variable P.
          // Note also that the function requires the previous temperature measurement (temperatureValue).
          // (If temperature is stable, you can do one temperature measurement for a number of pressure measurements.)
          // Function returns 1 if successful, 0 if failure.
          status = BMP180[objectCursor].getPressure(pressureFromSensor,temperatureFromSensor);
          if (status == 0) {
            //Serial.println("error retrieving pressure measurement\n");
          }
        } else {
          //Serial.println("error starting pressure measurement\n");
        }
      } else {
        //Serial.println("error retrieving temperature measurement\n");
      }
    } else {
      //Serial.println("error starting temperature measurement\n");
    }
  } else if (sensor_id == 85) {
    //https://github.com/adafruit/Adafruit-BMP085-Library
    temperatureFromSensor = BMP085d[objectCursor].readTemperature();
    pressureFromSensor = BMP085d[objectCursor].readPressure()/100; //to get millibars!
  } else if (sensor_id == 75) { //LM75, so ghetto
    //https://electropeak.com/learn/interfacing-lm75-temperature-sensor-with-arduino/
    temperatureFromSensor = LM75[objectCursor].readTemperatureC();
  } else { //either sensor_id is NULL or 0
    //no sensor at all
  }
  //ordinalOfOverwrite allows us to take the temperature (and other values) from one of the previous sensors and place it in an arbitrary position of the delimited string
  if(ordinalOfOverwrite < 0) {
    temperatureValue = temperatureFromSensor;
    pressureValue = pressureFromSensor;
    humidityValue = humidityFromSensor;
    gasValue = gasFromSensor;
  } else {
    if(sensorValue == NULL) {
      if(sensor_sub_type == 1){
        sensorValue = String(pressureFromSensor);
      } else if (sensor_sub_type == 2){
        sensorValue = String(humidityFromSensor);
      } else if (sensor_sub_type == 3){
        sensorValue = String(gasFromSensor);
      } else {
        sensorValue = String(temperatureFromSensor); 
      }
    }
    
  }
  //
  if(sensor_id > 0) {

    
    transmissionString = nullifyOrNumber(temperatureValue) + "*" + nullifyOrNumber(pressureValue);
    transmissionString = transmissionString + "*" + nullifyOrNumber(humidityValue);
    transmissionString = transmissionString + "*" + nullifyOrNumber(gasValue);
    transmissionString = transmissionString + "*********"; //for esoteric weather sensors that measure wind and precipitation.  the last four are reserved for now

    if(offlineMode) {
      if(millis() - lastOfflineLog > 1000 * offline_log_granularity) {
        unsigned long millisVal = millis();
        //store that data in the FRAM:
        if(fram_address > 0) {
          std::vector<std::tuple<uint8_t, uint8_t, double>> framWeatherRecord;
          addOfflineRecord(framWeatherRecord, 0, 5, temperatureValue); 
          addOfflineRecord(framWeatherRecord, 1, 5, pressureValue); 
          addOfflineRecord(framWeatherRecord, 2, 5, humidityValue); 
          //addOfflineRecord(framWeatherRecord, 28, 2, (double)millis()); 
          if(rtc_address > 0) {
            addOfflineRecord(framWeatherRecord, 40, 2, currentRTCTimestamp());
            Serial.println(currentRTCTimestamp());
          } else {
            addOfflineRecord(framWeatherRecord, 40, 2, timeClient.getEpochTime());
            
          }
          //addOfflineRecord(framWeatherRecord, 8, 6, 3.141592653872233); 
          writeRecordToFRAM(framWeatherRecord);
          Serial.println("Saved a record to FRAM.");
          Serial.print(transmissionString);
          Serial.println(millisVal);
          //Serial.println("stored millis:");
          //printHexBytes(millisVal);
        }
        lastOfflineLog = millis();
      }
    } 
  }
  //using delimited data instead of JSON to keep things simple
  transmissionString = transmissionString + nullifyOrInt(sensor_id) + "*" + nullifyOrInt(deviceFeatureId) + "*" + sensorName + "*" + nullifyOrInt(consolidateAllSensorsToOneRecord); 
  if(ordinalOfOverwrite > -1) {
    transmissionString = replaceNthElement(transmissionString, ordinalOfOverwrite, String(sensorValue), '*');
    //Serial.println("ORDINAL OF OVERWRITE EFFECTS!");
    //Serial.println(transmissionString);
  }
  return transmissionString;
}

void startWeatherSensors(int sensorIdLocal, int sensorSubTypeLocal, int i2c, int pinNumber, int powerPin) {
  //i've made all these inputs generic across different sensors, though for now some apply and others do not on some sensors
  //for example, you can set the i2c address of a BME680 or a BMP280 but not a BMP180.  you can specify any GPIO as a data pin for a DHT
  int objectCursor = 0;
  if(sensorObjectCursor->has((String)sensor_id)) {
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
    if (!BME680[objectCursor].begin(I2C_STANDARD_MODE, i2c)) {  // Start B DHTME680 using I2C, use first device found
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

  sensorObjectCursor->put((String)sensorIdLocal, objectCursor + 1); //we keep track of how many of a particular sensor_id we use
}

void handleWeatherData() {
  String transmissionString = weatherDataString(sensor_id, sensor_sub_type, sensor_data_pin, sensor_power_pin, sensor_i2c, NULL, 0, deviceName, -1, consolidate_all_sensors_to_one_record);
  server.send(200, "text/plain", transmissionString); //Send values only to client ajax request
}

void compileAndSendWeatherData(String zerothRowData, String thirdRowData, String fourthRowData, bool doPinCursorChanges, uint16_t fRAMOrdinal) {
  String transmissionString = "";
  if(zerothRowData != "") {
    transmissionString = zerothRowData;
  } else {
    if(sensor_id > -1) {
      transmissionString = weatherDataString(sensor_id, sensor_sub_type, sensor_data_pin, sensor_power_pin, sensor_i2c, NULL, 0, deviceName, -1, consolidate_all_sensors_to_one_record);
    }
  }
  //add the data for any additional sensors, delimited by '!' for each sensor
  String additionalSensorData = handleDeviceNameAndAdditionalSensors((char *)additionalSensorInfo.c_str(), false);
  if(transmissionString == "") {
    additionalSensorData = additionalSensorData.substring(1); //trim off leading "!" if there is no default sensor data
  }
  transmissionString = transmissionString + additionalSensorData;
  //the time-stamps of connection failures, delimited by *
  transmissionString = transmissionString + "|" + joinValsOnDelimiter(moxeeRebootTimes, "*", 10);
  //the values of the pins as the microcontroller understands them, delimited by *, in the order of the pin_list provided by the server
  //Serial.print("pin total: ");
  //Serial.println(pinTotal);
  transmissionString = transmissionString + "|" + joinMapValsOnDelimiter(pinMap, "*"); //also send pin as they are known back to the server
  //other server-relevant info as needed, delimited by *
  //transmissionString = transmissionString + "|" + lastCommandId + "*" + pinCursor + "*" + (int)localSource + "*" + ipAddressToUse + "*" + (int)requestNonJsonPinInfo + "*" + (int)justDeviceJson + "*" + changeSourceId + "*" + timeClient.getEpochTime();
  transmissionString = transmissionString + "|";
  if(thirdRowData != "") {
    transmissionString = transmissionString + thirdRowData;
  } else {


  if(ipAddress.indexOf(' ') > 0) { //i was getting HTML header info mixed in for some reason
    ipAddress = ipAddress.substring(0, ipAddress.indexOf(' '));
  }
    String ipAddressToUse = ipAddress;
    if(ipAddressAffectingChange != "") {
       ipAddressToUse = ipAddressAffectingChange;
       changeSourceId = 1;
    }
  
    if(onePinAtATimeMode) {
      if(doPinCursorChanges) {
        pinCursor++;
        if(pinCursor >= pinTotal) {
          pinCursor = 0;
        }
      }
    }
    transmissionString = transmissionString + lastCommandId + "*" + pinCursor + "*" + (int)localSource + "*" + ipAddressToUse + "*" + (int)requestNonJsonPinInfo + "*" + (int)justDeviceJson + "*" + changeSourceId + "*" + timeClient.getEpochTime();
    transmissionString = transmissionString + "*" + millis(); //so we can know how long the gizmo has been up
    transmissionString = transmissionString + "*";
    if(latencyCount > 0) {
      transmissionString = transmissionString + (1000 * latencySum)/latencyCount;
    }

  }
  
  transmissionString = transmissionString + "|";
  if(fourthRowData != "") {
    transmissionString = transmissionString + fourthRowData;
  } else {
    transmissionString = transmissionString + "*" + measuredVoltage + "*" + measuredAmpage; //if this device could timestamp data from its archives, it would put the numeric timetamp before measuredVoltage
    //transmissionString = transmissionString + "*" + latitude + "*" + longitude; //not yet supported. might also include accelerometer data some day
    //Serial.println(transmissionString);
  }
  if(!offlineMode) {
    sendRemoteData(transmissionString, "getDeviceData", fRAMOrdinal); //if fRAMOrdinal == 0xFFFF then don't worry about FRAM
  }
}

void wiFiConnect() {
  lastOfflineReconnectAttemptTime = millis();
  unsigned long lastDotTime = 0;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  WiFi.persistent(false); //hopefully keeps my flash from being corrupted, see: https://rayshobby.net/wordpress/esp8266-reboot-cycled-caused-by-flash-memory-corruption-fixed/
  WiFi.begin(wifi_ssid, wifi_password);    
  Serial.println();
  // Wait for connection
  int wiFiSeconds = 0;
  bool initialAttemptPhase = true;
  while (WiFi.status() != WL_CONNECTED) {
     if (millis() - lastOfflineReconnectAttemptTime > 10000) { // 10s timeout
      WiFi.disconnect();
      WiFi.begin(wifi_ssid, wifi_password);
      lastOfflineReconnectAttemptTime = millis();
      Serial.print("*");
    }
    if (WiFi.status() == WL_NO_SSID_AVAIL) {
      Serial.println("SSID not found.");
      if (fram_address > 0) {
        offlineMode = true;
        return;
      }
    }
    if (millis() - lastDotTime >= 1000) {
      Serial.print(".");
      lastDotTime = millis();
      wiFiSeconds++;
    }
    uint32_t wifiTimeoutToUse = wifi_timeout;
    if(knownMoxeePhase == 0) {
      wifiTimeoutToUse = granularity_when_in_moxee_phase_0;// used to be granularity_when_in_connection_failure_mode;
    } else { //if unknown or operational, let's go slowly!
      wifiTimeoutToUse = granularity_when_in_moxee_phase_1;
    }
    if(wiFiSeconds > wifiTimeoutToUse) {
      Serial.println("");
      Serial.println("WiFi taking too long, rebooting Moxee");
      rebootMoxee();
      WiFi.begin(wifi_ssid, wifi_password);  
      wiFiSeconds = 0; //if you don't do this, you'll be stuck in a rebooting loop if WiFi fails once
      initialAttemptPhase = false;
    }
    if(!initialAttemptPhase && wiFiSeconds > (wifi_timeout/2)  && fram_address > 0) {
      //give up for the time being
      offlineMode = true;
      haveReconnected = false;
      Serial.print("Entering offline mode...");
      return;
    }
    yield();
  }
  wifiOnTime = millis();
  if(offlineMode){
    haveReconnected = true;
  }
  knownMoxeePhase = 1;
  offlineMode = false;
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(wifi_ssid);
  Serial.print("IP address: ");
  ipAddress =  WiFi.localIP().toString();
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP
}

//SEND DATA TO A REMOTE SERVER TO STORE IN A DATABASE----------------------------------------------------
void sendRemoteData(String datastring, String mode, uint16_t fRAMordinal) {
  WiFiClient clientGet;
  const int httpGetPort = 80;
  String url;
  //String mode = "getDeviceData";
  //most of the time we want to getDeviceData, not saveData. the former picks up remote control activity. the latter sends sensor data
  if(mode == "getDeviceData") {
    if(millis() - lastDataLogTime > data_logging_granularity * 1000 || lastDataLogTime == 0) {
      mode = "saveData";
    }
    if(deviceName == "") {
      mode = "getInitialDeviceInfo";
    }
  }

  String encryptedStoragePassword = encryptStoragePassword(datastring);
  url = (String)url_get + "?k2=" + encryptedStoragePassword + "&device_id=" + device_id + "&mode=" + mode + "&data=" + urlEncode(datastring, true);
 
  if(debug) {
    Serial.println("\r>>> Connecting to host: ");
  }
  //Serial.println(host_get);
  int attempts = 0;
  while(!clientGet.connect(host_get, httpGetPort) && attempts < connection_retry_number) {
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
  if (attempts >= connection_retry_number) {
    Serial.print("Connection failed, moxee rebooted: ");
    connectionFailureTime = millis();
    connectionFailureMode = true;
    rebootMoxee();

    if(debug) {
      Serial.print(host_get);
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
     clientGet.println(host_get);
     clientGet.println("User-Agent: ESP8266/1.0");
     clientGet.println("Accept-Encoding: identity");
     clientGet.println("Connection: close\r\n\r\n");
     unsigned long timeoutP = millis();
     while (clientGet.available() == 0) {
       if (millis() - timeoutP > 10000) {
        //let's try a simpler connection and if that fails, then reboot moxee
        //clientGet.stop();
        if(clientGet.connect(host_get, httpGetPort)){
         clientGet.println("GET / HTTP/1.1");
         clientGet.print("Host: ");
         clientGet.println(host_get);
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
        if(sensor_config_string != "") {
          retLine = replaceFirstOccurrenceAtChar(retLine, String(sensor_config_string), '|');
          //retLine = retLine + "|" + String(sensor_config_string); //had been doing it this way; not as good!
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
      if(fram_address > 0) {
        changeDelimiterOnRecord(fRAMordinal, 0xFE);
      }
    }
   
  } //if (attempts >= connection_retry_number)....else....    
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
  int oldsensor_id = -1;
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
      if(oldsensor_id != sensorIdLocal) { //they're sorted by sensor_id, so the objectCursor needs to be set to zero if we're seeing the first of its type
        objectCursor = 0;
      }
      if(sensorIdLocal == sensor_id) { //this particular additional sensor is the same type as the base (non-additional) sensor, so we have to pre-start it higher
        objectCursor++;
      }
      if(intialize) {
        startWeatherSensors(sensorIdLocal, sensorSubTypeLocal, i2c, pinNumber, powerPin); //guess i have to pass all this additional info
      } else {
        //otherwise do a weatherDataString
        out = out + "!" + weatherDataString(sensorIdLocal, sensorSubTypeLocal, pinNumber, powerPin, i2c, deviceFeatureId, objectCursor, sensorName, ordinalOfOverwrite, consolidateAllSensorsToOneRecord);
      }
      objectCursor++;
      oldsensor_id = sensorIdLocal;
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
      key = nonJsonPinDatum[1];
      friendlyPinName = nonJsonPinDatum[0];
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
    if(command == "reboot") {
      //can't do this here, so we defer it!
      if(!deferred) {
        nonJsonLine--; //get the ! back at the beginning
        size_t len = strlen(nonJsonLine);
        deferredCommand = new char[len + 1];  // +1 for null terminator
        strcpy(deferredCommand, nonJsonLine);
        textOut("Rebooting... \n");
      } else {
         rebootEsp();
      }
    } else if(command == "reboot now") {
      rebootEsp(); //only use in extreme measures -- as an instant command will produce a booting loop until command is manually cleared
    } else if(command == "one pin at a time") {
      onePinAtATimeMode = (boolean)commandData.toInt(); //setting a global.
    } else if(command == "sleep seconds per loop") {
      deep_sleep_time_per_loop = commandData.toInt(); //setting a global.
    } else if(command == "snooze seconds per loop") {
      light_sleep_time_per_loop = commandData.toInt(); //setting a global.
    } else if(command == "polling granularity") {
      polling_granularity = commandData.toInt(); //setting a global.
    } else if(command == "logging granularity") {
      data_logging_granularity = commandData.toInt(); //setting a global.
    } else if(command == "clear latency average") {
      latencyCount = 0;
      latencySum = 0;
    } else if(command == "ir") {
      sendIr(commandData); //ir data must be comma-delimited
    } else if(command == "clear fram") {
      if(fram_address > 0) {
        clearFramLog(); 
      }
    } else if(command == "dump fram") {
      if(fram_address > 0) {
        displayAllFramRecords(); 
      }
    } else if(command == "dump fram hex") {
      if(fram_address > 0) {
        if(!commandData || commandData == "") {
          hexDumpFRAM(2 * fram_index_size, lastRecordSize, 15);
        } else {
          hexDumpFRAM(commandData.toInt(), lastRecordSize, 15);
        }
      }
    } else if(command == "dump fram hex#") {
      if(fram_address > 0) {
        hexDumpFRAMAtIndex(commandData.toInt(), lastRecordSize, 15);
      }
    } else if(command == "swap fram") {
      if(fram_address > 0) {
        swapFRAMContents(fram_index_size * 2, 554, lastRecordSize);
      }
    } else if(command == "dump fram record") {
      if(fram_address > 0) {
        displayFramRecord((uint16_t)commandData.toInt()); 
      }
    } else if(command == "dump fram index") {
      if(fram_address > 0) {
        dumpFramRecordIndexes();
      }
    } else if (command == "set date") {
      if(rtc_address > 0) {
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
      if(rtc_address > 0) {
        printRTCDate();
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
    } else {
      //lastCommandLogId = 0; //don't do this!!
    }
    if(commandId > 0) { //don't reset lastCommandId if the command came via serial port
      lastCommandId = commandId;
    }
  }
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

void rebootEsp() {
  textOut("Rebooting ESP\n");
  ESP.restart();
}

void rebootMoxee() {  //moxee hotspot is so stupid that it has no watchdog.  so here i have a little algorithm to reboot it.
  if(moxee_power_switch > -1) {
    digitalWrite(moxee_power_switch, LOW);
    delay(7000);
    digitalWrite(moxee_power_switch, HIGH);
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
  if(moxeeRebootCount > 9 && fram_address > 0) { //don't bother with offline mode if we can't log data
    offlineMode = true;
    moxeeRebootCount = 0;
  } else if(moxeeRebootCount > number_of_hotspot_reboots_over_limited_timeframe_before_esp_reboot) { //kind of a watchdog
    rebootEsp();
  }
}

//SETUP----------------------------------------------------
void setup(void){
  
  //set specified pins to start low immediately, keeping devices from turning on
  for(int i=0; i<10; i++) {
    if((int)pins_to_start_low[i] == -1) {
      break;
    }
    pinMode((int)pins_to_start_low[i], OUTPUT);
    digitalWrite((int)pins_to_start_low[i], LOW);
  }
  if(moxee_power_switch > -1) {
    pinMode(moxee_power_switch, OUTPUT);
    digitalWrite(moxee_power_switch, HIGH);
  }
  Serial.begin(115200, SERIAL_8N1, SERIAL_FULL);
  Serial.setRxBufferSize(256);  
 
  Serial.setDebugOutput(false);
  textOut("\n\nJust started up...\n");
 
  Wire.begin();
  //Wire.setClock(50000); // Set I2C speed to 100kHz (default is 400kHz)
  startWeatherSensors(sensor_id,  sensor_sub_type, sensor_i2c, sensor_data_pin, sensor_power_pin);
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
  if(ina219_address > 0) {
    ina219 = new Adafruit_INA219(ina219_address);
    if (!ina219->begin()) {
      Serial.println("Failed to find INA219 chip");
    } else {
      ina219->setCalibration_16V_400mA();
    }
  }
  if(fram_address > 0) {
    if (!fram.begin(fram_address)) {
      Serial.println("Could not find FRAM (or EEPROM).");
    } else {
      Serial.println("FRAM or EEPROM found");
    }
      currentRecordCount = readRecordCountFromFRAM();
      if(lastRecordSize == 0) {
        lastRecordSize = getRecordSizeFromFRAM(0xFFFF);
      }
  }
  if(ir_pin > -1) {
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
  
  int granularityToUse = polling_granularity;
  if(connectionFailureMode) {
    if(knownMoxeePhase == 0) {
      granularityToUse = granularity_when_in_moxee_phase_0;// used to be granularity_when_in_connection_failure_mode;
    } else { //if unknown or operational, let's go slowly!
      granularityToUse = granularity_when_in_moxee_phase_1;
    }
  }
  //if we've been up for a week or there have been lots of moxee reboots in a short period of time, reboot esp8266
  if(nowTime > 1000 * 86400 * 7 || nowTime < hotspot_limited_time_frame * 1000  && moxeeRebootCount >= number_of_hotspot_reboots_over_limited_timeframe_before_esp_reboot) {
    //let's not do this anymore
    //Serial.print("MOXEE REBOOT COUNT: ");
    //Serial.print(moxeeRebootCount);
    //Serial.println();
    //rebootEsp();
  }
  //Serial.print(granularityToUse);
  //Serial.print(" ");
  //Serial.println(connectionFailureTime);
  if(nowTime < granularityToUse * 1000 || (nowTime - lastPoll)/1000 > granularityToUse || connectionFailureTime>0 && connectionFailureTime + connection_failure_retry_seconds * 1000 > nowTime) {  //send data to backend server every <polling_granularity> seconds or so
    //Serial.print("Connection failure time: ");
    //Serial.println(connectionFailureTime);
    //Serial.print("  Connection failure calculation: ");
    //Serial.print(connectionFailureTime>0 && connectionFailureTime + connection_failure_retry_seconds * 1000);
    //Serial.println("Epoch time:");
    //Serial.println(timeClient.getEpochTime());

    //compileAndSendWeatherData(String zerothRowData, String thirdRowData, String fourthRowData, bool doPinCursorChanges, uint16_t fRAMOrdinal)
    compileAndSendWeatherData("", "", "", true, 0xFFFF);
    lastPoll = nowTime;
  }

  lookupLocalPowerData();

  if(offlineMode || fram_address < 1){
    if(nowTime - lastOfflineReconnectAttemptTime > 1000 * offline_reconnect_interval) {
      //time to attempt a reconnection
     if(WiFi.status() != WL_CONNECTED) {
      wiFiConnect();
     }
      
    }
  } else {
    if(rtc_address > 0) { //if we have an RTC and we are not offline, sync RTC
      if (nowTime - lastRtcSyncTime > 86400000 || lastRtcSyncTime == 0) {
        syncRTCWithNTP();
        lastRtcSyncTime = nowTime;
      }
    }
  }
  if(haveReconnected) {
    //try to send stored records to the backend
    if(fram_address > 0){
      //dumpMemoryStats(101);
      sendAStoredRecordToBackend();
    } else {
      haveReconnected = false;
    }
  }

  if(canSleep) {
    //this will only work if GPIO16 and EXT_RSTB are wired together. see https://www.electronicshub.org/esp8266-deep-sleep-mode/
    if(deep_sleep_time_per_loop > 0) {
      textOut("sleeping...\n");
      ESP.deepSleep(deep_sleep_time_per_loop * 1e6); 
    }
     //this will only work if GPIO16 and EXT_RSTB are wired together. see https://www.electronicshub.org/esp8266-deep-sleep-mode/
    if(light_sleep_time_per_loop > 0) {
      textOut("snoozing...\n");
      sleepForSeconds(light_sleep_time_per_loop);
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
   Wire.beginTransmission(rtc_address);
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
  Wire.beginTransmission(rtc_address);
  Wire.write(0);
  Wire.endTransmission();
  
  Wire.requestFrom(rtc_address, 7);

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
  if (currentRecordCount >= fram_index_size) {
    currentRecordCount = 0;
  }
  uint16_t recordStartAddress = (currentRecordCount == 0) 
    ? framIndexAddress + (fram_index_size * 2)  // Leave space for the index table
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
  fram.read(fram_log_top + 2, countBytes, 2); // Read 2 bytes from address fram_log_top + 2
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
  fram.read(fram_log_top, countBytes, 2); // Read 2 bytes from address fram_log_top
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
  fram.write(fram_log_top + 2, &hi, 1); // Write 2 bytes 
  fram.write(fram_log_top + 3, &low, 1); // Write 2 bytes 
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
  fram.write(fram_log_top, &hi, 1); // Write 2 bytes 
  fram.write(fram_log_top + 1, &low, 1); // Write 2 bytes 
  interrupts(); 
  //when we do this, we ALSO want to update lastFramRecordSentIndex so that it will point to the record immediately after the one we just saved so that we can maximize rescued data should we lose the internet
  uint16_t nextRecord = recordCount+1;
  if(nextRecord >= fram_index_size)
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
  String zerothDataRowString = makeAsteriskString(12);
  String fourthDataRowString = makeAsteriskString(12);
  if(positionInFram >= fram_index_size){ //it might be too big if it overflows or way off if it was never written
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
      if(ordinal < 20) {  
        zerothDataRowString = replaceNthElement(zerothDataRowString, ordinal, String(value), '*');
      } else if (ordinal >= 40 && ordinal < 60) {
        fourthDataRowString = replaceNthElement(fourthDataRowString, ordinal-40, String(value), '*');
      }
    }

    //void handleWeatherData(String zerothRowData, String thirdRowData, String fourthRowData, bool doPinCursorChanges, uint16_t fRAMOrdinal) {
    compileAndSendWeatherData(zerothDataRowString, "", fourthDataRowString, true, positionInFram);
  } else {
    positionInFram++;
    //now save the ordinal in fram_log_top + 2
    fRAMRecordsSkipped++;
    if(fRAMRecordsSkipped > fram_index_size) {
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
  //now save the ordinal in fram_log_top + 2
  writeLastFRAMRecordSentIndex(index + 1);
}

//FRAM MEMORY MAP:
/*
fram_log_top: number of FRAM records
fram_log_top + 2: ordinal of last FRAM record sent to backend
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
  while (addr < fram_log_top) {
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
  while (size + recordStartAddress < fram_log_top) {
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

  // Allocate a buffer to temporarily store data
  uint8_t buffer1[length];
  uint8_t buffer2[length];

  // Read data from the two locations
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


int freeMemory() {
    return ESP.getFreeHeap();
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

/*
String oldEncryptStoragePassword(String datastring) {
  int timeStamp = timeClient.getEpochTime();
  char buffer[10];
  itoa(timeStamp, buffer, 10);  // Base 10 conversion
  String timestampString = String(buffer);
  byte checksum = calculateChecksum(datastring);
  String encryptedStoragePassword = urlEncode(simpleEncrypt(simpleEncrypt((String)storage_password, timestampString.substring(1,9), salt), String((char)countSetBitsInString(datastring)), String((char)checksum)), false);
  return encryptedStoragePassword;
}
*/

uint8_t rotateLeft(uint8_t value, uint8_t count) {
    return (value << count) | (value >> (8 - count));
}

uint8_t rotateRight(uint8_t value, uint8_t count) {
    return (value >> count) | (value << (8 - count));
}

//let's define how encryption_scheme works:
//a data_string is passed in as is the storage_password.  we want to be able to recreate the storage_password server-side, so no can be lossy on the storage_password
//it's all based on nibbles. two operations are performed by byte from these two nibbles, upper nibble first, then lower nibble
//00: do nothing (byte unchanged)
//01: xor storage_password at this position with count_of_zeroes in dataString
//02: xor between storage_password at this position and timestampString at this position.
//03: xor between storage_password at this position and data_string  at this position.
//04: xor storage_password with ROL data_string at this position
//05: xor storage_password with ROR data_string at this position
//06: xor storage_password with checksum of entire data_string;
//07: xor storage_password with checksum of entire stringified timestampString
//08: xor storage_password with set_bits of entire data_string
//09  xor storage_password with set_bits in entire timestampString
//10: ROL this position of storage_password 
//11: ROR this position of storage_password 
//12: ROL this position of storage_password twice
//13: ROR this position of storage_password twice
//14: invert byte of storage_password at this position (zeroes become ones and ones become zeroes)
//15: xor storage_password at this position with count_of_zeroes in full timestampString
String encryptStoragePassword(String datastring) {
    int timeStamp = timeClient.getEpochTime();
    char buffer[10];
    itoa(timeStamp, buffer, 10);  // Convert timestamp to string
    String timestampStringInterim = String(buffer);
    String timestampString = timestampStringInterim.substring(1,8); //second param is OFFSET
    //Serial.print(timestampString);
    //Serial.println("<=known to frontend");
    //timestampString = "0123227";
    // Convert encryption_scheme into an array of nibbles
    uint8_t nibbles[16];  // 64-bit number has 16 nibbles
    for (int i = 0; i < 16; i++) {
        nibbles[i] = (encryption_scheme >> (4 * (15 - i))) & 0xF;  // Extract nibble
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
    for (int i = 0; i < strlen(storage_password) * 2; i++) {
  
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
          thisByteOfStoragePassword = storage_password[counter];
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

