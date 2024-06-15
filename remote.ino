/* 
Local Remote, Gus Mueller, April 22 2024
Provides a local control-panel for the remote-control system here:
https://github.com/judasgutenberg/Esp8266_RemoteControl
*/
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <LiquidCrystal_I2C.h>
#include <ESP_EEPROM.h>
#include "config.h"

//pseudoEPROM will work if you have the right settings in Arduino
struct nonVolatileStruct {
  char  controlIpAddress[30];
  char  weatherIpAddress[30];
} eepromData;

void ICACHE_RAM_ATTR buttonPushed();
StaticJsonDocument<1000> jsonBuffer;
StaticJsonDocument<2000> deviceJsonBuffer;  //this has to be big for some reason
StaticJsonDocument<1000> energyJsonBuffer;
String deviceName = "Your Device";
String specialUrl = "";
String ourIpAddress;

String controlIpAddress; //stored in EEPROM
String weatherIpAddress; //stored in EEPROM
 
const byte buttonNumber = 4;
byte buttonMode = 13;
byte buttonUp = 15;
byte buttonDown = 12;
byte buttonChange = 14;
byte buttonPins[buttonNumber] = {buttonChange, buttonDown, buttonMode, buttonUp}; //connect to the column pinouts of the keypad left to right: 13, 15, 12, 14
String localCopyOfJson;
long connectionFailureTime = 0;
bool connectionFailureMode = false;
bool allowInterrupts = false;
long timeOutForServerDataUpdates;
long lastTimeButtonPushed = 0;
long oldWeatherRecordingTime = 0;
long backlightOnTime = 0;
byte menuCursor = 0;
byte menuBegin = 0;
byte currentMode = 0;
byte modeCursor = 0;
const byte modeCount = 3;
byte modeDeviceSwitcher = 0;
byte modeWeather = 1;
byte modePower = 2;
byte modes[modeCount] = {modeDeviceSwitcher, modeWeather, modePower};
long updateTimes[modeCount] = {0,0,0};
long connectTimes[modeCount] = {0,0,0};
long lastScreenInit = 0;
signed char totalMenuItems = 1;
char totalScreenLines = 4;
String deviceJson = "";

double temperatureValue = -100;
double humidityValue;
double pressureValue;

double oldTemperatureValue = -100;
double oldHumidityValue;
double oldPressureValue;

int pvPower;
int batPower;
int batPercent = -1000;
int loadPower;

LiquidCrystal_I2C lcd(0x27,20,totalScreenLines); 

void setup(){
  Serial.begin(115200);
  Serial.println("Starting up Local Remote");
  backlightOn();
  lcd.init();
  lcd.setCursor(0,0);
  lcd.print("Starting...");
  WiFiConnect();
 
  enableInterripts();
 
  EEPROM.begin(sizeof(nonVolatileStruct));
  if(EEPROM.percentUsed()>=0) {
    EEPROM.get(0, eepromData);
    Serial.println("EEPROM has data from a previous run.");
    Serial.print(EEPROM.percentUsed());
    Serial.println("% of ESP flash space currently used");
    controlIpAddress = (String)eepromData.controlIpAddress;
    weatherIpAddress = (String)eepromData.weatherIpAddress;
  } else {
    Serial.println("EEPROM size changed - EEPROM data zeroed - commit() to make permanent");    
  }
}
  
