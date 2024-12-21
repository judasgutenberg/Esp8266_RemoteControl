
 
  
extern const char* wifi_ssid;  
extern const char* wifi_password;
extern const char* storage_password;  
extern const char* url_get;
extern const char* host_get;
extern const char* sensor_config_string;
extern const int sensor_id; //SENSORS! -- we support these: 75 for LM75, 85 for BMP085, 180 for BMP180, 2301 for DHT 2301, 680 for BME680.  0 for no sensor.   
extern const int sensor_i2c;
extern const int consolidate_all_sensors_to_one_record;
extern const int device_id; //3 really is watchdog
extern const int polling_granularity; //how often to poll backend in seconds, 4 makes sense
extern const int data_logging_granularity; //how often to store data in backend, 300 makes sense
extern const int connection_failure_retry_seconds;
extern const int connection_retry_number;

extern const int granularity_when_in_connection_failure_mode; //40 was too little time for everything to come up and start working reliably, at least with my sketchy cellular connection
extern const int number_of_hotspot_reboots_over_limited_timeframe_before_esp_reboot; //reboots moxee four times in 340 seconds (number below) and then reboots itself
extern const int hotspot_limited_time_frame; //seconds

extern const int moxee_power_switch; //usually 14
extern const int deep_sleep_time_per_loop; //seconds. doesn't yet work

extern const int sensor_data_pin;  
extern const int sensor_power_pin;  
// #define dhType DHT11 // DHT 11
// #define dhType DHT22 // DHT 22, AM2302, AM2321
extern const int sensor_sub_type;
extern const int ir_pin; 
extern const int ina219_address;
extern const char pins_to_start_low[];
