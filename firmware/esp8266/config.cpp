#include "config.h"
#include "globals.h"
char* cs[CONFIG_STRING_COUNT]; //used for string configurations
int ci[CONFIG_TOTAL_COUNT]; //used for int configurations

void initConfig(void) {
  cs[STORAGE_PASSWORD] = "your_storage_password";  // must match storage_password in backend
  cs[URL_GET] = "/weather/server.php";
  cs[HOST_GET] = "yourwebsite.com";
  cs[ENCRYPTION_SCHEME] = "87447CBC22A95125"; // 16-char hex string
  cs[SENSOR_CONFIG_STRING] = ""; // "14*-1*2301*11*0*NULL*stank*1";
  cs[PINS_TO_START_LOW] = ""; // encode as hex if needed
  cs[WIFI_SSID] = "your_ssid1";
  cs[WIFI_PASSWORD] = "12345678";
  cs[WIFI_SSID_1] = "your_ssid2";
  cs[WIFI_PASSWORD_1] = "12345678";
  cs[WIFI_SSID_2] = "your_ssid3";
  cs[WIFI_PASSWORD_2] = "12345678";
  
  ci[SENSOR_ID] = 280;          //we support these: 7410 for ADT7410, 2320 for AHT20, 75 for LM75, 85 for BMP085, 180 for BMP180, 2301 for DHT 2301, 680 for BME680.  0 for no sensor. Multiple sensors are possible.
  ci[SENSOR_I2C] = 0x76;
  ci[CONSOLIDATE_ALL_SENSORS_TO_ONE_RECORD] = 1;
  ci[DEVICE_ID] = 11;
  ci[POLLING_GRANULARITY] = 4;
  ci[DATA_LOGGING_GRANULARITY] = 300;
  ci[OFFLINE_LOG_GRANULARITY] = 100;
  ci[WIFI_TIMEOUT] = 100;
  ci[OFFLINE_RECONNECT_INTERVAL] = 20;
  ci[FRAM_INDEX_SIZE] = 256;
  ci[FRAM_LOG_TOP] = 28672;
  ci[CONNECTION_FAILURE_RETRY_SECONDS] = 4;
  ci[CONNECTION_RETRY_NUMBER] = 22;
  ci[GRANULARITY_WHEN_IN_MOXEE_PHASE_0] = 3;
  ci[GRANULARITY_WHEN_IN_MOXEE_PHASE_1] = 17;
  ci[NUMBER_OF_HOTSPOT_REBOOTS_OVER_LIMITED_TIMEFRAME_BEFORE_ESP_REBOOT] = 4;
  ci[HOTSPOT_LIMITED_TIME_FRAME] = 340;
  ci[MOXEE_POWER_SWITCH] = 0;
  ci[DEEP_SLEEP_TIME_PER_LOOP] = 0;
  ci[LIGHT_SLEEP_TIME_PER_LOOP] = 0;
  ci[SENSOR_DATA_PIN] = -1;
  ci[SENSOR_POWER_PIN] = 12;
  ci[SENSOR_SUB_TYPE] = 21;
  ci[IR_PIN] = 14;
  ci[INA219_ADDRESS] = 0x40;
  ci[FRAM_ADDRESS] = 0x50;
  ci[RTC_ADDRESS] = 0x68;
  ci[SLAVE_I2C] = 0;
  ci[SLAVE_PET_WATCHDOG_COMMAND] = 0;
  ci[DEBUG] = 1;
  ci[POLLING_SKIP_LEVEL] = 50;
  ci[LOCAL_WEB_SERVICE_RESPONSIVENESS] = 1;
  ci[BAUD_RATE_LEVEL] = 9;
}


void initSlaveDefaults() {                                      
  css[0] = "Characteristic #2;0x3ffbb61c;0x3ffbb5fc;4;4;6;7;8;9;10;11;0x3ffbb60c;0;1";
  css[1] = "Characteristic #7;I (318800102);0x3ffbb5bc;6;7";
  css[2] = "Characteristic #9;I (28318992);0x3ffbb64c;0;1";

  cis[SLAVE_POWER_MODE] = 2;
  cis[SLAVE_PARSE_MODE] = 0;
  cis[SLAVE_BAUD_RATE_LEVEL] = 9;
  cis[SLAVE_I2C_ADDRESS] = 20;
  cis[SLAVE_REBOOT_PIN] = 7;
  cis[SLAVE_SERIAL_MODE] = 2;
 
}
