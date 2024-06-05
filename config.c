
const char* ssid = "your_ssid"; //mine was Moxee Hotspot83_2.4G
const char* password = "";
const char* storagePassword = "somethingdifferent"; //to ensure someone doesn't store bogus data on your server. should match value in config.php
//data posted to remote server so we can keep a historical record
//url will be in the form: http://your-server.com:80/weather/data.php?data=
const char* urlGet = "/weather/data.php";
const char* hostGet = "yourdomain.com";
const int sensorType = 0; //2301;//2301;//680; //SENSORS! -- we support these: 180 for BMP180, 2301 for DHT 2301, 680 for BME680.  0 for no sensor.  No support for multiple sensors for now.
const int sensorI2C = 0x77;
const int sensorSubType = 21; // DHT 21, AM2301
const int sensorData = 14; 
const int sensorPower = 12;
const int locationId = 4; //3 really is watchdog
const int pollingGranularity = 4; //how often to poll backend in seconds, 4 makes sense
const int dataLoggingGranularity = 300; //how often to store data in backend, 300 makes sense
const int connectionFailureRetrySeconds = 4;
const int connectionRetryNumber = 22;

const int granularityWhenInConnectionFailureMode = 5; //40 was too little time for everything to come up and start working reliably, at least with my sketchy cellular connection
const int numberOfHotspotRebootsOverLimitedTimeframeBeforeEspReboot = 4; //reboots moxee four times in 340 seconds (number below) and then reboots itself
const int hotspotLimitedTimeFrame = 340; //seconds

const int moxeePowerSwitch = 13; //usually 14

const int deepSleepTimePerLoop = 12;  //in seconds. saves energy.  set to zero if unneeded

//if you are using a DHT hygrometer/temperature probe, these values will be important
//particularly if you reflashed a MySpool temperature probe (https://myspool.com/) with custom firmware
//on those, the sensorData is 14 and sensorPower is 12
 
// #define dhType DHT11 // DHT 11
// #define dhType DHT22 // DHT 22, AM2302, AM2321
const int dhtType = 21; // DHT 21, AM2301

const char pinsToStartLow[] = {12, 13, -1}; //so when the device comes up it doesn't immediately turn on attached devices
