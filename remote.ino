/*
 * ESP8266 Remote Control. Also sends weather data from three different kinds of sensors (configured in config.c) 
 * based on something from https://circuits4you.com
 * reorganized and extended by Gus Mueller, April 24 2022 - March 27 2024
 * Also resets a Moxee Cellular hotspot if there are network problems
 * since those do not include watchdog behaviors
 */
 
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SimpleMap.h>
 
#include "config.h"

#include "Zanshin_BME680.h"  // Include the BME680 Sensor library
#include <DHT.h>
#include <SFE_BMP180.h>
#include <Adafruit_BMP085.h>
#include <Temperature_LM75_Derived.h>

DHT dht(dhtData, dhtType);
SFE_BMP180 BMP180;
BME680_Class BME680;
Adafruit_BMP085 BMP085d;
Generic_LM75 LM75;

StaticJsonDocument<1000> jsonBuffer;
WiFiUDP ntpUDP; //i guess i need this for time lookup
NTPClient timeClient(ntpUDP, "pool.ntp.org");

long connectionFailureTime = 0;
long lastDataLogTime = 0;
int timeSkewAmount = 0; //i had it as much as 20000 for 20 seconds, but serves no purpose that I can tell
int pinTotal = 12;
String pinList[12];

SimpleMap<String, int> *pinMap = new SimpleMap<String, int>([](String &a, String &b) -> int {
  if (a == b) return 0;      // a and b are equal
  else if (a > b) return 1;  // a is bigger than b
  else return -1;            // a is smaller than b
});

long moxeeRebootTimes[] = {0,0,0,0,0,0,0,0,0,0,0};
int moxeeRebootCount = 0;
int timeOffset = 0;
long lastCommandId = 0;
bool glblRemote = false;
bool onePinAtATimeMode = false; //used when the server starts gzipping data and we can't make sense of it
int pinCursor = -1;
bool connectionFailureMode = true;  //when we're in connectionFailureMode, we check connection much more than pollingGranularity. otherwise, we check it every pollingGranularity

ESP8266WebServer server(80); //Server on port 80

float altitude(const int32_t press, const float seaLevel = 1013.25);
float altitude(const int32_t press, const float seaLevel) {
  /*!
  @brief     This converts a pressure measurement into a height in meters
  @details   The corrected sea-level pressure can be passed into the function if it is known,
             otherwise the standard atmospheric pressure of 1013.25hPa is used (see
             https://en.wikipedia.org/wiki/Atmospheric_pressure) for details.
  @param[in] press    Pressure reading from BME680
  @param[in] seaLevel Sea-Level pressure in millibars
  @return    floating point altitude in meters.
  */ 
  static float Altitude;
  Altitude = 44330.0 * (1.0 - pow(((float)press / 100.0) / seaLevel, 0.1903));  // Convert into meters
  return (Altitude);
}

//ESP8266's home page:----------------------------------------------------
void handleRoot() {
 server.send(200, "text/html", "nothing"); //Send web page
}

