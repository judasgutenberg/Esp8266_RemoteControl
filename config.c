
const char* wifi_ssid = "your-ssid"; //mine was Moxee Hotspot83_2.4G
const char* wifi_password = "your-wifi-password";
const char* storage_password = "your-storage-password"; //to ensure someone doesn't store bogus data on your server. should match value in the storage_password column in you user table for your user
const unsigned long long encryption_scheme = 0x1234567890ABCDEF; //formerly called "salt"
//data posted to remote server so we can keep a historical record
//url will be in the form: http://your-server.com:80/weather/server.php?data=
const char* url_get = "/weather/server.php";
const char* host_get = "your-server.com";
const char* sensor_config_string = ""; "0*-1*1*99*NULL*NULL*hotwater*11*1"; //<--that places an additional value at reserved4 in device_log; this is an easy way to specify multiple sensors. the format is: dataPin*powerPin*sensorType*sensorSubType*i2c_address*device_feature_id*name*ordinalOfOverwrite*consolidate_all_sensors_to_one_record|next_sensor...
const int sensor_id = 280; //SENSORS! -- we support these: 7410 for ADT7410, 2320 for AHT20, 75 for LM75, 85 for BMP085, 180 for BMP180, 2301 for DHT 2301, 680 for BME680.  0 for no sensor. Multiple sensors are possible.
const int sensor_i2c = 0x76;
const int consolidate_all_sensors_to_one_record = 1;
const int device_id = 11; //3 really is watchdog
int polling_granularity = 4; //how often to poll backend in seconds, 4 makes sense
int data_logging_granularity = 300; //how often to store data in backend, 300 makes sense
int offline_log_granularity = 100;
int wifi_timeout = 100; //normally 100 in seconds
int offline_reconnect_interval = 20;
const int fram_index_size = 256;
const int fram_log_top = 28672; //if we have a FRAM, we should partition it so the last 4k can be used to store various values like Moxee Reboot times, e

const int connection_failure_retry_seconds = 4;
const int connection_retry_number = 22;

const int granularity_when_in_moxee_phase_0 = 3; //phase 0 is when it's just showing battery levels and is useless. reboot immediately!
const int granularity_when_in_moxee_phase_1 = 17; //phase 1 is operational
const int number_of_hotspot_reboots_over_limited_timeframe_before_esp_reboot = 4; //reboots moxee four times in 340 seconds (number below) and then reboots itself
const int hotspot_limited_time_frame = 340; //seconds

const int moxee_power_switch = 13; //usually 14 set to -1 if not used

int deep_sleep_time_per_loop = 0;  //in seconds. saves energy.  set to zero if unneeded. GPIO16/D0 needs to be connected to RST in hardware first. can be changed remotely.  all state is lost when sleeping
int light_sleep_time_per_loop = 0; //GPIO pins will hold state and so will variables through this

//if you are using a DHT hygrometer/temperature probe, these values will be important
//particularly if you reflashed a MySpool temperature probe (https://myspool.com/) with custom firmware
//on those, the sensorData is 14 and sensorPower is 12
const int sensor_data_pin = -1; 
const int sensor_power_pin = 12;
// #define dhType DHT11 // DHT 11
// #define dhType DHT22 // DHT 22, AM2302, AM2321
const int sensor_sub_type = 21; // DHT 21, AM2301

const char pins_to_start_low[] = {12, 13, -1}; //so when the device comes up it doesn't immediately turn on attached devices

const int ir_pin = 14; //set to the value of an actual data pin if you want to send data via ir from your controller on occasion
const int ina219_address = 0x40; //set to -1 if you have no power voltage to monitor
const int fram_address = 0x50; //used for storing offline logged data, etc. Make sure to also set offline_log_granularity so offline data can be stored in the FRAM
const int rtc_address = 0x68;  //for offline timestamping using a DS3231 or a DS1307; in many cases unnecessary  

const int slave_i2c = 20;
int slave_pet_watchdog_command = 203; //sets the watchdog to require petting every 1000 seconds and makes sure to pet.  202 is once every 100 seconds and 204 is once every 10000 seconds
