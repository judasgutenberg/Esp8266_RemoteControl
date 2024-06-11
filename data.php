<?php 
//remote control and weather monitoring backend. 
//i've tried to keep all the code vanilla and old school
//of course in php it's all kind of bleh
//gus mueller, April 14 2024
//////////////////////////////////////////////////////////////

//ini_set('display_errors', 1);
//ini_set('display_startup_errors', 1);
//error_reporting(E_ALL);

include("config.php");
include("site_functions.php");
include("device_functions.php");
disable_gzip();
$conn = mysqli_connect($servername, $username, $password, $database);
$method = "none";
$mode = "";
$out = [];
$date = new DateTime("now", new DateTimeZone('America/New_York'));//obviously, you would use your timezone, not necessarily mine
$pastDate = $date;
$formatedDateTime =  $date->format('Y-m-d H:i:s');
$currentTime = $date->format('H:i:s');
$pastDate->modify('-20 minutes');
$formatedDateTime20MinutesAgo =  $pastDate->format('Y-m-d H:i:s');
//$formatedDateTime =  $date->format('H:i');
$deviceId = "";
$locationId = "";
$deviceName = "Your device";
$deviceIds = [];
$sensorId = "NULL";
$nonJsonPinData = 0;
$justGetDeviceInfo = 0;
if($_REQUEST) {
	if(array_key_exists("storagePassword", $_REQUEST)) {
		$deviceIds = deriveDeviceIdsFromStoragePassword($_REQUEST["storagePassword"]);
	}
	if(array_key_exists("mode", $_REQUEST)) {
		$mode = $_REQUEST["mode"];
	} else {
		$mode = "getData";
	}
	//i may end up deciding that locationId and deviceId are the same thing, and for now they are
	//but maybe some day they will be different things
	if(array_key_exists("locationId", $_REQUEST)) {
		$locationId = $_REQUEST["locationId"];
	}
	if (array_key_exists("deviceId", $_REQUEST)) {
		$deviceId = $_REQUEST["deviceId"];
	}
	if(!$locationId) {
		$locationId =  $deviceId;
	}
	if(!$deviceId) {
		$deviceId = $locationId;
	}
	if($locationId == ""){
		$locationId = 1;
	}
	if(!$conn) {
        $out = ["error"=>"bad database connection"];
      } else {
		if(array_key_exists("data", $_REQUEST)) {
			$data = $_REQUEST["data"];
        	$lines = explode("|",$data);
			$weatherInfoString = $lines[0];
			
        	$arrWeatherData = explode("*", $weatherInfoString);
			/*
			$temperature = $arrWeatherData[0];
			$pressure = $arrWeatherData[1];
			$humidity = $arrWeatherData[2];
			$gasMetric = "NULL";
			
			if(count($arrWeatherData)>3) {
				$gasMetric = $arrWeatherData[3];
			}
			*/
			if(count($arrWeatherData)>4) { //if we actually want to populate the sensor column in device we need to get sensorId now, though now devices can have multiple sensors
				$sensorId = $arrWeatherData[4];
			}
			

		} else {
			$lines = [];
			$arrWeatherData = [0,0,0,0,0,0,0,0,0,0,0,0];
		}
		//var_dump($deviceIds);
		$canAccessData = array_search($locationId, $deviceIds) !== false;//old way: array_key_exists("storagePassword", $_REQUEST) && $storagePassword == $_REQUEST["storagePassword"];

		if($canAccessData) {
			$user = deriveUserFromStoragePassword($storagePassword);
			
			if($mode=="kill") {
				$method  = "kill";
			} else if ($mode=="getDevices") {
				$sql = "SELECT ip_address, name, device_id FROM device  WHERE device_id IN (" . implode("," , $deviceIds) . ") ORDER BY NAME ASC";
				//echo $sql;
  				$result = mysqli_query($conn, $sql);
  				if($result) {
	  				$rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
					$out["devices"] = $rows;
				}
			} else if ($mode=="getEnergyInfo"){ //this gets critical SolArk data for possible use automating certain things
				
				$energyInfo = getCurrentSolarData($user);
				$out["energy_info"] = [];
				if($energyInfo){
					if(gvfa("errors", $energyInfo)){
						$out["error"] = $energyInfo["errors"];
					} else {
						$out["energy_info"]["pv_power"] = $energyInfo["solar_power"];
						$out["energy_info"]["bat_power"] = $energyInfo["battery_power"];
						$out["energy_info"]["gen_power"] = $energyInfo["grid_power"];
						$out["energy_info"]["load_power"] = $energyInfo["load_power"];
						$out["energy_info"]["bat_percent"] = $energyInfo["battery_percentage"];
					}
				}
			} else if ($mode=="getDeviceData" || $mode == "getInitialDeviceInfo") {
				$deviceSql = "SELECT name, location_name FROM device WHERE device_id = " . intval($deviceId);
				$getDeviceResult = mysqli_query($conn, $deviceSql);
				if($getDeviceResult) {
					$deviceRow = mysqli_fetch_array($getDeviceResult);
					$deviceName = $deviceRow["name"];
				}
			} else if ($mode=="getOfficialWeatherData") {
				$sql = "SELECT latitude, longitude  FROM device  WHERE device_id =" . intval($deviceId);
				$getDeviceResult = mysqli_query($conn, $sql);
				if($getDeviceResult) {
					$deviceRow = mysqli_fetch_array($getDeviceResult);
					$latitude = $deviceRow["latitude"];
					$longitude = $deviceRow["longitude"];
					$apiKey = $user["open_weather_api_key"];
				}
				if($latitude  && $longitude && $apiKey) {
					$out["official_weather"] = getWeatherDataByCoordinates($latitude, $longitude, $apiKey);
				}
			} else if ($mode=="getInverterData") {
				if(array_key_exists("scale", $_REQUEST)) {
					$scale = $_REQUEST["scale"];
				} else {
					$scale = "";
				}
				if(!$conn) {
					$out = ["error"=>"bad database connection"];
				} else {
					
					if($scale == ""  || $scale == "fine") {
						$sql = "SELECT * FROM " . $database . ".inverter_log  
						WHERE user_id = " . $user["user_id"] . " AND  recorded > DATE_ADD(NOW(), INTERVAL -1 DAY) 
						ORDER BY inverter_log_id ASC";
					} else {
						if($scale == "hour") {
							$sql = "SELECT
							*,
							YEAR(recorded), DAYOFYEAR(recorded), HOUR(recorded) FROM " . $database . ".inverter_log  
							WHERE user_id = " . $user["user_id"] . " AND recorded > DATE_ADD(NOW(), INTERVAL -7 DAY) 
								GROUP BY YEAR(recorded), DAYOFYEAR(recorded), HOUR(recorded)
								ORDER BY inverter_log_id ASC";
						}
						if($scale == "day") {
							$sql = "SELECT 	 
							*,
							YEAR(recorded), DAYOFYEAR(recorded) FROM " . $database . ".inverter_log  
							 
								GROUP BY YEAR(recorded), DAYOFYEAR(recorded)
								ORDER BY inverter_log_id ASC";
						}
					}
 
					if($sql) {
						$result = mysqli_query($conn, $sql);
						$out = [];
						if($result && $canAccessData) {
							while($row = mysqli_fetch_all($result, MYSQLI_ASSOC)) {
								array_push($out, $row);
							}
						}
					}
				}
				$method  = "read";	

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
						WHERE recorded > DATE_ADD(NOW(), INTERVAL -1 DAY) AND location_id=" . $locationId . " 
						ORDER BY weather_data_id ASC";
					} else {
						if($scale == "hour") {
							$sql = "SELECT
							*,
							YEAR(recorded), DAYOFYEAR(recorded), HOUR(recorded) FROM " . $database . ".weather_data  
							WHERE recorded > DATE_ADD(NOW(), INTERVAL -7 DAY) AND location_id=" . $locationId . " 
								GROUP BY YEAR(recorded), DAYOFYEAR(recorded), HOUR(recorded)
								ORDER BY weather_data_id ASC";
						}
						if($scale == "day") {
							$sql = "SELECT 	 
							*,
							YEAR(recorded), DAYOFYEAR(recorded) FROM " . $database . ".weather_data  
							WHERE location_id=" . $locationId . " 
								GROUP BY YEAR(recorded), DAYOFYEAR(recorded)
								ORDER BY weather_data_id ASC";
						}
					}
					if($sql) {
						$result = mysqli_query($conn, $sql);
						$out = [];
						if($result && $canAccessData) {
							while($row = mysqli_fetch_array($result)) {
								array_push($out, $row);
							}
						}
					}
				}
				$method  = "read";	
			} else if ($mode == "saveData") { //save data
			//test url;:
			// //http://randomsprocket.com/weather/data.php?storagePassword=vvvvvvv&locationId=3&mode=saveData&data=10736712.76*12713103.20*1075869.28*NULL|0*0*1710464489*1710464504*1710464519*1710464534*1710464549*1710464563*1710464579*1710464593*
				
				//select * from weathertron.weather_data where location_id=3 order by recorded desc limit 0,10;
				$multipleSensorArray = explode("!", $weatherInfoString);
				foreach($multipleSensorArray  as $sensorDataString) { //if there is a ! in the weatherInfoString, 

					$arrWeatherData = explode("*", $sensorDataString);
					
					$temperature = $arrWeatherData[0];
					$pressure = $arrWeatherData[1];
					$humidity = $arrWeatherData[2];
					$gasMetric = "NULL";
					$deviceFeatureId = "NULL";
					if(count($arrWeatherData)>3) {
						$gasMetric = $arrWeatherData[3];
					}
					if(count($arrWeatherData)>4) {
						$sensorId = $arrWeatherData[4];
					}
					if(count($arrWeatherData)>5) {
						$deviceFeatureId = $arrWeatherData[5];
					}



					$weatherSql = "INSERT INTO weather_data(location_id, device_feature_id, recorded, temperature, pressure, humidity, gas_metric, wind_direction, precipitation, wind_speed, wind_increment, sensor_id) VALUES (" . 
					mysqli_real_escape_string($conn, $locationId) . "," .
					mysqli_real_escape_string($conn, $deviceFeatureId) . ",'" .  
					mysqli_real_escape_string($conn, $formatedDateTime)  . "'," . 
					mysqli_real_escape_string($conn, $temperature) . "," . 
					mysqli_real_escape_string($conn, $pressure) . "," . 
					mysqli_real_escape_string($conn, $humidity) . "," . 
					mysqli_real_escape_string($conn, $gasMetric) .
					",NULL,NULL,NULL,NULL," .
					mysqli_real_escape_string($conn, $sensorId) .
					")";
					
 

					if($temperature != "NULL" || $pressure != "NULL" || $humidity != "NULL" || $gasMetric != "NULL") { //if sensors are all null, do not attempt to store!
					
						$result = mysqli_query($conn, $weatherSql);
					
					}
				}
				$method  = "saveWeatherData";
				$out = Array("message" => "done", "method"=>$method);
			

			}
			if($mode == "getInitialDeviceInfo" ) { //return a double-delimited string of additional sensors, etc. this one begins with a "*" so we can identify it in the ESP8266. it will be the first data requested by the remote control
				$outString = "*" . str_replace("|", "", str_replace("*", "", $deviceName));
				$sensorSql = "SELECT  f.name, pin_number, power_pin, sensor_type, sensor_sub_type, via_i2c_address, device_feature_id 
					FROM device_feature f 
					LEFT JOIN device_type_feature t ON f.device_type_feature_id=t.device_type_feature_id 
					WHERE  sensor_type IS NOT NULL AND device_id=" . intval($deviceId) . " AND f.enabled ORDER BY sensor_type, via_i2c_address, pin_number;";
				$sensorResult = mysqli_query($conn, $sensorSql);
				//echo $sensorSql;
				if($sensorResult){
					$rows = mysqli_fetch_all($sensorResult, MYSQLI_ASSOC);
					foreach($rows as $row){
						//var_dump($row);
						$outString .= "|" . $row["pin_number"] . "*" . $row["power_pin"] . "*" . $row["sensor_type"] . "*" . $row["sensor_sub_type"] . "*" . $row["via_i2c_address"] . "*" . $row["device_feature_id"] . "*" . str_replace("|", "", str_replace("*", "", $row["name"]));
					}
				}
				die($outString);
			} else if($mode == "getDeviceData" || $mode == "saveData" ) {
				$out["device"] = $deviceName;
				$ipAddress = "192.168.1.X";
				$mustSaveLastKnownDeviceValueAsValue = 0;
				$method = "getDeviceData";
				$pinValuesKnownToDevice = [];
				$specificPin = -1;
				 
				if(count($lines)>1) {
					$recentReboots = explode("*", $lines[1]);
					foreach($recentReboots as $rebootOccasion) {
						if(intval($rebootOccasion) > 0 && $canAccessData) {
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
						//var_dump($pinValuesKnownToDevice);
						//echo "<P>" . $pinValuesKnownToDevice[3];
						//echo "<P>" .count($pinValuesKnownToDevice);
					}
					if(count($lines) > 3) {
						$extraInfo = explode("*", $lines[3]);
						if(count($extraInfo)>1){
							$lastCommandId = $extraInfo[0];
							$specificPin = $extraInfo[1]; //don't do this if $nonJsonPinData
					
						}
						//var_dump($extraInfo);
						if(count($extraInfo)>2){
							$mustSaveLastKnownDeviceValueAsValue = $extraInfo[2];
						}
						if(count($extraInfo)>3){
							$ipAddress = $extraInfo[3];
							if($ipAddress) {			
								if(strpos($ipAddress, " ") > 0){ //was getting crap from some esp8266s here
									$ipAddress = explode(" ", $ipAddress)[0];
								}
								$deviceSql = "UPDATE device SET ip_address='" . $ipAddress . "' ";
								if($sensorId){
									$deviceSql .= ", sensor_id=" . intval($sensorId);
								}
								
								$deviceSql .= " WHERE device_id=" . intval($deviceId);
								$deviceResult = mysqli_query($conn, $deviceSql);
								//echo $deviceSql;
							}
							
						} 

						if(count($extraInfo)>4) {
							$nonJsonPinData = $extraInfo[4];
						}
						if(count($extraInfo)>5) {
							$justGetDeviceInfo = $extraInfo[5];
							if($justGetDeviceInfo == '1'){
								$specificPin = -1; //this should always be -1 if justGetDeviceInfo is 1
							}
						}
					 
					}
				
				}

 
				$managementCache = []; //save us some database lookups
				//SELECT pin_number, f.name, value, enabled, can_be_analog, IFNULL(via_i2c_address, 0) AS i2c, device_feature_id FROM device_feature f LEFT JOIN device_type_feature t ON f.device_type_feature_id=t.device_type_feature_id WHERE device_id=11 ORDER BY i2c, pin_number;
				//the part where we include any data from our remote control system:
				$deviceSql = "SELECT allow_automatic_management, last_known_device_value, pin_number, f.name, value, enabled, can_be_analog, IFNULL(via_i2c_address, 0) AS i2c, device_feature_id 
					FROM device_feature f LEFT JOIN device_type_feature t ON f.device_type_feature_id=t.device_type_feature_id 
					WHERE pin_number IS NOT NULL AND sensor_type IS NULL AND device_id=" . intval($deviceId) . " ORDER BY i2c, pin_number;";
				//echo $deviceSql;
				$result = mysqli_query($conn, $deviceSql);
				if($result) {
					$rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
					$pinCursor = 0;
					//logSql($mustSaveLastKnownDeviceValueAsValue . "^" . $lines[2] . "^" . "------------------------------");
					foreach($rows as $row) {
						$allowAutomaticManagement = $row["allow_automatic_management"];
						$deviceFeatureId = $row["device_feature_id"];
						$pinNumber = $row["pin_number"];
						$sqlIfDataGoingUpstream = "";
						$managementRuleId = "NULL";
						$managementRuleName = "";
						$managementRuleIdIfNeeded = "";
						$mechanism = $ipAddress; //we will overwrite this if we end up making a change automatically
						$automatedChangeMade = false;
						//a good place to put automation code!  if an automated change happens, we will change $pinValuesKnownToDevice[$pinCursor] as if it happened via local remote, but then alter $mechanism and $managementRuleId
						//we also have to set $automatedChangeMade to true but TOTALLY SKIP the setting of $row['ss'], since from the local remote's perspective, the change happened server-side
						if($allowAutomaticManagement) {
							$automationSql = "SELECT m.* FROM management_rule m JOIN device_feature_management_rule d ON m.management_rule_id = d.management_rule_id AND m.user_id=d.user_id WHERE d.device_feature_id = " . $deviceFeatureId . " ORDER BY management_priority DESC";
							//logSql("management sql:" .  $automationSql);
							$automationResult = mysqli_query($conn, $automationSql);
							if($automationResult) {
								$automationRows = mysqli_fetch_all($automationResult, MYSQLI_ASSOC);
								$tagFindingPattern = '/<[a-zA-Z][^>]*?>/'; 
								foreach($automationRows as $automationRow) {
									$managementRuleIdIfNeeded = $automationRow["management_rule_id"];
									$timeValidStart = $automationRow["time_valid_start"];
									$timeValidEnd = $automationRow["time_valid_end"];
									$conditions = $automationRow["conditions"];
									$originalConditions = $conditions;
									$managementResultValue = $automationRow["result_value"];
									$managementRuleName = $automationRow["name"];

									$conditions = trim($conditions); // Remove any leading/trailing whitespace
									$conditions = mb_convert_encoding($conditions, 'UTF-8', 'auto'); 
					
									if($timeValidStart == NULL || $timeValidEnd == NULL || $currentTime >= $timeValidStart  && $currentTime <=  $timeValidEnd ) {
										//now all we need to do is worry about the conditions.  but these can be complicated!
										//the way a condition works is as follows:
										//you specify tokens of the form <logTableName[location_id_if_appropriate].columnName>, and these are replaced with values automatically
										//so <weather_data[1].temperatureValue> is replaced with the most recent temperatureValue for location_id=1
										//for inverter_log, there is no location_id (at least not yet!), so <inverter_log[].solar_power> provides the latest solar_power
										//if any log value is more than 20 minutes stale, automation does not happen (might want to log it though somehow!)
										preg_match_all($tagFindingPattern, $conditions, $matches);
										//now we have all our tags! we need to look up their respective data and substitute in!
										$tokenContents = $matches[0];
										
										
										foreach ($tokenContents as $originalToken) {
											$tokenReplaced = false;
											$lookedUpValue = NULL;
											//logSql("management token:" .  $tokenContent);
											$tokenContent = substr($originalToken, 1, -1); //chatgpt gave me too many chars with the regex
											//logSql("management token:" .  $tokenContent);
											//echo $tokenContent . "\n";
											if(array_key_exists($tokenContent, $managementCache)) {
												$lookedUpValue = $managementCache[$tokenContent];
												//echo "cache: " . $tokenContent . "=" .  $lookedUpValue . "\n";
											} else {
												$dotParts = explode(".", $tokenContent);
												$managmentColumn = "";
												$managementLocationId = "";
												if(count($dotParts) > 1){
													$managmentColumn = filterStringForSqlEntities($dotParts[1]);
												}
												$bracketParts = explode("[", $dotParts[0]);
												$managementTableName = filterStringForSqlEntities($bracketParts[0]);
												if(count($bracketParts) > 1){
													$managementLocationId =  str_replace("]", "", $bracketParts[1]);
												}
												$extraManagementWhereClause = "";
												if($managementLocationId != ""){
													$extraManagementWhereClause = " AND location_id=" . intval($managementLocationId);
												}
												$managmentValueLookupSql = "SELECT " . $managmentColumn . " As value FROM " . $managementTableName . " WHERE recorded >= '" . $formatedDateTime20MinutesAgo . "' AND " . $managementTableName . "_id = (SELECT MAX(" . $managementTableName. "_id) FROM " . $managementTableName . " WHERE 1=1 " . $extraManagementWhereClause . ") " . $extraManagementWhereClause;
												//echo $managmentValueLookupSql . "\n";
												
												//logSql("lookup sql:" . $managmentValueLookupSql);
												$managementValueResult = mysqli_query($conn, $managmentValueLookupSql);

												if($managementValueResult) {
													$valueArray = mysqli_fetch_array($managementValueResult);
													if($valueArray) {
														$lookedUpValue = gvfa("value", $valueArray);
													}
													//echo $originalToken . " :" .  $lookedUpValue . "\n";
												}
												
											}
											if($lookedUpValue != NULL) {
												$managementCache[$tokenContent] = $lookedUpValue;
												$tokenReplaced = true;
												//after all that, replace the token with the value in the condition!
												$conditions = str_replace($originalToken, $lookedUpValue, $conditions);

											} else {
												logSql("BAD lookup SQL: " . $managmentValueLookupSql);
											}
											if(!$tokenReplaced){
												$conditions = str_replace($originalToken, "<fail/>", $conditions);
												//echo  "!" . $lookedUpValue . "|=|" . $originalToken . "|:" . $conditions . "\n";
											}

											
										}
										if(count($tokenContents) > 0 ) {
											//logSql("management conditions:" .  $conditions);
											//echo "\n" . $conditions . "\n";
											$managementJudgment = false;
											if(strpos($conditions, "<fail/>") === false) {
												if(isValidPHP("echo " . $conditions . ";")) {
													$managementJudgment = eval('return ' . $conditions . ';');
												} else {
													logSql("BAD CONDITIONS:" .  $conditions);
												}
											} else {
												logSql("FAILED TOKEN REPLACEMENT:" .  $originalConditions);
											}

											//logSql("management judgment:" . $managementJudgment);
											if($managementJudgment == 1  && $row["value"] != $managementResultValue){
												$mechanism = "automation";
												$managementRuleId = $managementRuleIdIfNeeded;
												$pinValuesKnownToDevice[$pinCursor] =  $managementResultValue;
												$automatedChangeMade = true;
												//logSql($managementRuleName . "; setting #" . $deviceFeatureId . " to " . $pinValuesKnownToDevice[$pinCursor]);
											}
										}
									}
								}
							}
						}
						$lastModified  = "";
						$sqlToUpdateDeviceFeature = "";
						//echo count($pinValuesKnownToDevice) . "*" . $pinCursor . "<BR>";
						//this part update device_feature so we can tell from the server if the device has taken on the server's value
						if(count($pinValuesKnownToDevice) > $pinCursor && $pinValuesKnownToDevice[$pinCursor] != "") {
							$lastModified = " last_known_device_modified='" . $formatedDateTime . "',";
							$lastKnownDevice = " last_known_device_value =  " . $pinValuesKnownToDevice[$pinCursor] . ","; //only do this when we actually have data from the microcontroller
							$sqlToUpdateDeviceFeature = "UPDATE device_feature SET <lastknowndevice/><lastmodified/><additional/>";
							
							//echo $sqlToUpdateDeviceFeature  . "<BR> " . $specificPin  . "<BR>";
							$sqlIfDataGoingUpstream = " value =" . $pinValuesKnownToDevice[$pinCursor] . ",";
							if($automatedChangeMade) {
								$sqlToUpdateDeviceFeature = str_replace("<lastknowndevice/>", "", $sqlToUpdateDeviceFeature); 
							} else {
								$sqlToUpdateDeviceFeature = str_replace("<lastknowndevice/>", $lastKnownDevice, $sqlToUpdateDeviceFeature);
							}
						}
						if($mustSaveLastKnownDeviceValueAsValue || $automatedChangeMade){ //actually update the pin values here too!
							if(count($pinValuesKnownToDevice) > $pinCursor && $pinValuesKnownToDevice[$pinCursor] != "") {
								$sqlToUpdateDeviceFeature = str_replace("<additional/>", $sqlIfDataGoingUpstream, $sqlToUpdateDeviceFeature);
							}
							if(!$automatedChangeMade && ($pinCursor == count($rows)-1 || $specificPin > -1)) {
								$row["ss"] = 1; //only do this on the last pin! and don't do it for automatedChanges
							} else {
								$row["ss"] = 0;
							}
						} else {
							if(count($pinValuesKnownToDevice) > $pinCursor  && $pinValuesKnownToDevice[$pinCursor] != "") {
								$sqlToUpdateDeviceFeature = str_replace("<additional/>", "", $sqlToUpdateDeviceFeature);
							}
							$row["ss"] = 0;
							
						}
						if(count($pinValuesKnownToDevice) > $pinCursor) { //do all the logging and the lastModified part
							if($row["value"] != $pinValuesKnownToDevice[$pinCursor] || $row["last_known_device_value"] !=  $row["value"]) { //we're changing a value by sending one upstream. otherwise we shouldn't change last_known_device_modified
								$sqlToUpdateDeviceFeature = str_replace("<lastmodified/>", $lastModified, $sqlToUpdateDeviceFeature);
								
								$oldValue = $row["value"];
								$newValue = $pinValuesKnownToDevice[$pinCursor];
								
								if($row["last_known_device_value"] !=  $row["value"]) {
									$mechanism = "server-side";
									$oldValue = $row["last_known_device_value"];
									$newValue = $row["value"];
								} 
								//also log this change in the new device_feature_log table!  we're going to need that for when device_features get changed automatically based on data as well!
								$loggingSql = "INSERT INTO device_feature_log (device_feature_id, user_id, recorded, beginning_state, end_state, management_rule_id, mechanism) VALUES (";
								$loggingSql .= $row["device_feature_id"] . "," . $user["user_id"] . ",'" . $formatedDateTime . "'," .$oldValue . "," . $newValue  . "," . $managementRuleId  . ",'" . $mechanism . "')";
								//if($mechanism == "automation"){
									//logSql("logging sql: " . $loggingSql);
									//logSql("update sql: " . $sqlToUpdateDeviceFeature);
								//}
								//echo $loggingSql;
								logSql($loggingSql);
								logSql("specific pin: ".$specificPin . " pinCursor:" . $pinCursor  );
								logSql("querystring: ". $_SERVER['QUERY_STRING']  );
								if($automatedChangeMade || $specificPin > -1 && $specificPin == $pinCursor  || $specificPin == -1){ //otherwise we get too much logging if we're in one-pin-at-a-mode time
									$loggingResult = mysqli_query($conn, $loggingSql);
								}
							} else {
								$sqlToUpdateDeviceFeature = str_replace("<lastmodified/>", "", $sqlToUpdateDeviceFeature);
							}
						}
						if($specificPin == -1 || $specificPin ==  $pinCursor){
							unset($row["device_feature_id"]);//make things as lean as possible for IoT device
							unset($row["last_known_device_value"]);//make things as lean as possible for IoT device
							unset($row["allow_automatic_management"]);//make things as lean as possible for IoT device
							$out["device_data"][] = $row;
						}
						if(count($pinValuesKnownToDevice) > $pinCursor  && $pinValuesKnownToDevice[$pinCursor] != "") {
							if(endsWith($sqlToUpdateDeviceFeature, ",")) {
								$sqlToUpdateDeviceFeature = substr($sqlToUpdateDeviceFeature, 0, -1);
							}
							$sqlToUpdateDeviceFeature .= " WHERE device_feature_id=" . $deviceFeatureId;
							logSql("change sql:" . $sqlToUpdateDeviceFeature);
							if($sqlToUpdateDeviceFeature != "") {
								$updateResult = mysqli_query($conn, $sqlToUpdateDeviceFeature);
							}
						}
							
						if($row["i2c"] > 0){
							$out["pin_list"][] = $row["i2c"] . "." . $pinNumber ;
						} else {
							$out["pin_list"][] = $pinNumber ;
						}
						$pinCursor++;
					}
				}
			} 
		}else {
			$out = ["error"=>"you lack permissions"];
		}
	}
	//var_dump($extraInfo);
	//var_dump($nonJsonPinData);
	//var_dump($justGetDeviceInfo);
	if($nonJsonPinData == '1' && array_key_exists("device_data", $out)) { //create a very bare-bones non-JSON delimited data object to speed up data propagation to device
		$nonJsonOutString = "|";
		foreach($out["device_data"] as $deviceDatum){
			if($deviceDatum["enabled"]) {
				if($deviceDatum["i2c"] > 0){
					$pinName = $deviceDatum["i2c"] . "." . $deviceDatum["pin_number"];
				} else {
					$pinName = $deviceDatum["pin_number"];
				}
				$nonJsonOutString .=  str_replace("|", "", str_replace("*", "", $deviceDatum["name"])) . "*" . $pinName . "*" . $deviceDatum["value"] .  "*" . $deviceDatum["can_be_analog"] . "*" . $deviceDatum["ss"] . "|";
			}

		}
		$nonJsonOutString = substr($nonJsonOutString, 0, -1);
		die($nonJsonOutString);
	} else {
		if($justGetDeviceInfo == '1' && array_key_exists("device_data", $out)) { //used to greatly limit sent back JSON data to a just-started ESP8266
			unset($out["device_data"]);
		}
		echo json_encode($out);
	}
} else {
	echo '{"message":"done", "method":"' . $method . '"}';
}