void loop(){
  if(millis() - (long)backlightOnTime > backlightTimeout * 1000 && millis()/1000 - (long)backlightOnTime /1000  != 0) {
    Serial.print(millis());
    Serial.print(" | ");
    Serial.print(backlightOnTime);
    Serial.print(" | ");
    Serial.print(backlightTimeout);
    Serial.print(" | ");
    Serial.print((millis() - backlightOnTime)/1000 );
    Serial.println();
    lcd.noBacklight(); //power down the backlight if no buttons have been pushed in awhile (configured as backlightTimeout in config.c)
  }
  if(totalMenuItems == 0 || specialUrl != "" || (millis() % 5000  == 0 && ( millis() - timeOutForServerDataUpdates > hiatusLengthOfUiUpdatesAfterUserInteraction * 1000 || timeOutForServerDataUpdates == 0))) {
    if(millis() - connectTimes[modeDeviceSwitcher] > rebootPollingTimeout * 1000 && millis() > allowanceForBoot * 1000) {
      Serial.print("Too long to poll! Seconds:");
      Serial.println((millis() - connectTimes[modeDeviceSwitcher]) / 1000);
      rebootEsp();
    }
    if(millis() - timeOutForServerDataUpdates > 60 * 1000 && millis() - lastScreenInit  > hiatusLengthOfUiUpdatesAfterUserInteraction * 1000 ) {  //every hiatusLengthOfUiUpdatesAfterUserInteraction seconds of idle, reset screen
      lcd.init();
      lcd.noBacklight();
      lastScreenInit = millis();
    }
    if(deviceJson == "") {
      getDeviceInfo();
      lcd.setCursor(0,1);
      lcd.print("Getting device data");
    } else {
      if(temperatureValue == -100 || millis() - updateTimes[modeWeather] > weatherUpdateInterval * 1000) { //get temperatures every weatherUpdateInterval seconds
        if(temperatureValue == -100) {
          lcd.setCursor(0,2);
          lcd.print("Getting weather data");
        }
        getWeatherData();
        updateTimes[modeWeather] = millis();
      }
      if (batPercent == -1000 || millis() - updateTimes[modePower] > energyUpateInterval * 1000) { //get values every energyUpateInterval (maybe 69) seconds, alright!
        if(batPercent == -1000) {
          lcd.setCursor(0,3);
          lcd.print("Getting energy data");
        }
        getEnergyInfo();
        updateTimes[modePower] = millis();
      } 
      if(updateTimes[modeDeviceSwitcher] == 0 || millis() - updateTimes[0] > pollingGranularity * 1000) {
        getControlFormData();
        updateTimes[modeDeviceSwitcher] = millis();
      }
    }
  }
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
      Serial.println("WiFi taking too long");
      wiFiSeconds = 0; //if you don't do this, you'll be stuck in a rebooting loop if WiFi fails once
    }
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  ourIpAddress =  WiFi.localIP().toString();
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP
}

void backlightOn() {
  lcd.backlight();
  backlightOnTime = millis();
}

void getWeatherData() {
  WiFiClient clientGet;
  const int httpGetPort = 80;
  String url;
  url = "/weatherdata";
  const char* dataSourceHost = weatherIpAddress.c_str();
  int attempts = 0;
  while(!clientGet.connect(dataSourceHost, httpGetPort) && attempts < connectionRetryNumber) {
    attempts++;
    delay(100);
  }
  Serial.println();
  if (attempts >= connectionRetryNumber) {
    Serial.print("Weather info connection failed");
    clientGet.stop();
    return;
  } else {
     Serial.println(url);
     clientGet.println("GET " + url + " HTTP/1.1");
     clientGet.print("Host: ");
     clientGet.println(dataSourceHost);
     clientGet.println("User-Agent: ESP8266/1.0");
     clientGet.println("Accept-Encoding: identity");
     clientGet.println("Connection: close\r\n\r\n");
     unsigned long timeoutP = millis();
     while (clientGet.available() == 0) {
       if (millis() - timeoutP > 10000) {
        if(clientGet.connect(dataSourceHost, httpGetPort)){
         //timeOffset = timeOffset + timeSkewAmount; //in case two probes are stepping on each other, make this one skew a 20 seconds from where it tried to upload data
         clientGet.println("GET / HTTP/1.1");
         clientGet.print("Host: ");
         clientGet.println(dataSourceHost);
         clientGet.println("User-Agent: ESP8266/1.0");
         clientGet.println("Accept-Encoding: identity");
         clientGet.println("Connection: close\r\n\r\n");
        }//if (clientGet.connect(
        //clientGet.stop();
        return;
       } //if( millis() -  
     }
    delay(2); //see if this improved data reception. OMG IT TOTALLY WORKED!!!
    while(clientGet.available()){
      connectTimes[modeWeather] = millis();
      String retLine = clientGet.readStringUntil('\r');//when i was reading string until '\n' i didn't get any JSON most of the time!
      retLine.trim();
      if(retLine.indexOf('*') >0 && retLine.indexOf('|') >0) {
        String lineArray[1];
        String dataArray[3];
        splitString(retLine, '|', lineArray, 1);
        String firstLine = lineArray[0];
        splitString(firstLine, '*', dataArray, 3);
        Serial.println(firstLine);
        oldTemperatureValue = temperatureValue;
        oldHumidityValue = humidityValue;
        oldPressureValue = pressureValue;
        temperatureValue = dataArray[0].toDouble();
        pressureValue = dataArray[1].toDouble();
        humidityValue = dataArray[2].toDouble();
      } else {
        Serial.print("weather non-data line returned: ");
        Serial.println(retLine);
      }
    }
  }  
  clientGet.stop();
}