void handleWeatherData() {
  double humidityValue;
  double temperatureValue;
  double pressureValue;
  double gasValue;
  String transmissionString = "";
  int32_t humidityRaw;
  int32_t temperatureRaw;
  int32_t pressureRaw;
  int32_t gasRaw;
  int32_t alt;
  
  static char     buf[16];                        // sprintf text buffer
  static uint16_t loopCounter = 0;                // Display iterations
  if (sensorType == 680) {
    //BME680 code:
    BME680.getSensorData(temperatureRaw, humidityRaw, pressureRaw, gasRaw);
    //i'm not sure what all this is about, since i just copied it from the BME680 example:
    sprintf(buf, "%4d %3d.%02d", (loopCounter - 1) % 9999,  // Clamp to 9999,
            (int8_t)(temperatureRaw / 100), (uint8_t)(temperatureRaw % 100));   // Temp in decidegrees
    //Serial.print(buf);
    sprintf(buf, "%3d.%03d", (int8_t)(humidityRaw / 1000),
            (uint16_t)(humidityRaw % 1000));  // Humidity milli-pct
    //Serial.print(buf);
    sprintf(buf, "%7d.%02d", (int16_t)(pressureRaw / 100),
            (uint8_t)(pressureRaw % 100));  // Pressure Pascals
    //Serial.print(buf);
    alt = altitude(pressureRaw);                                                // temp altitude
    sprintf(buf, "%5d.%02d", (int16_t)(alt), ((uint8_t)(alt * 100) % 100));  // Altitude meters
    //Serial.print(buf);
    sprintf(buf, "%4d.%02d\n", (int16_t)(gasRaw / 100), (uint8_t)(gasRaw % 100));  // Resistance milliohms
    //Serial.print(buf);
    humidityValue = (double)humidityRaw/1000;
    temperatureValue = (double)temperatureRaw/100;
    pressureValue = (double)pressureRaw/100;
    gasValue = (double)gasRaw/100;  //all i ever get for this is 129468.6 and 8083.7
  } else if (sensorType == 2301) {
    //DHT2301 code:
    digitalWrite(dhtPower, HIGH); //turn on DHT power. 
    delay(10);
    humidityValue = (double)dht.readHumidity();
    temperatureValue = (double)dht.readTemperature();
    pressureValue = 0; //really should set unknown values as null
    digitalWrite(dhtPower, LOW);//turn off DHT power. maybe it saves energy, and that's why MySpool did it this way
  } else if(sensorType == 180) {
    //BMP180 code:
    char status;
    double p0,a;
    status = BMP180.startTemperature();
    if (status != 0)
    {
      // Wait for the measurement to complete:
      delay(status);   
      // Retrieve the completed temperature measurement:
      // Note that the measurement is stored in the variable T.
      // Function returns 1 if successful, 0 if failure.
      status = BMP180.getTemperature(temperatureValue);
      if (status != 0)
      {
        status = BMP180.startPressure(3);
        if (status != 0)
        {
          // Wait for the measurement to complete:
          delay(status);
          // Retrieve the completed pressure measurement:
          // Note that the measurement is stored in the variable P.
          // Note also that the function requires the previous temperature measurement (temperatureValue).
          // (If temperature is stable, you can do one temperature measurement for a number of pressure measurements.)
          // Function returns 1 if successful, 0 if failure.
          status = BMP180.getPressure(pressureValue,temperatureValue);
          if (status != 0)
          {
            a = BMP180.altitude(pressureValue,p0);
          }
          else Serial.println("error retrieving pressure measurement\n");
        }
        else Serial.println("error starting pressure measurement\n");
      }
      else Serial.println("error retrieving temperature measurement\n");
    } else {
      Serial.println("error starting temperature measurement\n");
    }
    humidityValue = NULL; //really should set unknown values as null
  } else if (sensorType == 85) {
    //https://github.com/adafruit/Adafruit-BMP085-Library
    temperatureValue = BMP085d.readTemperature();
    pressureValue = BMP085d.readPressure()/100; //to get millibars!
    humidityValue = NULL; //really should set unknown values as null
  } else if (sensorType == 75) { //LM75
    //https://electropeak.com/learn/interfacing-lm75-temperature-sensor-with-arduino/
    temperatureValue = LM75.readTemperatureC();
    pressureValue = NULL;
    humidityValue = NULL;
  } else {
    //no sensor at all
    temperatureValue = NULL;//don't want to save data from no sensor, so force temperature out of range
    pressureValue = NULL;
    humidityValue = NULL;
  }
  if(onePinAtATimeMode) {
    pinCursor++;
    if(pinCursor > pinTotal) {
      pinCursor = 0;
    }
  }
  //Serial.print("Pin total: ");
  //Serial.print(pinTotal);
  //Serial.print(" ");
  //Serial.println(pinMap->size());
  //i don't send the data to the server with JSON because it's pretty simple and can just be * and | delimited
  //the weather data part of the string, delimited by *
  transmissionString = NullifyOrNumber(temperatureValue) + "*" + NullifyOrNumber(pressureValue) + "*" + NullifyOrNumber(humidityValue) + "*" + NullifyOrNumber(gasValue); //using delimited data instead of JSON to keep things simple
  //the time-stamps of connection failures, delimited by *
  transmissionString = transmissionString + "|" + JoinValsOnDelimiter(moxeeRebootTimes, "*", 10);
  //the values of the pins as the microcontroller understands them, delimited by *, in the order of the pin_list provided by the server
  transmissionString = transmissionString + "|" + JoinMapValsOnDelimiter(pinMap, "*", pinTotal); //also send pin as they are known back to the server
  //other server-relevant info as needed, delimited by *
  transmissionString = transmissionString + "|" + lastCommandId + "*" + pinCursor;
  //Serial.println(transmissionString);
  //had to use a global, died a little inside
  if(glblRemote) {
    sendRemoteData(transmissionString);
  } else {
    server.send(200, "text/plain", transmissionString); //Send values only to client ajax request
  }
 
}

