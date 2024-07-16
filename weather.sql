

CREATE TABLE weather_data(
  weather_data_id INT AUTO_INCREMENT PRIMARY KEY,
  location_id INT NULL,
  recorded DATETIME,
  temperature DECIMAL(6,3) NULL,
  pressure DECIMAL(9,4) NULL,
  humidity DECIMAL(6,3) NULL,
  wind_direction INT NULL,
  precipitation INT NULL,
  wind_speed DECIMAL(8,3) NULL,
  wind_increment INT NULL,
  gas_metric DECIMAL(15,4) NULL,
  sensor_id INT NULL,
  device_feature_id INT NULL
);

CREATE TABLE reboot_log(
  reboot_log_id INT AUTO_INCREMENT PRIMARY KEY,
  location_id INT,
  recorded DATETIME
);

CREATE TABLE location(
  location_id INT AUTO_INCREMENT PRIMARY KEY,
  device_type_feature_id INT NULL,
  name VARCHAR(100) NULL,
  description VARCHAR(2000) NULL,
  device_id INT NULL,
  i2c_address INT NULL,
  connecting_device_id INT NULL,
  user_id INT NULL,
  created DATETIME
);

CREATE TABLE device_type(
  device_type_id INT AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(100) NULL,
  description VARCHAR(2000) NULL,
  architecture VARCHAR(100) NULL,
  power_voltage DECIMAL(9,3) NULL,
  user_id INT NULL,
  created DATETIME
);

CREATE TABLE device(
  device_id INT AUTO_INCREMENT PRIMARY KEY,
  device_type_id INT,
  name VARCHAR(100) NULL,
  description VARCHAR(2000) NULL,
  user_id INT NULL,
  sensor_id INT NULL,
  latitude DECIMAL(8,5) NULL,
  longitude DECIMAL(8,5) NULL,
  created DATETIME
);

CREATE TABLE feature_type(
  feature_type_id INT AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(100) NULL,
  description VARCHAR(2000) NULL,
  user_id INT NULL,
  created DATETIME
);

CREATE TABLE device_type_feature(
  device_type_feature_id INT AUTO_INCREMENT PRIMARY KEY,
  feature_type_id INT,
  can_be_input TINYINT,
  can_be_output TINYINT,
  can_be_analog TINYINT,
  pin_number INT,
  via_i2c_address INT,
  sensor_type INT DEFAULT NULL,
  sensor_sub_type INT DEFAULT NULL,
  power_pin INT NULL,
  name VARCHAR(100) NULL,
  description VARCHAR(2000) NULL,
  user_id INT NULL,
  created DATETIME
  
);

CREATE TABLE user(
  user_id INT AUTO_INCREMENT PRIMARY KEY,
  email VARCHAR(100) NULL,
  password VARCHAR(100) NULL,
  expired DATETIME NULL,
  preferences TEXT NULL,
  storage_password VARCHAR(100) NULL,
  energy_api_username VARCHAR(100) NULL,
  energy_api_password VARCHAR(100) NULL,
  energy_api_plant_id INT NULL,
  open_weather_api_key VARCHAR(100) NULL,
  created DATETIME
);

CREATE TABLE device_feature(
  device_feature_id INT AUTO_INCREMENT PRIMARY KEY,
  device_type_feature_id INT,
  device_type_id INT,
  value INT  DEFAULT NULL,
  name VARCHAR(100) NULL,
  description VARCHAR(2000) NULL,
  enabled TINYINT DEFAULT 0,
  user_id INT NULL,
  created DATETIME,
  modified DATETIME,
  last_known_device_value INT NULL,
  last_known_device_modified DATETIME,
  allow_automatic_management TINYINT DEFAULT 1
);

CREATE TABLE device_feature_management_rule(
  device_feature_id INT,
  management_rule_id INT,
  user_id INT,
  management_priority INT DEFAULT 1,
  created DATETIME
)


CREATE TABLE device_feature_log(
  device_feature_log_id INT AUTO_INCREMENT PRIMARY KEY,
  device_feature_id INT,
  user_id INT,
  recorded DATETIME,
  beginning_state INT,
  end_state INT,
  management_rule_id INT NULL,
  mechanism VARCHAR(20) NULL
)

CREATE TABLE management_rule(
  management_rule_id  INT AUTO_INCREMENT PRIMARY KEY,
  user_id INT,
  name VARCHAR(50),
  result_value INT DEFAULT 1,
  description VARCHAR(2000),
  time_valid_start TIME NULL,
  time_valid_end TIME NULL,
  conditions TEXT,
  created DATETIME
)

CREATE TABLE inverter_log(
  inverter_log_id  INT AUTO_INCREMENT PRIMARY KEY,
  user_id INT,
  recorded DATETIME,
  solar_power INT NULL,
  load_power INT NULL,
  grid_power INT NULL,
  battery_percentage INT NULL,
  battery_power INT NULL,
  battery_voltage DECIMAL(8,5) NULL,
  solar_potential INT NULL
  
  mystery_value1 INT NULL,
  mystery_value2 INT NULL,
  changer1 INT NULL,
  changer2 INT NULL,
  changer3 INT NULL,
  changer4 INT NULL,
  changer5 INT NULL,
  changer6 INT NULL,
  changer7 INT NULL
)

