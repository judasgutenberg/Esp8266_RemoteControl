#include "globals.h"
#include "config.h"
#include <SimpleMap.h>



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
#include <IRremoteESP8266.h>
IRsend irsend(ci[IR_PIN]); 
Adafruit_INA219* ina219;
Adafruit_VL53L0X lox[4];
Adafruit_FRAM_I2C fram;



//SEND DATA TO A REMOTE SERVER TO STORE IN A DATABASE----------------------------------------------------
// ---------- Non-blocking sendRemoteData() state machine -------------
//
// Usage:
//   startRemoteTask(datastring, mode, fRAMordinal);
//   // call runRemoteTask() frequently from loop()
// ---------------------------------------------------------------------

// Configurable timings (tweak as desired)
const unsigned long CONNECT_RETRY_SPACING_MS = 200;   // spacing between connect attempts (non-blocking)
const unsigned long CONNECT_TIMEOUT_MS = 5000;       // per-connection wait for server (was ~10s, shortened)
const unsigned long REPLY_AVAIL_TIMEOUT_MS = 10000;  // wait for first byte from server
const unsigned long MAX_RESPONSE_SIZE = 32 * 1024UL; // safety cap to avoid runaway memory use


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
uint8_t outputMode = 0;
String responseBuffer = "";
unsigned long lastPet = 0;
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

byte parsedSerialData[64];
bool slaveUnpetted = true;
uint8_t lastSlavePowerMode = 0;
ESP8266WebServer server(80); //Server on port 80
