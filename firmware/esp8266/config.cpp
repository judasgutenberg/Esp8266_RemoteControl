#include "config.h"
#include "globals.h"

char* cs[CONFIG_STRING_COUNT]; //used for string configurations
int ci[CONFIG_TOTAL_COUNT]; //used for int configurations

void initConfig(void) {
  cs[STORAGE_PASSWORD] = "your_secret";  // must match storage_password in backend
  cs[URL_GET] = "/weather/server.php";
  cs[HOST_GET] = "your_server.com";
  cs[ENCRYPTION_SCHEME] = "ABCDEF0123456789"; // 16-char hex string
  cs[SENSOR_CONFIG_STRING] = ""; // "14*-1*2301*11*0*NULL*stank*1";
  cs[PINS_TO_START_LOW] = ""; 
  cs[WIFI_SSID] = "your_ssid";
  cs[WIFI_PASSWORD] = "password";
  cs[WIFI_SSID_1] = "your_other_ssid";
  cs[WIFI_PASSWORD_1] = "could by blank";
  cs[WIFI_SSID_2] = "wow_a_third_ssid";
  cs[WIFI_PASSWORD_2] = "";
  
  ci[SENSOR_ID] = 280;          // e.g. 2301 = DHT2301
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
  ci[INA219_ADDRESS] = 0;//0x40;
  ci[FRAM_ADDRESS] = 0;//0x50;
  ci[RTC_ADDRESS] = 0;//0x68;
  ci[SLAVE_I2C] = 20;
  ci[SLAVE_PET_WATCHDOG_COMMAND] = 203;
}
