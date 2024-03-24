/*
 * ESP8266 NodeMCU Real Time Data Graph 
 * Updates and Gets data from webpage without page refresh
 * based on something from https://circuits4you.com
 * reorganized and extended by Gus Mueller, April 24 2022
 * now also resets a Moxee Cellular hotspot if there are network problems
 * since those do not include watchdog behaviors
 */
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>


#include <NTPClient.h>
#include <WiFiUdp.h>


#include "Zanshin_BME680.h"  // Include the BME680 Sensor library
StaticJsonDocument<2200> jsonBuffer;
 
long connectionFailureTime = 0;
BME680_Class BME680;  ///< Create an instance of the BME680 class

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

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

#include "config.h"



int timeSkewAmount = 0; //i had it as much as 20000 for 20 seconds, but serves no purpose that I can tell
long moxeeRebootTimes[] = {0,0,0,0,0,0,0,0,0,0,0};

int timeOffset = 0;
bool glblRemote = false;


bool connectionFailureMode = true;  //when we're in connectionFailureMode, we check connection much more than secondsGranularity. otherwise, we check it every secondsGranularity
int moxeeRebootCount = 0;


ESP8266WebServer server(80); //Server on port 80



//ESP8266's home page:----------------------------------------------------
void handleRoot() {

 server.send(200, "text/html", "nothing"); //Send web page
}


String JoinValsOnDelimiter(long vals[], String delimiter) {
  String out = "";
  for(int i=0; i<10; i++){
    out = out + (String)vals[i] + delimiter;
  }
  return out;
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
    BME680.getSensorData(temperatureRaw, humidityRaw, pressureRaw, gasRaw);
  } 
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
  gasValue = (double)gasRaw/100;

  if(sensorType == 0) {
    temperatureValue = 9999999;//don't want to save data from no sensor, so force temperature out of range
  }
  
  transmissionString = NullifyOrNumber(temperatureValue) + "*" + NullifyOrNumber(pressureValue) + "*" + NullifyOrNumber(humidityValue) + "*" + NullifyOrNumber(gasValue); //using delimited data instead of JSON to keep things simple
  
  transmissionString = transmissionString + "|" + JoinValsOnDelimiter(moxeeRebootTimes, "*");
  
  Serial.println(transmissionString);
  //had to use a global, died a little inside
  if(glblRemote) {
    sendRemoteData(transmissionString);
  } else {
    server.send(200, "text/plain", transmissionString); //Send values only to client ajax request
  }
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

//SETUP----------------------------------------------------
void setup(void){
  //pinMode(0, OUTPUT);
  if(moxeePowerSwitch > 0) {
    pinMode(moxeePowerSwitch, OUTPUT);
    digitalWrite(moxeePowerSwitch, HIGH);
  }
  Serial.begin(115200);
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
  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP
  server.on("/", handleRoot);      //Which routine to handle at root location. This is display page
  server.on("/weatherdata", handleWeatherData); //This page is called by java Script AJAX
  server.begin();                  //Start server
  Serial.println("HTTP server started");

  Serial.print(F("- Initializing BME680 sensor\n"));
  while (!BME680.begin(I2C_STANDARD_MODE) && sensorType == 680) {  // Start BME680 using I2C, use first device found
    Serial.print(F("-  Unable to find BME680. Trying again in 5 seconds.\n"));
    delay(5000);
  }  // of loop until device is located
  Serial.print(F("- Setting 16x oversampling for all sensors\n"));
  if(sensorType == 680) {
    BME680.setOversampling(TemperatureSensor, Oversample16);  // Use enumerated type values
    BME680.setOversampling(HumiditySensor, Oversample16);     // Use enumerated type values
    BME680.setOversampling(PressureSensor, Oversample16);     // Use enumerated type values
    //Serial.print(F("- Setting IIR filter to a value of 4 samples\n"));
    BME680.setIIRFilter(IIR4);  // Use enumerated type values
    //Serial.print(F("- Setting gas measurement to 320\xC2\xB0\x43 for 150ms\n"));  // "�C" symbols
    BME680.setGas(320, 150);  // 320�c for 150 milliseconds
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

//SEND DATA TO A REMOTE SERVER TO STORE IN A DATABASE----------------------------------------------------
void sendRemoteData(String datastring) {
  WiFiClient clientGet;
  const int httpGetPort = 80;
  String url;
  String storagePasswordToUse = storagePassword;
  if(sensorType == 0) {
    //seemed like a good idea at the time
    //storagePasswordToUse = "notgonnawork";//don't want to save data from no sensor;
  }
  url =  (String)urlGet + "?storagePassword=" + (String)storagePasswordToUse + "&locationId=" + locationId + "&mode=saveData&data=" + datastring;
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
       //this part looks sus, under what circumstances would timeoutP2 accumulate 10000 milliseconds here:
       unsigned long timeoutP2 = millis();
       if (millis() - timeoutP2 > 10000) {
        Serial.println(">>> Client Timeout: moxee rebooted: ");
        Serial.println(hostGet);
        rebootMoxee();
        clientGet.stop();
        return; 
       }//if (millis()...
      }//if (clientGet.connect(
      //clientGet.stop();
      return;
     } //while (client
   
     //just checks the 1st line of the server response. Could be expanded if needed;
    delay(1); //see if this improved data reception. OMG IT TOTALLY WORKED!!!
    while(clientGet.available()){
      String retLine = clientGet.readStringUntil('\n');
      retLine.trim();
      if(retLine.charAt(0) == '{') {
        Serial.println(retLine);
        setLocalHardwareToServerStateFromJson((char *)retLine.c_str());
        break; 
      } else {
        Serial.print("non-JSON line returned: ");
        Serial.println(retLine);
      }
    }
   }
  } //end client connection if else             
  Serial.println("\r>>> Closing host: ");
  Serial.println(hostGet);
  clientGet.stop();
}

