#ifndef CONFIG_H
#define CONFIG_H
// ===== AUTO-GENERATED CONFIGURATION INDEX DEFINITIONS =====

// String configuration options (0–11)
#define STORAGE_PASSWORD 0
#define URL_GET 1
#define HOST_GET 2
#define ENCRYPTION_SCHEME 3
#define SENSOR_CONFIG_STRING 4
#define PINS_TO_START_LOW 5
#define WIFI_SSID 6
#define WIFI_PASSWORD 7
#define WIFI_SSID_1 8
#define WIFI_PASSWORD_1 9
#define WIFI_SSID_2 10
#define WIFI_PASSWORD_2 11

// Integer configuration options (12+)
#define SENSOR_ID 16
#define SENSOR_I2C 17
#define CONSOLIDATE_ALL_SENSORS_TO_ONE_RECORD 18
#define DEVICE_ID 19
#define POLLING_GRANULARITY 20
#define DATA_LOGGING_GRANULARITY 21
#define OFFLINE_LOG_GRANULARITY 22
#define WIFI_TIMEOUT 23
#define OFFLINE_RECONNECT_INTERVAL 24
#define FRAM_INDEX_SIZE 25
#define FRAM_LOG_TOP 26
#define CONNECTION_FAILURE_RETRY_SECONDS 27
#define CONNECTION_RETRY_NUMBER 28
#define GRANULARITY_WHEN_IN_MOXEE_PHASE_0  29
#define GRANULARITY_WHEN_IN_MOXEE_PHASE_1  30

#define NUMBER_OF_HOTSPOT_REBOOTS_OVER_LIMITED_TIMEFRAME_BEFORE_ESP_REBOOT 31
#define HOTSPOT_LIMITED_TIME_FRAME 32
#define MOXEE_POWER_SWITCH 33
#define DEEP_SLEEP_TIME_PER_LOOP 34
#define LIGHT_SLEEP_TIME_PER_LOOP 35
#define SENSOR_DATA_PIN 36
#define SENSOR_POWER_PIN 37
#define SENSOR_SUB_TYPE 38
#define IR_PIN 39
#define INA219_ADDRESS 40
#define FRAM_ADDRESS 41
#define RTC_ADDRESS 42
#define SLAVE_I2C 43
#define SLAVE_PET_WATCHDOG_COMMAND 44

#define CONFIG_STRING_COUNT 16
#define CONFIG_TOTAL_COUNT 45
// ==========================================================


extern char* cs[CONFIG_STRING_COUNT]; // string configuration values
extern int ci[CONFIG_TOTAL_COUNT];    // integer configuration values

void initConfig(void);


#endif