function disable_gzip() { //i wanted this to work so the microcontroller wouldn't have to decompress data but it didn't work
	header('Content-Encoding: identity');
	@ini_set('zlib.output_compression', 'Off');
	@ini_set('output_buffering', 'Off');
	@ini_set('output_handler', '');
	@ini_set('DeflateBufferSize', 8192);
	@ini_set('no-gzip', 'On');
	@ini_set('fastcgi_pass_request_headers', 'Off');
	@apache_setenv('no-gzip', 1);	
}


//this only works if you make sure your users all have distinct storagePasswords!
function deriveDeviceIdsFromStoragePassword($storagePassword) {
	Global $conn;
	$deviceIds = [];
	$sql = "SELECT device_id FROM user u JOIN device d ON u.user_id=d.user_id WHERE storage_password='" . mysqli_real_escape_string($conn, $storagePassword)  . "' ORDER BY device_id ASC";
	$result = mysqli_query($conn, $sql);
	if($result) {
		$rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
		foreach($rows as $row){
			$deviceIds[] = $row["device_id"];
		}
		//var_dump($deviceIds);
		return $deviceIds;
	}
}

function deriveUserFromStoragePassword($storagePassword) {
	Global $conn;
	$sql = "SELECT * FROM user WHERE storage_password='" . mysqli_real_escape_string($conn, $storagePassword)  . "'";
	$result = mysqli_query($conn, $sql);
	if($result) {
		$row = mysqli_fetch_array($result, MYSQLI_ASSOC);
		return $row;
	}
}

function logSql($sql){
	//return; //for when you don't actually want to log
	global $formatedDateTime;
	$myfile = file_put_contents('sql.txt', "\n\n" . $formatedDateTime . ": " . $sql, FILE_APPEND | LOCK_EX);
}
 

//some helpful sql examples for creating sql users:
//CREATE USER 'weathertron'@'localhost' IDENTIFIED  BY 'your_password';
//GRANT CREATE, ALTER, DROP, INSERT, UPDATE, DELETE, SELECT, REFERENCES, RELOAD on *.* TO 'weathertron'@'localhost' WITH GRANT OPTION;
//GRANT ALL PRIVILEGES ON *.* TO 'weathertron'@'localhost' WITH GRANT OPTION;
 