//SETUP----------------------------------------------------
void setup(void){
  //set specified pins to start low immediately, keeping devices from turning on
  for(int i=0; i<10; i++) {
    if((int)pinsToStartLow[i] == -1) {
      break;
    }
    pinMode((int)pinsToStartLow[i], OUTPUT);
    digitalWrite((int)pinsToStartLow[i], LOW);
  }
  
  if(moxeePowerSwitch > 0) {
    pinMode(moxeePowerSwitch, OUTPUT);
    digitalWrite(moxeePowerSwitch, HIGH);
  }
  Serial.begin(115200);
  Serial.println();
  Serial.println("Just started up...");
  Wire.begin();
  WiFiConnect();
  server.on("/", handleRoot);      //Which routine to handle at root location. This is display page
  server.on("/weatherdata", handleWeatherData); //This page is called by java Script AJAX
  server.begin();                  //Start server
  Serial.println("HTTP server started");
  if(sensorType == 680) {
    Serial.print(F("Initializing BME680 sensor...\n"));
    while (!BME680.begin(I2C_STANDARD_MODE) && sensorType == 680) {  // Start BME680 using I2C, use first device found
      Serial.print(F(" - Unable to find BME680. Trying again in 5 seconds.\n"));
      delay(5000);
    }  // of loop until device is located
    Serial.print(F("- Setting 16x oversampling for all sensors\n"));
    BME680.setOversampling(TemperatureSensor, Oversample16);  // Use enumerated type values
    BME680.setOversampling(HumiditySensor, Oversample16);     // Use enumerated type values
    BME680.setOversampling(PressureSensor, Oversample16);     // Use enumerated type values
    //Serial.print(F("- Setting IIR filter to a value of 4 samples\n"));
    BME680.setIIRFilter(IIR4);  // Use enumerated type values
    //Serial.print(F("- Setting gas measurement to 320\xC2\xB0\x43 for 150ms\n"));  // "�C" symbols
    BME680.setGas(320, 150);  // 320�c for 150 milliseconds
  } else if (sensorType == 2301) {
    Serial.print(F("Initializing DHT AM2301 sensor...\n"));
    pinMode(dhtPower, OUTPUT);
    digitalWrite(dhtPower, LOW);
    dht.begin();
  } else if (sensorType == 180) { //BMP180
    BMP180.begin();
  } else if (sensorType == 85) { //BMP085
    Serial.print(F("Initializing BMP085...\n"));
    BMP085d.begin();
  }
  //initialize NTP client
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(0);
}

void WiFiConnect() {
  WiFi.begin(ssid, password);     //Connect to your WiFi router
  Serial.println();
  // Wait for connection
  int wiFiSeconds = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    wiFiSeconds++;
    if(wiFiSeconds > 80) {
      Serial.println("WiFi taking too long, rebooting Moxee");
      rebootMoxee();
      wiFiSeconds = 0; //if you don't do this, you'll be stuck in a rebooting loop if WiFi fails once
    }
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP
}

String JoinValsOnDelimiter(long vals[], String delimiter, int numberToDo) {
  String out = "";
  for(int i=0; i<numberToDo; i++){
    out = out + (String)vals[i];
    if(i < numberToDo-1) {
      out = out + delimiter;
    }
  }
  return out;
}

String JoinMapValsOnDelimiter(SimpleMap<String, int> *pinMap, String delimiter, int numberToDo) {
  String out = "";
  for (int i = 0; i < pinMap->size(); i++) {
    out = out + (String)pinMap->getData(i);
    if(i < numberToDo-1) {
      out = out + delimiter;
    }
  }
  return out;
}

String NullifyOrNumber(double inVal) {
  if(inVal == NULL) {
    return "NULL";
  } else {

    return String(inVal);
  }
}

void ShiftArrayUp(long array[], long newValue, int arraySize) {
    // Shift elements down by one index
    for (int i =  1; i < arraySize ; i++) {
        array[i - 1] = array[i];
    }
    // Insert the new value at the beginning
    array[arraySize - 1] = newValue;
}

