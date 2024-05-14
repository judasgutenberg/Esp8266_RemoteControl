

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
  sensor_id INT NULL
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
  device_type_id INT,
  can_be_input TINYINT,
  can_be_output TINYINT,
  can_be_analog TINYINT,
  pin_number INT,
  via_i2c_address INT,
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
  storage_password VARCHAR(100) NULL;
  energy_api_username VARCHAR(100) NULL;
  energy_api_password VARCHAR(100) NULL;
  energy_api_plant_id INT NULL;
  created DATETIME
);

CREATE TABLE device_feature(
  device_feature_id INT AUTO_INCREMENT PRIMARY KEY,
  device_type_feature_id INT,
  device_type_id INT,
  value INT NULL,
  name VARCHAR(100) NULL,
  description VARCHAR(2000) NULL,
  device_feature ADD enabled TINYINT DEFAULT 0,
  user_id INT NULL,
  created DATETIME,
  modified DATETIME,
  last_known_device_value INT NULL,
  last_known_device_modified DATETIME,
  allow_automatic_management TINY DEFAULT 1
  management_priority INT NULL
);

CREATE TABLE device_feature_management_rule(
  device_feature_id INT,
  management_rule_id INT,
  user_id INT,
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
  battery_power INT NULL
)

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
--ALTER TABLE device_feature ADD allow_automatic_management TINY DEFAULT 1;
--ALTER TABLE device_feature ADD management_priority INT NULL;
--ALTER TABLE device ADD sensor_id INT NULL;
--ALTER TABLE management_rule ADD result_value INT NULL;
--ALTER TABLE management_rule RENAME COLUMN management_script TO conditions;

INSERT INTO device_type (name, architecture, power_voltage, created) VALUES ('NodeMCU', 'ESP8266', 3.3, NOW());
INSERT INTO device (name, device_type_id, created) VALUES ('Hotspot Watchdog', 1, NOW());
INSERT INTO location (location_id, name, device_id, created) VALUES (1, 'outside cabin', 1, NOW());
INSERT INTO location (location_id, name, device_id, created) VALUES (2, 'downstairs cabin', 1, NOW());
INSERT INTO location (location_id, name, device_id, created) VALUES (3, 'upstairs cabin', 1, NOW());

INSERT INTO feature_type (name, created) VALUES ('digital output',  NOW());

INSERT INTO device_type_feature (name, created, can_be_input, can_be_output, can_be_analog, pin_number) VALUES ('D14',  NOW(), 1, 1, 0, 13);
INSERT INTO device_feature (name, created, enabled) VALUES ('Moxee Power Switch',  NOW(), 1);

