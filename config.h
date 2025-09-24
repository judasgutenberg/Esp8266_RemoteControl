 
  
extern const char* wifi_ssid; //mine was Moxee Hotspot83_2.4G
extern const char* wifi_password;
extern const char* storage_password; //to ensure someone doesn't store bogus data on your server. should match value in config.php
extern const unsigned long long encryption_scheme;
//data posted to remote server so we can keep a historical record
//url will be in the form: http://your-server.com:80/weather/data.php?data=
extern const char* url_get;
extern const char* host_get;
extern const char* sensor_config_string;
extern const int sensor_id; //SENSORS! -- we support these: 7410 for ADT7410, 2320 for AHT20, 75 for LM75, 85 for BMP085, 180 for BMP180, 2301 for DHT 2301, 680 for BME680.  0 for no sensor. Multiple sensors are possible.
extern const int sensor_i2c;
extern const int consolidate_all_sensors_to_one_record;
extern const int device_id; //3 really is watchdog
extern int polling_granularity; //how often to poll backend in seconds, 4 makes sense
extern int data_logging_granularity; //how often to store data in backend, 300 makes sense
extern int offline_log_granularity; //will usually be paired with a value in fram_address
extern int wifi_timeout;
extern int offline_reconnect_interval;
extern const int fram_index_size;
extern const int fram_log_top;

extern const int connection_failure_retry_seconds;
extern const int connection_retry_number;

extern const int granularity_when_in_moxee_phase_0; //phase 0 is when it's just showing battery levels and is useless. reboot immediately!
extern const int granularity_when_in_moxee_phase_1; //phase 1 is operational.  let it linger when failed
extern const int number_of_hotspot_reboots_over_limited_timeframe_before_esp_reboot; //reboots moxee four times in 340 seconds (number below) and then reboots itself
extern const int hotspot_limited_time_frame; //seconds

extern const int moxee_power_switch; //usually 14
extern int deep_sleep_time_per_loop; //seconds. 
extern int light_sleep_time_per_loop;

extern const int sensor_data_pin;  
extern const int sensor_power_pin;  
// #define dhType DHT11 // DHT 11
// #define dhType DHT22 // DHT 22, AM2302, AM2321
extern const int sensor_sub_type;

extern const char pins_to_start_low[];

extern const int ir_pin; 
extern const int ina219_address;
extern const int fram_address; //will usually be paired with a value in offline_log_granularity as a way to store offline log data

extern const int rtc_address;

extern int slave_i2c;
extern const int slave_pet_watchdog_command;

