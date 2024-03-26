<?php 
//temperaturebot backend. 
//i've tried to keep all the code vanilla and old school
//of course in php it's all kind of bleh
//gus mueller, April 7 2022
//////////////////////////////////////////////////////////////

//ini_set('display_errors', 1);
//ini_set('display_startup_errors', 1);
//error_reporting(E_ALL);

include("config.php");
disable_gzip();
$conn = mysqli_connect($servername, $username, $password, $database);
$method = "none";
$mode = "";
$out = [];
$date = new DateTime("now", new DateTimeZone('America/New_York'));//obviously, you would use your timezone, not necessarily mine
$formatedDateTime =  $date->format('Y-m-d H:i:s');
//$formatedDateTime =  $date->format('H:i');

if($_REQUEST) {
	if(array_key_exists("mode", $_REQUEST)) {
		$mode = $_REQUEST["mode"];
	} else {
		$mode = "getData";
	}
	//we had been passing in a locationId but we have since 
	//changed that to deviceId. locationId is to support
	//the devices still passing in locationId
	if(array_key_exists("locationId", $_REQUEST)) {
		$deviceId = $_REQUEST["locationId"];
	} else if (array_key_exists("deviceId", $_REQUEST)) {
		$deviceId = $_REQUEST["deviceId"];
	}
	if(!$conn) {
        $out = ["error"=>"bad database connection"];
      } else {
		if(array_key_exists("data", $_REQUEST)) {
			$data = $_REQUEST["data"];
        	$lines = explode("|",$data);
			$weatherInfoString = $lines[0];
        	$arrWeatherData = explode("*", $weatherInfoString);
		} else {
			$lines = [];
			$arrWeatherData = [0,0,0,0,0,0,0,0,0,0,0,0];
		}
		if($mode=="kill") {
			$method  = "kill";
		
		} else if ($mode=="getDeviceData") {



		} else if ($mode=="getData") {
			if(array_key_exists("scale", $_REQUEST)) {
				$scale = $_REQUEST["scale"];
			} else {
				$scale = "";
			}
			
			if(!$conn) {
				$out = ["error"=>"bad database connection"];
			} else {
				
				if($scale == ""  || $scale == "fine") {
					$sql = "SELECT * FROM " . $database . ".weather_data  
					WHERE recorded > DATE_ADD(NOW(), INTERVAL -1 DAY) AND location_id=" . $deviceId . " 
					ORDER BY weather_data_id ASC";
				} else {
					if($scale == "hour") {
						$sql = "SELECT
						*,
						YEAR(recorded), DAYOFYEAR(recorded), HOUR(recorded) FROM " . $database . ".weather_data  
						WHERE recorded > DATE_ADD(NOW(), INTERVAL -7 DAY) AND location_id=" . $deviceId . " 
							GROUP BY YEAR(recorded), DAYOFYEAR(recorded), HOUR(recorded)
							ORDER BY weather_data_id ASC";
					}
					if($scale == "day") {
						$sql = "SELECT 	 
						*,
						YEAR(recorded), DAYOFYEAR(recorded) FROM " . $database . ".weather_data  
						WHERE location_id=" . $deviceId . " 
							GROUP BY YEAR(recorded), DAYOFYEAR(recorded)
							ORDER BY weather_data_id ASC";
					}
				}
				/*
				//using averages didn't work for some reason:
				weather_data_id, 
				recorded, 
				AVG(temperature) AS temperature, 
				AVG(pressure) AS pressure, 
				AVG(humidity) AS humidity, 
				wind_direction, 
				AVG(precipitation) AS precipitation, 
				wind_increment, 
				*/
				//echo $sql;
				$result = mysqli_query($conn, $sql);
				$out = [];
				if($result) {
					while($row = mysqli_fetch_array($result)) {
						array_push($out, $row);
					}
				}
			}
			$method  = "read";	
		} else if ($mode == "saveData") { //save data
		//test url;:
		// //http://randomsprocket.com/weather/data.php?storagePassword=butterfly&locationId=3&mode=saveData&data=10736712.76*12713103.20*1075869.28*NULL|0*0*1710464489*1710464504*1710464519*1710464534*1710464549*1710464563*1710464579*1710464593*

			
			
			
			$temperature = $arrWeatherData[0];
			$pressure = $arrWeatherData[1];
			$humidity = $arrWeatherData[2];
			$gasMetric = "NULL";
			if(count($arrWeatherData)>3) {
			$gasMetric = $arrWeatherData[3];
			}
			
			//select * from weathertron.weather_data where location_id=3 order by recorded desc limit 0,10;
			$weatherSql = "INSERT INTO weather_data(location_id, recorded, temperature, pressure, humidity, gas_metric, wind_direction, precipitation, wind_speed, wind_increment) VALUES (" . 
			mysqli_real_escape_string($conn, $deviceId) . ",'" .  
			mysqli_real_escape_string($conn, $formatedDateTime)  . "'," . 
			mysqli_real_escape_string($conn, $temperature) . "," . 
			mysqli_real_escape_string($conn, $pressure) . "," . 
			mysqli_real_escape_string($conn, $humidity) . "," . 
			mysqli_real_escape_string($conn, $gasMetric) .
			",NULL,NULL,NULL,NULL)";
			//echo $weatherSql;

			if($temperature != "NULL") { //if temperature is null, do not attempt to store!
				if(array_key_exists("storagePassword", $_REQUEST) && $storagePassword == $_REQUEST["storagePassword"]) { //prevents malicious data corruption
				$result = mysqli_query($conn, $weatherSql);
				}
			}
			$method  = "insert";
			$out = Array("message" => "done", "method"=>$method);
		

		}
		if($mode == "getDeviceData" || $mode == "saveData" ) {
			$pinValuesKnownToDevice = [];
			$specificPin = -1;
			if(count($lines)>1) {
				$recentReboots = explode("*", $lines[1]);
				foreach($recentReboots as $rebootOccasion) {
					if(intval($rebootOccasion) > 0 && array_key_exists("storagePassword", $_REQUEST) && $storagePassword == $_REQUEST["storagePassword"]) {
					$dt = new DateTime();
					$dt->setTimestamp($rebootOccasion);
					$rebootOccasionSql = $dt->format('Y-m-d H:i:s');
					$rebootLogSql = "INSERT INTO reboot_log(location_id, recorded) SELECT " . intval($deviceId) . ",'" .$rebootOccasionSql . "' 
						FROM DUAL WHERE NOT EXISTS (SELECT * FROM reboot_log WHERE location_id=" . intval($deviceId) . " AND recorded='" . $rebootOccasionSql . "' LIMIT 1)";
					
					$result = mysqli_query($conn, $rebootLogSql);
					}
				}
				if(count($lines) > 2) {
					$pinValuesKnownToDevice = explode("*", $lines[2]);

				}
				if(count($lines) > 3) {
					$extraInfo = explode("*", $lines[3]);
					$lastCommandId = $extraInfo[0];
					$specificPin = $extraInfo[1];
				}
			
			} 

			//the part where we include any data from our remote control system:
			$deviceSql = "SELECT pin_number, value, enabled, can_be_analog, device_feature_id  FROM device_feature f LEFT JOIN device_type_feature t ON f.device_type_feature_id=t.device_type_feature_id WHERE device_id=" . intval($deviceId) . ";";
			//echo $deviceSql;
			$result = mysqli_query($conn, $deviceSql);
			if($result) {
				$rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
				$pinCursor = 0;
				foreach($rows as $row) {
					$pinNumber = $row["pin_number"];
					if($specificPin == -1 || $specificPin ==  $pinNumber ){
						if(count($pinValuesKnownToDevice)>0) {
							//this part update device_feature so we can tell from the server if the device has taken on the server's value
							$sqlToUpdateDeviceFeature = "UPDATE device_feature SET last_known_device_value =  " . $pinValuesKnownToDevice[$pinCursor];
							$sqlToUpdateDeviceFeature .= ", last_known_device_modified='" . $formatedDateTime . "'";
							$sqlToUpdateDeviceFeature .= " WHERE device_feature_id=" . $row["device_feature_id"];
							$updateResult = mysqli_query($conn, $sqlToUpdateDeviceFeature);
						}
						unset($row["device_feature_id"]);//make things as lean as possible for IoT device
						$out["device_data"][] = $row;
					}
					$out["pin_list"][] = $pinNumber ;
					$pinCursor++;
				}
			}
			
		}
	}
	echo json_encode($out);
	
	
} else {
	echo '{"message":"done", "method":"' . $method . '"}';
}

function disable_gzip() {
	header('Content-Encoding: identity');
	@ini_set('zlib.output_compression', 'Off');
	@ini_set('output_buffering', 'Off');
	@ini_set('output_handler', '');
	@ini_set('DeflateBufferSize', 8192);
	@ini_set('no-gzip', 'On');
	@ini_set('fastcgi_pass_request_headers', 'Off');
	@apache_setenv('no-gzip', 1);	
}

//some helpful sql examples for creating sql users:
//CREATE USER 'weathertron'@'localhost' IDENTIFIED  BY 'your_password';
//GRANT CREATE, ALTER, DROP, INSERT, UPDATE, DELETE, SELECT, REFERENCES, RELOAD on *.* TO 'weathertron'@'localhost' WITH GRANT OPTION;
//GRANT ALL PRIVILEGES ON *.* TO 'weathertron'@'localhost' WITH GRANT OPTION;
 