//SEND DATA TO A REMOTE SERVER TO STORE IN A DATABASE----------------------------------------------------
void sendRemoteData(String datastring) {
  WiFiClient clientGet;
  const int httpGetPort = 80;
  String url;
  String mode = "getDeviceData";
  String storagePasswordToUse = storagePassword;
  if(sensorType == 0) {
    //seemed like a good idea at the time
    //storagePasswordToUse = "notgonnawork";//don't want to save data from no sensor;
  }
  //most of the time we want to getDeviceData, not saveData. the former picks up remote control activity. the latter sends sensor data
  if(millis() - lastDataLogTime > dataLoggingGranularity * 1000) {
    mode = "saveData";
    lastDataLogTime = millis();
  }
  url =  (String)urlGet + "?storagePassword=" + (String)storagePasswordToUse + "&locationId=" + locationId + "&mode=" + mode + "&data=" + datastring;
  Serial.println("\r>>> Connecting to host: ");
  //Serial.println(hostGet);
  int attempts = 0;
  while(!clientGet.connect(hostGet, httpGetPort) && attempts < connectionRetryNumber) {
    attempts++;
    delay(200);
  }
  Serial.print("Connection attempts:  ");
  Serial.print(attempts);
  Serial.println();
  if (attempts >= connectionRetryNumber) {
    Serial.print("Connection failed, moxee rebooted: ");
    connectionFailureTime = millis();
    connectionFailureMode = true;
    rebootMoxee();
    Serial.print(hostGet);
    Serial.println();
  } else {
     connectionFailureTime = 0;
     connectionFailureMode = false;
     Serial.println(url);
     clientGet.println("GET " + url + " HTTP/1.1");
     clientGet.print("Host: ");
     clientGet.println(hostGet);
     clientGet.println("User-Agent: ESP8266/1.0");
     clientGet.println("Accept-Encoding: identity");
     clientGet.println("Connection: close\r\n\r\n");
     unsigned long timeoutP = millis();
     while (clientGet.available() == 0) {
       if (millis() - timeoutP > 10000) {
        //let's try a simpler connection and if that fails, then reboot moxee
        //clientGet.stop();
        if(clientGet.connect(hostGet, httpGetPort)){
         //timeOffset = timeOffset + timeSkewAmount; //in case two probes are stepping on each other, make this one skew a 20 seconds from where it tried to upload data
         clientGet.println("GET / HTTP/1.1");
         clientGet.print("Host: ");
         clientGet.println(hostGet);
         clientGet.println("User-Agent: ESP8266/1.0");
         clientGet.println("Accept-Encoding: identity");
         clientGet.println("Connection: close\r\n\r\n");
        }//if (clientGet.connect(
        //clientGet.stop();
        return;
       } //if( millis() -  
     }
    delay(1); //see if this improved data reception. OMG IT TOTALLY WORKED!!!
    bool receivedData = false;
    bool receivedDataJson = false;
    while(clientGet.available()){
      receivedData = true;
      String retLine = clientGet.readStringUntil('\n');
      retLine.trim();
      if(retLine.charAt(0) == '{') {
        Serial.println(retLine);
        setLocalHardwareToServerStateFromJson((char *)retLine.c_str());
        receivedDataJson = true;
        break; 
      } else {
        Serial.print("non-JSON line returned: ");
        Serial.println(retLine);
      }
    }
    if(receivedData && !receivedDataJson) { //an indication our server is gzipping data needed for remote control.  So instead pull it down one pin at a time and hopefully get under the gzip cutoff
      onePinAtATimeMode = true;
    }
   
  } //if (attempts >= connectionRetryNumber)....else....    
  Serial.println("\r>>> Closing host: ");
  Serial.println(hostGet);
  clientGet.stop();
}

