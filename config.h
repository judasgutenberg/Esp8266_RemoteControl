 
  
extern const char* ssid; 
extern const char* password;
extern const char* storage_password; //to ensure someone doesn't store bogus data on your server.
//data posted to remote server so we can keep a historical record
//url will be in the form: http://your-server.com:80/weather/data.php?data=
extern const char* data_source_host;
extern const int weather_device;
extern const int control_device; 
 
extern const int polling_granularity; //how often to poll backend in seconds, 4 makes sense
extern const int reboot_polling_timeout;
extern const int connection_retry_number;
extern const int allowance_for_boot;
extern const int hiatus_length_of_ui_updates_after_user_interaction;

extern const int weather_update_interval;
extern const int energy_update_interval;
 
extern const double temperature_delta_for_change;
extern const double pressure_delta_for_change;
extern const double humidity_delta_for_change;

extern const int backlight_timeout;