void getDeviceInfo() { //goes on the internet to get the latest ip addresses of the various things to connect to. otherwise get them from like EEPROM
  WiFiClient clientGet;
  const int httpGetPort = 80;
  String url;
  url = "/weather/data.php?storagePassword=" + (String)storagePassword + "&mode=getDevices";

  char * dataSourceHost = "randomsprocket.com";
  int attempts = 0;
  while(!clientGet.connect(dataSourceHost, httpGetPort) && attempts < connectionRetryNumber) {
    attempts++;
    delay(100);
  }
  Serial.println();
  if (attempts >= connectionRetryNumber) {
    Serial.print("Device info connection failed");
    clientGet.stop();
    return;
  } else {
     Serial.println(url);
     clientGet.println("GET " + url + " HTTP/1.1");
     clientGet.print("Host: ");
     clientGet.println(dataSourceHost);
     clientGet.println("User-Agent: ESP8266/1.0");
     clientGet.println("Accept-Encoding: identity");
     clientGet.println("Connection: close\r\n\r\n");
     unsigned long timeoutP = millis();
     while (clientGet.available() == 0) {
       if (millis() - timeoutP > 10000) {
        if(clientGet.connect(dataSourceHost, httpGetPort)){
         //timeOffset = timeOffset + timeSkewAmount; //in case two probes are stepping on each other, make this one skew a 20 seconds from where it tried to upload data
         clientGet.println("GET / HTTP/1.1");
         clientGet.print("Host: ");
         clientGet.println(dataSourceHost);
         clientGet.println("User-Agent: ESP8266/1.0");
         clientGet.println("Accept-Encoding: identity");
         clientGet.println("Connection: close\r\n\r\n");
        }//if (clientGet.connect(
        //clientGet.stop();
        return;
       } //if( millis() -  
     }
    delay(2); //see if this improved data reception. OMG IT TOTALLY WORKED!!!
    while(clientGet.available()){
      String retLine = clientGet.readStringUntil('\r');//when i was reading string until '\n' i didn't get any JSON most of the time!
      retLine.trim();
      if(retLine.charAt(0) == '{') {
        Serial.println(retLine);
        deviceJson = (String)retLine.c_str();
        DeserializationError error = deserializeJson(deviceJsonBuffer, deviceJson);
        String ipAddress;
        int deviceId;
        Serial.println((int)deviceJsonBuffer["devices"].size());
        bool eepromUpdateNeeded = false;
        for(int i=0; i<deviceJsonBuffer["devices"].size(); i++) {
            ipAddress = (String)deviceJsonBuffer["devices"][i]["ip_address"];
            deviceId = (int)deviceJsonBuffer["devices"][i]["device_id"];
            Serial.print(ipAddress);
            Serial.print(" ");
            Serial.println(deviceId);
            if(deviceId == (int)weatherDevice){
              if(weatherIpAddress != ipAddress){
                weatherIpAddress = ipAddress; 
                const char* cString = weatherIpAddress.c_str();
                std::strncpy(eepromData.weatherIpAddress, cString, sizeof(eepromData.weatherIpAddress));
                eepromData.weatherIpAddress[sizeof(eepromData.weatherIpAddress) - 1] = '\0';
                eepromUpdateNeeded = true;
              }            
            } else if(deviceId == (int)controlDevice){
              if(controlIpAddress != ipAddress){
                controlIpAddress = ipAddress; 
                const char* cString = controlIpAddress.c_str();
                std::strncpy(eepromData.controlIpAddress, cString, sizeof(eepromData.controlIpAddress));
                eepromData.controlIpAddress[sizeof(eepromData.controlIpAddress) - 1] = '\0';
                eepromUpdateNeeded = true;
              }
            }
        }
        if(eepromUpdateNeeded) {
          EEPROM.put(0, eepromData);
          if(!EEPROM.commit()){
            Serial.println("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxEeprom update failed.");
          } else {
            Serial.println("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxEeprom update SUCCEEDED!!!!.");
          }
        }
      } else {
        Serial.print("device non-JSON line returned: ");
        Serial.println(retLine);
      }
    }
  }  
  clientGet.stop();
}