//this will set any pins specified in the JSON
void setLocalHardwareToServerStateFromJson(char * json){
  int pinNumber = 0;
  int value = -1;
  int canBeAnalog = 0;
  int enabled = 0;
  DeserializationError error = deserializeJson(jsonBuffer, json);
  if(jsonBuffer["device_data"]) {
    Serial.print("number of device pins: ");
    Serial.print(jsonBuffer["device_data"].size());
    Serial.println();
    for(int i=0; i<jsonBuffer["device_data"].size(); i++) {
      Serial.print("number of pin: ");
      Serial.print(i);
      Serial.println();
      pinNumber = (int)jsonBuffer["device_data"][i]["pin_number"];
      value = (int)jsonBuffer["device_data"][i]["value"];
      canBeAnalog = (int)jsonBuffer["device_data"][i]["can_be_analog"];
      enabled = (int)jsonBuffer["device_data"][i]["enabled"];
      Serial.print("pin: ");
      Serial.print(pinNumber);
      Serial.print("; value: ");
      Serial.print(value);
      Serial.println();
      pinMode(pinNumber, OUTPUT);
      if(enabled) {
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
  //only do one reboot!
   /*
  delay(4000);
  digitalWrite(moxeePowerSwitch, LOW);
  delay(4000);
  digitalWrite(moxeePowerSwitch, HIGH);
  */
  
 
  ShiftArrayUp(moxeeRebootTimes,  timeClient.getEpochTime(), 10);
 
 
  moxeeRebootCount++;
}

//LOOP----------------------------------------------------
void loop(void){
  long nowTime = millis() + timeOffset;
  timeClient.update();
  
  int granularityToUse = secondsGranularity;
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
  
  if(nowTime - ((nowTime/(1000 * granularityToUse) )*(1000 * granularityToUse)) == 0 || connectionFailureTime>0 && connectionFailureTime + connectionFailureRetrySeconds * 1000 > millis()) {  //send data to backend server every <secondsGranularity> seconds or so
    Serial.print("Connection failure time: ");
    Serial.print(connectionFailureTime);
    //Serial.print("  Connection failure calculation: ");
    //Serial.print(connectionFailureTime>0 && connectionFailureTime + connectionFailureRetrySeconds * 1000);
    Serial.println();
    //Serial.println("Epoch time:");
    //Serial.println(timeClient.getEpochTime());
    glblRemote = true;
    handleWeatherData();
    glblRemote = false;
  }
  //Serial.println(dht.readTemperature());
  server.handleClient();          //Handle client requests
  //digitalWrite(0, HIGH );
  //delay(100);
  //digitalWrite(0, LOW);
  
}