CREATE TABLE report(
  report_id  INT AUTO_INCREMENT PRIMARY KEY,
  user_id INT,
  name VARCHAR(100),
  created DATETIME,
  modified DATETIME,
  form TEXT NULL,
  `sql` TEXT NULL
);

CREATE TABLE report_log(
  report_log_id  INT AUTO_INCREMENT PRIMARY KEY,
  report_id INT,
  user_id INT,
  run DATETIME,
  data TEXT NULL,
  records_returned int,
  runtime int
);


--fixes for older versions of the schema:
--ALTER TABLE device_feature ADD enabled TINYINT DEFAULT 0;
--ALTER TABLE location ADD device_id INT NULL;
--ALTER TABLE location ADD device_type_feature_id INT NULL;
--ALTER TABLE location ADD i2c_address INT NULL;
--ALTER TABLE location ADD connecting_device_id INT NULL;
--ALTER TABLE location ADD user_id INT NULL;
--ALTER TABLE device_type ADD user_id INT NULL;
--ALTER TABLE device ADD user_id INT NULL;
--ALTER TABLE feature_type ADD user_id INT NULL;
--ALTER TABLE device_type_feature ADD user_id INT NULL;
--ALTER TABLE device_feature ADD user_id INT NULL;
--ALTER TABLE device_feature ADD last_known_device_value INT NULL;
--ALTER TABLE device_feature ADD last_known_device_modified DATETIME;
--ALTER TABLE user ADD preferences TEXT NULL;
--ALTER TABLE user ADD storage_password VARCHAR(100) NULL;
--ALTER TABLE device_type_feature ADD via_i2c_address INT NULL;
--ALTER TABLE user ADD energy_api_username VARCHAR(100) NULL;
--ALTER TABLE user ADD energy_api_password VARCHAR(100) NULL;
--ALTER TABLE user ADD energy_api_plant_id INT NULL;
--ALTER TABLE weather_data ADD sensor_id INT NULL;
--ALTER TABLE device_feature ADD allow_automatic_management TINYINT DEFAULT 1;
--ALTER TABLE device_feature ADD management_priority INT NULL;
--ALTER TABLE device ADD sensor_id INT NULL;
--ALTER TABLE management_rule ADD result_value INT NULL;
--ALTER TABLE management_rule RENAME COLUMN management_script TO conditions;
--ALTER TABLE device_feature_management_rule ADD management_priority INT DEFAULT 1;
--ALTER TABLE device_type_feature RENAME COLUMN device_type_id TO feature_type_id;

--ALTER TABLE device_type_feature ADD  sensor_type INT DEFAULT NULL;
--ALTER TABLE device_type_feature ADD  sensor_sub_type INT DEFAULT NULL;
--ALTER TABLE device_type_feature ADD  power_pin INT DEFAULT NULL;
 
--ALTER TABLE device_feature MODIFY value INT DEFAULT NULL;

--ALTER TABLE device ADD latitude DECIMAL(8,5) NULL;
--ALTER TABLE device ADD longitude DECIMAL(8,5) NULL;
--ALTER TABLE user ADD open_weather_api_key VARCHAR(100) NULL;
--ALTER TABLE weather_data ADD device_feature_id INT NULL;
--ALTER TABLE inverter_log ADD battery_voltage DECIMAL(8,5) NULL;
--ALTER TABLE inverter_log ADD solar_potential INT NULL;


  ALTER TABLE inverter_log ADD mystery_value1 INT NULL;
  ALTER TABLE inverter_log ADD mystery_value2 INT NULL;
  ALTER TABLE inverter_log ADD changer1 INT NULL;
  ALTER TABLE inverter_log ADD changer2 INT NULL;
  ALTER TABLE inverter_log ADD changer3 INT NULL;
  ALTER TABLE inverter_log ADD changer4 INT NULL;
  ALTER TABLE inverter_log ADD changer5 INT NULL;
  ALTER TABLE inverter_log ADD changer6 INT NULL;
  ALTER TABLE inverter_log ADD changer7 INT NULL;

INSERT INTO device_type (name, architecture, power_voltage, created) VALUES ('NodeMCU', 'ESP8266', 3.3, NOW());
INSERT INTO device (name, device_type_id, created) VALUES ('Hotspot Watchdog', 1, NOW());
INSERT INTO location (location_id, name, device_id, created) VALUES (1, 'outside cabin', 1, NOW());
INSERT INTO location (location_id, name, device_id, created) VALUES (2, 'downstairs cabin', 1, NOW());
INSERT INTO location (location_id, name, device_id, created) VALUES (3, 'upstairs cabin', 1, NOW());

INSERT INTO feature_type (name, created) VALUES ('digital output',  NOW());

INSERT INTO device_type_feature (name, created, can_be_input, can_be_output, can_be_analog, pin_number) VALUES ('D14',  NOW(), 1, 1, 0, 13);
INSERT INTO device_feature (name, created, enabled) VALUES ('Moxee Power Switch',  NOW(), 1);

