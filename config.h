 
  
extern const char* ssid; //mine was Moxee Hotspot83_2.4G
extern const char* password;
extern const char* storagePassword; //to ensure someone doesn't store bogus data on your server. should match value in config.php
//data posted to remote server so we can keep a historical record
//url will be in the form: http://your-server.com:80/weather/data.php?data=
extern const char* urlGet;
extern const char* hostGet;
extern const int sensorType; //SENSORS! -- we support these: 180 for BMP180, 2301 for DHT 2301, 680 for BME680.  0 for no sensor.  No support for multiple sensors for now.
extern const int locationId; //3 really is watchdog
extern const int pollingGranularity; //how often to poll backend in seconds, 4 makes sense
extern const int dataLoggingGranularity; //how often to store data in backend, 300 makes sense
extern const int connectionFailureRetrySeconds;
extern const int connectionRetryNumber;

extern const int granularityWhenInConnectionFailureMode; //40 was too little time for everything to come up and start working reliably, at least with my sketchy cellular connection
extern const int numberOfHotspotRebootsOverLimitedTimeframeBeforeEspReboot; //reboots moxee four times in 340 seconds (number below) and then reboots itself
extern const int hotspotLimitedTimeFrame; //seconds

extern const int hotspotLimitedTimeFrame; //seconds

extern const int hotspotLimitedTimeFrame; //seconds
extern const int moxeePowerSwitch; //usually 14
extern const int deepSleepTimePerLoop; //seconds. doesn't yet work
extern const int dhtData;  
extern const int dhtPower;  
// #define dhType DHT11 // DHT 11
// #define dhType DHT22 // DHT 22, AM2302, AM2321
extern const int dhtType;
extern const char pinsToStartLow[];