void getEnergyInfo() { //goes on the internet to get the latest solar energy data for display
  WiFiClient clientGet;
  const int httpGetPort = 80;
  String url;
  url = "/weather/data.php?storagePassword=" + (String)storagePassword + "&mode=getEnergyInfo";

  char * dataSourceHost = "randomsprocket.com";
  int attempts = 0;
  while(!clientGet.connect(dataSourceHost, httpGetPort) && attempts < connectionRetryNumber) {
    attempts++;
    delay(100);
  }
  Serial.println();
  if (attempts >= connectionRetryNumber) {
    Serial.print("Solar info connection failed");
    clientGet.stop();
    return;
  } else {
     Serial.println(url);
     clientGet.println("GET " + url + " HTTP/1.1");
     clientGet.print("Host: ");
     clientGet.println(dataSourceHost);
     clientGet.println("User-Agent: ESP8266/1.0");
     clientGet.println("Accept-Encoding: identity");
     clientGet.println("Connection: close\r\n\r\n");
     unsigned long timeoutP = millis();
     while (clientGet.available() == 0) {
       if (millis() - timeoutP > 10000) {
        if(clientGet.connect(dataSourceHost, httpGetPort)){
         //timeOffset = timeOffset + timeSkewAmount; //in case two probes are stepping on each other, make this one skew a 20 seconds from where it tried to upload data
         clientGet.println("GET / HTTP/1.1");
         clientGet.print("Host: ");
         clientGet.println(dataSourceHost);
         clientGet.println("User-Agent: ESP8266/1.0");
         clientGet.println("Accept-Encoding: identity");
         clientGet.println("Connection: close\r\n\r\n");
        }//if (clientGet.connect(
        //clientGet.stop();
        return;
       } //if( millis() -  
     }
    delay(2); //see if this improved data reception. OMG IT TOTALLY WORKED!!!
    while(clientGet.available()){
      connectTimes[modePower] = millis();
      String retLine = clientGet.readStringUntil('\r');//when i was reading string until '\n' i didn't get any JSON most of the time!
      retLine.trim();
      if(retLine.charAt(0) == '{') {
        Serial.println(retLine);
        String energyJson = (String)retLine.c_str();
        DeserializationError error = deserializeJson(energyJsonBuffer, energyJson);
        pvPower = (int)energyJsonBuffer["energy_info"]["pv_power"];
        batPower = (int)energyJsonBuffer["energy_info"]["bat_power"];
        batPercent = (int)energyJsonBuffer["energy_info"]["bat_percent"];
        loadPower = (int)energyJsonBuffer["energy_info"]["load_power"];
      } else {
        Serial.print("power non-JSON line returned: ");
        Serial.println(retLine);
      }
    }
  }  
  clientGet.stop();
}