//this will set any pins specified in the JSON
void setLocalHardwareToServerStateFromJson(char * json){
  char * nodeName="device_data";
  int pinNumber = 0;
  int value = -1;
  int canBeAnalog = 0;
  int enabled = 0;
  int pinCounter = 0;
  char i2c = 0;
  DeserializationError error = deserializeJson(jsonBuffer, json);
  if(jsonBuffer[nodeName]) {
    pinCounter = 0;
    if(!onePinAtATimeMode) {
      pinMap->clear(); //this won't work
    }
    for(int i=0; i<jsonBuffer[nodeName].size(); i++) {
      pinNumber = (int)jsonBuffer[nodeName][i]["pin_number"];
      value = (int)jsonBuffer[nodeName][i]["value"];
      canBeAnalog = (int)jsonBuffer["nodeName"][i]["can_be_analog"];
      enabled = (int)jsonBuffer[nodeName][i]["enabled"];
      i2c = (int)jsonBuffer[nodeName][i]["i2c"];
      if(i2c == NULL) {
        i2c = 0;
      }
      Serial.print("pin: ");
      Serial.print(pinNumber);
      Serial.print("; value: ");
      Serial.print(value);
      Serial.println();
      pinMode(pinNumber, OUTPUT);
      if(enabled) {
        for(char j=0; j<pinTotal; j++){
          String key = (String)i2c + "." + (String)pinNumber;
          if(i2c == 0){
            key = (String)pinNumber;
          }
          //Serial.println("! " + (String)pinList[j] +  " =?: " + key +  " correcto? " + (int((String)pinList[pinCounter] == key)));
          if((String)pinList[j] == key) {
            pinMap->remove(key);
            pinMap->put(key, value);
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

void setPinValueOnSlave(char i2cAddress, char pinNumber, char pinValue) {
  //if you have a slave Arduino set up with this code:
  //https://github.com/judasgutenberg/Generic_Arduino_I2C_Slave
  //and a device_type_feature specifies an i2c address
  //then this code will send the data to that slave Arduino
  Wire.beginTransmission(i2cAddress);
  Wire.write(pinNumber);
  Wire.write(pinValue);
  Wire.endTransmission();
}

//this will run commands sent to the server
//still needs to be implemented on the backend. but if i need it, it's here
void runCommands(char * json){
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

void rebootEsp() {
  Serial.println("Rebooting ESP");
  ESP.restart();
}

void rebootMoxee() {  //moxee hotspot is so stupid that it has no watchdog.  so here i have a little algorithm to reboot it.
  if(moxeePowerSwitch > 0) {
    digitalWrite(moxeePowerSwitch, LOW);
    delay(7000);
    digitalWrite(moxeePowerSwitch, HIGH);
  }
  //only do one reboot!  it usually takes two, but this thing can be made to cycle so fast that this same function can handle both reboots, which is important if the reboot happens to 
  //be out of phase with the cellular hotspot
  ShiftArrayUp(moxeeRebootTimes,  timeClient.getEpochTime(), 10);
  moxeeRebootCount++;
}

//LOOP----------------------------------------------------
void loop(void){
  timeClient.update();
  long nowTime = millis() + timeOffset;
  int granularityToUse = pollingGranularity;
  if(connectionFailureMode) {
    granularityToUse = granularityWhenInConnectionFailureMode;
  }
  //if we've been up for a week or there have been lots of moxee reboots in a short period of time, reboot esp8266
  if(nowTime > 1000 * 86400 * 7 || nowTime < hotspotLimitedTimeFrame * 1000  && moxeeRebootCount >= numberOfHotspotRebootsOverLimitedTimeframeBeforeEspReboot) {
    Serial.print("MOXEE REBOOT COUNT: ");
    Serial.print(moxeeRebootCount);
    Serial.println();
    rebootEsp();
  }
 
 
  if(nowTime - ((nowTime/(1000 * granularityToUse) )*(1000 * granularityToUse)) == 0 || connectionFailureTime>0 && connectionFailureTime + connectionFailureRetrySeconds * 1000 > millis()) {  //send data to backend server every <pollingGranularity> seconds or so
    //Serial.print("Connection failure time: ");
    //Serial.println(connectionFailureTime);
    //Serial.print("  Connection failure calculation: ");
    //Serial.print(connectionFailureTime>0 && connectionFailureTime + connectionFailureRetrySeconds * 1000);
    //Serial.println("Epoch time:");
    //Serial.println(timeClient.getEpochTime());
    glblRemote = true;
    handleWeatherData();
    glblRemote = false;
  }
  server.handleClient();          //Handle client requests
  //digitalWrite(0, HIGH );
  //delay(100);
  //digitalWrite(0, LOW);
  //so far, this does not work:
 
  if(millis() > 10000) {
    //this will only work if GPIO16 and EXT_RSTB are wired together. see https://www.electronicshub.org/esp8266-deep-sleep-mode/
    if(deepSleepTimePerLoop > 0) {
      Serial.println("sleeping...");
      ESP.deepSleep(deepSleepTimePerLoop * 1e6); 
      Serial.println("awake...");
      WiFiConnect();
    }
  }
}
