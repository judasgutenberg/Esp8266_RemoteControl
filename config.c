
const char* ssid = "your_ssid"; //mine was Moxee Hotspot83_2.4G
const char* password = "your_wifi_password";
const char* storage_password = "your_storage_password"; //to ensure someone doesn't store bogus data on your server. should match value in the storage_password column in you user table for your user
//data posted to remote server so we can keep a historical record
//url will be in the form: http://your-server.com:80/weather/data.php?data=
const char* data_source_host = "your-server.com";
const int weather_device  = 13;
const int control_device  = 11; 
const int polling_granularity = 2; //how often to poll backend in seconds, 4 makes sense
const int reboot_polling_timeout = 80; //if no polling in 300 seconds, reboot the ESP
const int connection_retry_number = 22;
const int allowance_for_boot = 70;
const int hiatus_length_of_ui_updates_after_user_interaction = 35;

const int weather_update_interval = 67;
const int energy_update_interval = 116;

const double temperature_delta_for_change = 0.2;
const double pressure_delta_for_change = 0.5;
const double humidity_delta_for_change = 0.3;

const int backlight_timeout = 600; //how long the backlight stays lit after we start up or a button is pushed.  the backlight uses 100 times more power than the rest of the LCD module
 