void getControlFormData() {
  Serial.print("control ip address:");
  Serial.println(controlIpAddress);
  WiFiClient clientGet;
  const int httpGetPort = 80;
  String url;
  if(specialUrl != "") {
    url =  (String)specialUrl;
    specialUrl = "";
  } else {
    url =  (String)"/readLocalData";
  }
  int attempts = 0;
  while(!clientGet.connect(controlIpAddress, httpGetPort) && attempts < connectionRetryNumber) {
    attempts++;
    delay(100);
  }
  Serial.println();
  if (attempts >= connectionRetryNumber) {
    Serial.print("Connection failed");
    connectionFailureTime = millis();
    connectionFailureMode = true;
  } else {
     connectionFailureTime = 0;
     connectionFailureMode = false;
     Serial.println(url);
     clientGet.println("GET " + url + " HTTP/1.1");
     clientGet.print("Host: ");
     clientGet.println(controlIpAddress);
     clientGet.println("User-Agent: ESP8266/1.0");
     clientGet.println("Accept-Encoding: identity");
     clientGet.println("Connection: close\r\n\r\n");
     unsigned long timeoutP = millis();
     while (clientGet.available() == 0) {
       if (millis() - timeoutP > 10000) {
        if(clientGet.connect(controlIpAddress, httpGetPort)){
         //timeOffset = timeOffset + timeSkewAmount; //in case two probes are stepping on each other, make this one skew a 20 seconds from where it tried to upload data
         clientGet.println("GET / HTTP/1.1");
         clientGet.print("Host: ");
         clientGet.println(controlIpAddress);
         clientGet.println("User-Agent: ESP8266/1.0");
         clientGet.println("Accept-Encoding: identity");
         clientGet.println("Connection: close\r\n\r\n");
        }//if (clientGet.connect(
        //clientGet.stop();
        return;
       } //if( millis() -  
     }
    delay(2); //see if this improved data reception. OMG IT TOTALLY WORKED!!!
    bool receivedData = false;
    bool receivedDataJson = false;
    while(clientGet.available()){
      connectTimes[modeDeviceSwitcher] = millis();
      receivedData = true;
      String retLine = clientGet.readStringUntil('\r');//when i was reading string until '\n' i didn't get any JSON most of the time!
      retLine.trim();
      if(retLine.charAt(0) == '{') {
        Serial.println(retLine);
        localCopyOfJson = (String)retLine.c_str();
        updateScreen(localCopyOfJson, 0, false); 
        receivedDataJson = true;
        break; 
      } else {
        Serial.print("non-JSON line returned: ");
        Serial.println(retLine);
      }
    }
  } //if (attempts >= connectionRetryNumber)....else....    
  Serial.println("\r>>> Closing host for json: ");
  Serial.println(controlIpAddress);
  clientGet.stop();
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

void updateScreen(String json, char startLine, bool withInit) { //handles the display of everything
  allowInterrupts = false;
  bool showTimingDebugInfo = false;
  lcd.clear();
  char buffer[12];
  if(currentMode == modeWeather) {
    char directionIndication = ' ';
    char format[] = "%7.2f";
    lcd.setCursor(0, 0);
    lcd.print("Current Weather");
    if (oldTemperatureValue == -100) { //this condition a proxy for there being no old weather data
      directionIndication = ' ';
    } else if(oldTemperatureValue-temperatureValue > temperatureDeltaForChange) {//we want a change in temperature to be at least as big as temperatureDeltaForChange to produce a change arrow
      directionIndication  = 'v';
    } else if (oldTemperatureValue-temperatureValue < -temperatureDeltaForChange) {
      directionIndication  = '^';
    } else {
      directionIndication  = '=';
    }
    lcd.setCursor(19, 1);
    lcd.print(directionIndication);
    lcd.setCursor(0, 1);
    sprintf(buffer, format, temperatureValue * 1.8 +32);
    lcd.print(buffer);
    lcd.setCursor(13, 1);
    lcd.print("deg F");
    if (oldTemperatureValue == -100) { //this condition a proxy for there being no old weather data
      directionIndication = ' ';
    } else if(oldPressureValue - pressureValue > pressureDeltaForChange) {//we want a change in pressure to be at least as big as pressureDeltaForChange to produce a change arrow
      directionIndication  = 'v';
    } else if (oldPressureValue - pressureValue < -pressureDeltaForChange) {
      directionIndication  = '^';
    } else {
      directionIndication  = '=';
    }
    lcd.setCursor(19, 2);
    lcd.print(directionIndication);  
    lcd.setCursor(0, 2);
    sprintf(buffer, format, pressureValue);
    lcd.print(buffer);
    lcd.setCursor(13, 2);
    lcd.print("mm Hg");
    if (oldTemperatureValue == -100) { //this condition a proxy for there being no old weather data
      directionIndication = ' ';
    } else if(oldHumidityValue - humidityValue > humidityDeltaForChange) { //we want a change in humidity to be at least as big as humidityDeltaForChange to produce a change arrow
      directionIndication  = 'v';
    } else if (oldHumidityValue - humidityValue < -humidityDeltaForChange) {
      directionIndication  = '^';
    } else {
      directionIndication  = '=';
    }
    lcd.setCursor(19, 3);
    lcd.print(directionIndication);
    lcd.setCursor(0, 3);
    sprintf(buffer, format, humidityValue);
    lcd.print(buffer);
    lcd.setCursor(13, 3);
    lcd.print("% rel");
    if(showTimingDebugInfo) {
      lcd.setCursor(17, 0);
      lcd.print((millis() - updateTimes[1])/1000);
    }
  } else if(currentMode == modePower) {
    char format[] = "%6d";
    lcd.setCursor(0, 0);
    lcd.print("Solar: ");
    sprintf(buffer, format, pvPower);
    lcd.setCursor(12, 0);
    lcd.print(buffer);
    lcd.setCursor(19, 0);
    lcd.print("w");
    lcd.setCursor(0, 1);
    lcd.print("Load: ");
    lcd.setCursor(12, 1);
    sprintf(buffer, format, loadPower);
    lcd.print(buffer);
    lcd.setCursor(19, 1);
    lcd.print("w");
    lcd.setCursor(0, 2);
    lcd.print("Battery: ");
    lcd.setCursor(12, 2);
    sprintf(buffer, format, batPercent);
    lcd.print(buffer);
    lcd.setCursor(19, 2);
    lcd.print("%");
    lcd.setCursor(0, 3);
    if(batPower < 0) {
      lcd.print("Charging: ");
    } else {
      lcd.print("Draining: "); 
    }
    lcd.setCursor(12, 3);
    sprintf(buffer, format, abs(batPower));
    lcd.print(buffer);
    lcd.setCursor(19, 3);
    lcd.print("w");
    if(showTimingDebugInfo) {
      lcd.setCursor(10, 0);
      lcd.print((millis() - updateTimes[2])/1000);
    }
  } else {
    if(specialUrl != "") {
      return;
    }
    allowInterrupts = true;
    if(withInit == true) {
      menuBegin = 0;
      menuCursor = 0;
    }
    if(startLine == 0){
      menuBegin = 0;
    }
    
    int value = -1;
    int serverSaved = 0;
    String friendlyPinName = "";
    String id = "";
    if(json != "") {
      DeserializationError error = deserializeJson(jsonBuffer, json);
    }
    if(jsonBuffer["device"]) { //deviceName is a global
      deviceName = (String)jsonBuffer["device"];
    }
    char * nodeName="pins";
    if(jsonBuffer[nodeName]) {
      char totalShown = 0;
      totalMenuItems = jsonBuffer[nodeName].size();
      for(int i=0; i<jsonBuffer[nodeName].size(); i++) {
        if(i >= startLine && totalShown < totalScreenLines) {
          lcd.setCursor(0, i - startLine);
          friendlyPinName = (String)jsonBuffer[nodeName][i]["name"];
          Serial.println(friendlyPinName);
          value = (int)jsonBuffer[nodeName][i]["value"];
          id = (String)jsonBuffer[nodeName][i]["id"];
          if(value == 1) {
            lcd.print(" *" + friendlyPinName.substring(0, 18));
          } else {
            lcd.print("  " + friendlyPinName.substring(0, 18));
          }
          totalShown++; 
        }
      }
    }
    Serial.print("total menu items:");
    Serial.println((int)totalMenuItems);
    if(showTimingDebugInfo) {
      lcd.setCursor(17, 0);
      lcd.print((millis() - updateTimes[0])/1000);
    }
  }
  allowInterrupts = true;
}

void moveCursorUp(){
  if(currentMode != modeDeviceSwitcher) {
    return;
  }
  Serial.print("up: ");
  Serial.print((int)menuCursor);
  Serial.println(" * " );

  if(menuCursor > totalScreenLines - 1) {
    lcd.setCursor(0, totalScreenLines - 1);
  } else {
    lcd.setCursor(0, menuCursor);
  }
  lcd.print(" ");
  if(menuBegin > 0) {
    //scroll screen up:
    menuBegin--;
    updateScreen("", menuBegin, false);
  } else {
    menuCursor--;
    if(menuCursor < 0  || menuCursor > 250) {
      Serial.print(" less than zero ");
      Serial.println((int)menuCursor);
      menuCursor = 0;
    }
  }
  if(menuCursor == totalScreenLines) {
    menuCursor = totalScreenLines - 1;
  }
  lcd.setCursor(0, menuCursor);
  lcd.print(">");
  Serial.println((int)menuCursor);
}
void moveCursorDown(){
  if(currentMode != modeDeviceSwitcher) {
    return;
  }
  Serial.println("down: ");
  Serial.print((int)menuCursor);
  lcd.setCursor(0, menuCursor);
  lcd.print(" ");
  menuCursor++;
  if(menuCursor > totalMenuItems-1) {
    menuCursor = totalMenuItems-1;
  }
  if(menuCursor > totalScreenLines - 1) {
    //scroll screen up:
    menuBegin = menuCursor-(totalScreenLines - 1);
    Serial.print(" Menu begin: ");
    Serial.println((int)menuBegin);
    updateScreen("", menuBegin, false);
    lcd.setCursor(0, totalScreenLines - 1);
  } else {
    lcd.setCursor(0, menuCursor);
  }
  lcd.print(">");
  Serial.println((int)menuCursor);
}

void toggleDevice(){
  if(currentMode == modeWeather) { //reboot the system if toggle is hit while we're looking at weather
    rebootEsp();
  }
  if(currentMode != modeDeviceSwitcher) {
    return;
  }
  Serial.print("TOGGLE! screen: ");
  if(menuCursor > totalScreenLines - 1) {
    lcd.setCursor(0, totalScreenLines - 1);
    Serial.print(totalScreenLines - 1);
  } else {
    lcd.setCursor(0, menuCursor);
     Serial.print(menuCursor);
  }
  Serial.print(" for menu item: ");
  if(getPinValue(menuCursor) == 0) {
    lcd.print(">*");
    sendDataToController(menuCursor, 1);
  } else {
    lcd.print("> ");
    sendDataToController(menuCursor, 0);
  }
  Serial.println((int)menuCursor);
}

void advanceMode() {
  modeCursor++;
  if(modeCursor >= modeCount) {
    modeCursor = 0;
  }
  currentMode = modes[modeCursor];
  Serial.print("current mode: ");
  Serial.println(currentMode);
  updateScreen("", menuBegin, false);
}
 
void buttonPushed() {
  if(!allowInterrupts) {
    return;
  }
  for(char i=0; i<buttonNumber; i++) {
    detachInterrupt(digitalPinToInterrupt(buttonPins[i]));
  }
  boolean skipButtonFunction = false;
  if(lastTimeButtonPushed == 0 || millis() - lastTimeButtonPushed > 150) { //150 millisecond debounce
    lastTimeButtonPushed = millis();
  } else {
    //too soon, so debounce!
    skipButtonFunction = true;
  }
  timeOutForServerDataUpdates = millis();
  volatile int val;
  volatile int hardwarePinNumber;
  //backlightOn();
  for(volatile char i=0; i<buttonNumber; i++) {
    hardwarePinNumber = buttonPins[i];
    val = digitalRead(hardwarePinNumber);
    
    if(val == 1) {
      if(millis() - backlightOnTime > backlightTimeout * 1000) {
        skipButtonFunction = true; //if we're in LCD blackout, don't do button function, just turn on backlight
      }
      
      if(!skipButtonFunction){
        if(hardwarePinNumber == buttonUp) {
          moveCursorUp();
        }
        if(hardwarePinNumber == buttonDown) {
          moveCursorDown();
        }
        if(hardwarePinNumber == buttonChange) {
          toggleDevice();
        }
        if(hardwarePinNumber == buttonMode) {
          advanceMode();
        }
      }
    }
  }
  backlightOn();
  enableInterripts();
}

void enableInterripts() {
   for(char i=0; i<buttonNumber; i++) {
    pinMode(buttonPins[i], OUTPUT);
    attachInterrupt(digitalPinToInterrupt(buttonPins[i]), buttonPushed, RISING);
  }
}

char getPinValue(char ordinal) {
  char * nodeName="pins";
  char value = -1;
  if(jsonBuffer[nodeName]) {
    for(int i=0; i<jsonBuffer[nodeName].size(); i++) {
      value = (int)jsonBuffer[nodeName][i]["value"];
      if(ordinal == i) {
        return value;
      }
    }
  }
  return 0;
}

void sendDataToController(char ordinal, char value) {
  char * nodeName="pins";
  String id = "";
  if(jsonBuffer[nodeName]) {
    for(int i=0; i<jsonBuffer[nodeName].size(); i++) {
      id = (String)jsonBuffer[nodeName][i]["id"];
      if((int)ordinal == i) {
        //we just snag one of the updates and use it to send data to the remote controller
        specialUrl =  (String)"/writeLocalData?id=" + id + "&on=" + (int)value;
      }
    }
  }
}

void rebootEsp() {
  Serial.println("Rebooting ESP");
  backlightOn();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Forced to restart...");
  ESP.restart();
}
