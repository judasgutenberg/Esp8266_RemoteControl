<?php 
//remote control and weather monitoring backend. 
//i've tried to keep all the code vanilla and old school
//of course in php it's all kind of bleh
//gus mueller, April 14 2024 - April 5 2025
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
$error = "";
$badSql = "";
$out = [];
$date = new DateTime("now", new DateTimeZone($timezone));//set the $timezone global in config.php
$pastDate = $date;
$aFewMinutesPastDate = $date;
$formatedDateTime =  $date->format('Y-m-d H:i:s');
$storageDateTime = $formatedDateTime;
$currentTime = $date->format('H:i:s');
$pastDate->modify('-20 minutes');
$formatedDateTime20MinutesAgo =  $pastDate->format('Y-m-d H:i:s');
$aFewMinutesPastDate->modify('-6 minutes');
$formatedDateTimeAFewMinutesAgo =  $aFewMinutesPastDate->format('Y-m-d H:i:s');
//$formatedDateTime =  $date->format('H:i');
$deviceId = "";
$locationId = "";
$deviceName = "Your device";
$deviceIds = [];
$sensorId = "NULL";
$nonJsonPinData = 0;
$justGetDeviceInfo = 0;
$storagePassword = "";
$multipleSensorArray = [];
$latestCommandData = null;

$user = autoLogin(); //if we are using this as a backend for the inverter or weather page, we don't need to pass the storagePassword at all.  this will only work once the user has logged in and selected a single tenant

if(array_key_exists("mode", $_REQUEST)) {
	$mode = $_REQUEST["mode"];
} else {
	$mode = "getWeatherData";
}
 
if($_POST) {
	logPost(gvfa("data", $_POST)); //help me debug
}
if($_REQUEST) {
	$periodAgo = 0;
	$scale = "day";
	//i may end up deciding that locationId and deviceId are the same thing, and for now they are
	//but maybe some day they will be different things
	if(array_key_exists("locationId", $_REQUEST)) {
		$locationId = $_REQUEST["locationId"];
	}
	if (array_key_exists("deviceId", $_REQUEST)) {
		$deviceId = $_REQUEST["deviceId"];
	}
	if (array_key_exists("device_id", $_REQUEST)) {
		$deviceId = $_REQUEST["device_id"];
	}
	if(!$locationId) {
		$locationId =  $deviceId;
	}
	if(!$deviceId) {
		$deviceId = $locationId;
	}
	if($locationId == ""){
		$locationId = 1; //need to revisit this for multiuser/multitenant
	}
	$locationIds = gvfw("location_ids");
	if(!$locationIds){
		$locationIds = $locationId;
	}

	$averageLatency = intval(readMemoryCache("latency-" . $deviceId));

	//absolute_timespan_cusps
	$absoluteTimespanCusps = false;
	$absoluteTimespanCusps = gvfw("absolute_timespan_cusps");
	$yearsAgo = 0;
	if(array_key_exists("years_ago", $_REQUEST)) {
		$yearsAgo = intval($_REQUEST["years_ago"]);
	} 
	if(array_key_exists("scale", $_REQUEST)) {
		$scale = $_REQUEST["scale"];
	} 
	if(array_key_exists("period_ago", $_REQUEST)) {
		$periodAgo = intval($_REQUEST["period_ago"]);
	}  

	$storagePassword  = gvfw("storage_password", gvfw("storagePassword"));
	$encryptedKey = gvfw("key");
	$encryptedKey2 = gvfw("k2"); //version 2
	$data = gvfa("data", $_REQUEST);
	$hashedData =  hash('sha256', $data);
	//$x = gvfw("x");
	if($encryptedKey){
		$checksum = calculateChecksum($data);
		$partiallyDecryptedKey = simpleDecrypt(urldecode($encryptedKey), strval(chr(countSetBitsInString($data))), strval(chr($checksum)));
		$storagePassword = simpleDecrypt($partiallyDecryptedKey, substr(strval(time() - ceil($averageLatency/1000) ), 1, 8) , $salt);
		
	} else if ($encryptedKey2) {
		$timeString = substr(strval(time()) , 1, 7);//third param is LENGTH
		//echo $timeString . "<=known to backend";
		//$timeString = "0123227";
		$storagePassword = simpleDecrypt2($encryptedKey2, $data,  $timeString, $encryptionScheme);
	}
	//die($storagePassword . " : " . time() . " ; " . $checksum . "?=" . $x  . " : " . urlencode($data));
	if($user && !$storagePassword) {
		$storagePassword  = $user['storage_password'];
		if(!in_array($mode, ["getOfficialWeatherData", "getInverterData", "getWeatherData", "getEarliestRecorded"])){ //keeps certain kinds of hacks from working
			die(json_encode(["error"=>"your brilliant hack has failed"]));
		}
	}
	
	$specificColumn = gvfw("specific_column");
	if($storagePassword){
		$deviceIds = deriveDeviceIdsFromStoragePassword($storagePassword);
	}

	$canAccessData = array_search($locationId, $deviceIds) !== false;//old way: array_key_exists("storagePassword", $_REQUEST) && $storagePassword == $_REQUEST["storagePassword"];
	if($canAccessData) {		
		$tenant = deriveTenantFromStoragePassword($storagePassword);
		$tenantId = $tenant["tenant_id"];
		if(!$conn) {
			$out = ["error"=>"bad database connection"];
		} else {
			$latestCommandData = getLatestCommandData($locationId, $tenant["tenant_id"]);
	 
			if(array_key_exists("data", $_REQUEST)) {
				
				$lines = explode("|",$data);
				//maybe move the parsing of all data up here so we have it if we need it
				///////
				
				$ipAddress = "192.168.1.X";
				$mustSaveLastKnownDeviceValueAsValue = 0;
				$method = "getDeviceData";
				$measuredVoltage = "NULL";
				$measuredAmpage = "NULL";
				$longitude = "NULL";
				$latitude = "NULL";
				$elevation = "NULL";
				$pinValuesKnownToDevice = [];
				$specificPin = -1;
				$saveDeviceInfo = false;
				$transmissionTimestamp = 0;
				$millis = "NULL";
				$averageLatency = "NULL";
				$latency = 0;

				if(count($lines)>1) {
					$recentReboots = explode("*", $lines[1]);
					foreach($recentReboots as $rebootOccasion) {
						if(intval($rebootOccasion) > 0 && $canAccessData) {
						$dt = new DateTime();
						$dt->setTimestamp($rebootOccasion);
						$rebootOccasionSql = $dt->format('Y-m-d H:i:s');
						$rebootLogSql = "INSERT INTO reboot_log(device_id, recorded) SELECT " . intval($deviceId) . ",'" .$rebootOccasionSql . "' 
							FROM DUAL WHERE NOT EXISTS (SELECT * FROM reboot_log WHERE device_id=" . intval($deviceId) . " AND recorded='" . $rebootOccasionSql . "' LIMIT 1)";
						
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
						//extraInfo: lastCommandId*pinCursor*localSource*ipAddressToUse*requestNonJsonPinInfo88*justDeviceJson*changeSourceId*transmissiontimestamp*millis*averageLatency
						if(count($extraInfo)>1){
							$lastCommandId = $extraInfo[0];
							markCommandDone($lastCommandId, $tenantId);
							$specificPin = $extraInfo[1]; //don't do this if $nonJsonPinData
						}
						//var_dump($extraInfo);
						if(count($extraInfo)>2){
							$mustSaveLastKnownDeviceValueAsValue = $extraInfo[2];
						}
						if(count($extraInfo)>3){
							$ipAddress = $extraInfo[3];
							$saveDeviceInfo = true;
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

						//changeSourceId, $extraInfo[6], not used here
						if(count($extraInfo)>7) {
							$transmissionTimestamp = $extraInfo[7];
							$latency = time() - intval($transmissionTimestamp);
						}
						if(count($extraInfo)>8) {
							$millis = $extraInfo[8];
						}
						if(count($extraInfo)>9) {
							$averageLatency = $extraInfo[9];
							writeMemoryCache("latency-" . $deviceId, $averageLatency);
						}

					 
					}
					if(count($lines) > 4) {
						$whereAndWhen = explode("*", $lines[4]); 
						//$whereAndWhen: numericTimestamp|measuredVoltage|measuredAmpage|latitude|longitude|elevation
						if(count($whereAndWhen)>0) {
							$numericTimestamp = defaultFailDown($whereAndWhen[0], "NULL"); //if a device has been offline, it might decide to timestamp the data it is sending from local storage
							if(is_numeric($numericTimestamp) && $numericTimestamp > 0){
								$dt = new DateTime();
								$dt->setTimestamp($numericTimestamp);
								$storageDateTime = $dt->format('Y-m-d H:i:s'); //set this to whatever was in the timestamp
							}
						}
						if(count($whereAndWhen)>1) {
							$measuredVoltage = defaultFailDown($whereAndWhen[1], "NULL");
						}
						if(count($whereAndWhen)>2) {
							$measuredAmpage = defaultFailDown($whereAndWhen[2], "NULL");
						}
						if(count($whereAndWhen)>3) {
							$latitude = defaultFailDown($whereAndWhen[3], "NULL");
						}
						if(count($whereAndWhen)>4) {
							$longitude = defaultFailDown($whereAndWhen[4], "NULL");
						}
						if(count($whereAndWhen)>5) {
							$elevation = defaultFailDown($whereAndWhen[5], "NULL");
						}
					}
				}
				////////

				$out["transmission_timestamp"] = $transmissionTimestamp;
				$out["backend_timestamp"] =  time();
				//logSql("timediff: for device# " . $deviceId . ": " . intval(intval($transmissionTimestamp)-intval(time())));

			if($mode=="saveIrData") { //data was captured from an irRecorder, so store it in the database!
				$irData = str_replace("*", ",", $lines[0]); //probably unnecessary now
				$irData = removeTrailingChar($irData, ","); //remove trailing commas from the sequence if it is there
				$irSql = "INSERT INTO 
				ir_pulse_sequence(ir_target_type_id, name, sequence, tenant_id, created) 
				VALUES (0, 'new', '" . $irData . "'," . $tenantId . ",'" . $storageDateTime . "')";
				$result = mysqli_query($conn, $irSql);
				//echo $irSql;
				$error = mysqli_error($conn);
				if($error != ""){
					$badSql = $irSql;
					logSql("bad infrared data save sql:" .  $irSql);
					$out["error"] = $error;
				}
			} else if ($mode=="debug") {
			} else if ($mode=="commandout") { //if we sent an instant_command, then the ESP8266 will redirect any output it generates to sendRemoteData, sending the output in the "data" parameter
				file_put_contents("instant_response_" . gvfw("device_id") . ".txt", $data, FILE_APPEND | LOCK_EX);
			} else if ($mode=="saveLocallyGatheredSolarData") { //used by the special inverter monitoring MCU to send fine-grain data promptly
					if($canAccessData) {
						///weather/data.php?storagePassword=xxxxxx&locationId=16&mode=saveLocallyGatheredSolarData&data=0*61*3336*3965*425*420*0*0*6359|||***192.168.1.200 
						$multipleSensorArray = explode("!", $lines[0]);
						//the first item will be energy data;  all subsequent items will be weather
						$energyInfoString = array_shift($multipleSensorArray);
						//var_dump($energyInfoString );
						$arrEnergyData = explode("*", $energyInfoString);
							$inverterSnapshotTime = $arrEnergyData[0];
							$gridPower = $arrEnergyData[1];
							$batteryPercent = $arrEnergyData[2];
							$batteryPower  = $arrEnergyData[3];
							$loadPower = $arrEnergyData[4];
							$solarString1 = $arrEnergyData[5];
							$solarString2 = $arrEnergyData[6];
							$batteryVoltage = intval($arrEnergyData[7])/100;
							$mysteryValue3 =  $arrEnergyData[8];
							$mysteryValue1 = $arrEnergyData[9];
							$mysteryValue2 = $arrEnergyData[10];
							$changer1 = $arrEnergyData[11];
							$changer2 = $arrEnergyData[12];
							$changer3 = $arrEnergyData[13];
							$changer4 = $arrEnergyData[14];
							$changer5 = $arrEnergyData[15];
							$changer6 = $arrEnergyData[16];
							$changer7 = $arrEnergyData[17];
						$energyInfo = saveSolarData($tenant, $gridPower, $batteryPercent,  
							$batteryPower, $loadPower, $solarString1, $solarString2, 
							$batteryVoltage, 
							$mysteryValue3,
							$mysteryValue1,
							$mysteryValue2,
							$changer1,
							$changer2,
							$changer3,
							$changer4,
							$changer5,
							$changer6,
							$changer7
						);
					}	
				} else {
					$weatherInfoString = $lines[0];
					
					$arrWeatherData = explode("*", $weatherInfoString);
					if(count($arrWeatherData)>4) { //if we actually want to populate the sensor column in device we need to get sensorId now, though now devices can have multiple sensors
						$sensorId = $arrWeatherData[4];
					}
					
				}
			} else {
				$lines = [];
				$arrWeatherData = [0,0,0,0,0,0,0,0,0,0,0,0];
			}

			
			if($mode=="kill") {
				$method  = "kill";
			} else if (beginsWith($mode, "getDevices")) {
				$deviceIdsPassedIn = gvfw("location_ids");
				$deviceIdsToUse = $deviceIds;
				if($deviceIdsPassedIn) {
					$deviceIdsToUse = explode("*", $deviceIdsPassedIn);
				}
				$sql = "SELECT ip_address, name, device_id FROM device  WHERE device_id IN (" . implode("," , $deviceIdsToUse) . ") ORDER BY NAME ASC";
				//echo $sql;
				$result = mysqli_query($conn, $sql);
				if($result) {
					$rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
					foreach($rows as &$row) {
						$deviceId = $row["device_id"];
						$additionalWhere = "";
						if($allDeviceColumnMaps) {
						$additionalWhere = " AND include_in_graph = 1 ";
						}
						$sql = "SELECT * FROM device_column_map WHERE device_id=" . $deviceId . " " . $additionalWhere . "  ORDER BY sort_order, display_name, column_name ";
						$subResult = mysqli_query($conn, $sql);
						if($subResult) {
							$subRows = mysqli_fetch_all($subResult, MYSQLI_ASSOC);
							$row["device_column_maps"] = $subRows;
						}
					}
					$out["devices"] = $rows;
				}
			} else if ($mode=="getEnergyInfo"){ //this gets critical SolArk data for possible use automating certain things
				$energyInfo = getCurrentSolarData($tenant);
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
			} else if ($mode=="getDeviceData" || $mode == "getInitialDeviceInfo" || $mode=="saveLocallyGatheredSolarData") {
				$deviceSql = "SELECT name, location_name FROM device WHERE device_id = " . intval($deviceId);
				$getDeviceResult = mysqli_query($conn, $deviceSql);
				if($getDeviceResult) {
					$deviceRow = mysqli_fetch_array($getDeviceResult);
					$deviceName = deDelimitify($deviceRow["name"]);
					$out["device"] = deDelimitify($deviceName);
				}
			} else if ($mode=="getOfficialWeatherData") {
				$sql = "SELECT latitude, longitude  FROM device  WHERE device_id =" . intval($deviceId);
				$getDeviceResult = mysqli_query($conn, $sql);
				if($getDeviceResult) {
					$deviceRow = mysqli_fetch_array($getDeviceResult);
					$latitudeForApi = $deviceRow["latitude"];
					$longitudeForApi = $deviceRow["longitude"];
					$apiKey = $tenant["open_weather_api_key"];
				}
				if($latitudeForApi  && $longitudeForApi && $apiKey) {
					$out["official_weather"] = getWeatherDataByCoordinates($latitudeForApi, $longitudeForApi, $apiKey);
				}
			} else if ($mode==="getEarliestRecorded") {
				$tableName = filterStringForSqlEntities(gvfw("table"));
				$sql = "SELECT MIN(recorded) AS recorded FROM " . $tableName . " WHERE 1=1 ";
				if($tenant && $tableName != 'device_log') {
					$sql .= " AND tenant_id=" . $tenantId;
				}
				if($locationId && $tableName != 'inverter_log') {
					$sql .= " AND device_id=" . $locationId;
				}
				$result = mysqli_query($conn, $sql);
				$error = mysqli_error($conn);
				if($result) {
				  $out = $result->fetch_assoc();
				}
				$out["sql"] = $sql;
				$out["error"] = $error;
			} else if ($mode=="getInverterData") {
				if(!$conn) {
					$out = ["error"=>"bad database connection"];
				} else {
					//i have a hardcoded config for all the different time scales in device_functions.php at or around
					//line 94 and it is used by both Javascript and PHP
					$scaleRecord = findRecordByKey(timeScales(), "text", $scale);
					$periodSize = $scaleRecord["period_size"];
					$periodScale = $scaleRecord["period_scale"];
					$initialOffset = gvfa("initial_offset", $scaleRecord, 0);
					$groupBy = gvfa("group_by", $scaleRecord, "");
					$startOfPeriod = "'" . $formatedDateTime . "'"; 
					$historyOffset = 1;
					$sql = "SELECT * FROM inverter_log  
						WHERE tenant_id = " . $tenantId . " ";
					if ($absoluteTimespanCusps == 1) {
						// Calculate starting point at the "cusp" of each period scale
						$startOfPeriod = sqlForStartOfPeriodScale($periodScale, $formatedDateTime);
						// Adjust SQL to break at cusps rather than present
						$sql .= " AND recorded > DATE_ADD(DATE_ADD(" . $startOfPeriod . ", INTERVAL -" . intval(($periodSize * ($periodAgo + 1) + $initialOffset)) . " " . $periodScale . " )" . ", INTERVAL -" . $yearsAgo . " YEAR)";
						if ($periodAgo > 0) {
							$sql .= " AND recorded < DATE_ADD(DATE_ADD(" . $startOfPeriod . ", INTERVAL -" . intval($periodSize * $periodAgo + $initialOffset) . " " . $periodScale . " )" . ", INTERVAL -" . $yearsAgo . " YEAR)";
						}
					} else {
						$sql .= " AND recorded > DATE_ADD(DATE_ADD('" . $formatedDateTime . "', INTERVAL -" . intval($periodSize * ($periodAgo + 1) + $initialOffset) . " " . $periodScale  . "), INTERVAL -" . $yearsAgo . " YEAR)";
					}
					//AND  recorded > DATE_ADD(" . $startOfPeriod . ", INTERVAL -" . intval(($periodSize * ($periodAgo + $historyOffset) + $initialOffset)) . " " . $periodScale . ") ";
					if($periodAgo  > 0) {
						$sql .= " AND recorded < DATE_ADD(" . $startOfPeriod . ", INTERVAL -" . intval(($periodSize * ($periodAgo) + $initialOffset)) . " " . $periodScale . ") ";
					}
					if($groupBy){
						$sql .= " GROUP BY " . $groupBy . " ";
					}
					$sql .= " ORDER BY inverter_log_id ASC";
					//die($sql);
					if($sql) {
						$result = mysqli_query($conn, $sql);
						$error = mysqli_error($conn);
						if($result && $canAccessData) {
							$out = mysqli_fetch_all($result, MYSQLI_ASSOC);
						}
						if(count($out) <1){
							array_push($out, ["sql" => $sql, "error"=>$error]);
						}
					}
					//die($periodAgo . " " . $sql);
				}
				$method  = "read";	

			} else if ($mode=="getWeatherData") {
				if(!$conn) {
					$out = ["error"=>"bad database connection"];
				} else {
					//i have a hardcoded config for all the different time scales in device_functions.php at or around
					//line 94 and it is used by both Javascript and PHP
					$scaleRecord = findRecordByKey(timeScales(), "text", $scale);
					$periodSize = $scaleRecord["period_size"];
					$periodScale = $scaleRecord["period_scale"];
					$initialOffset = gvfa("initial_offset", $scaleRecord, 0);
					$groupBy = gvfa("group_by", $scaleRecord, "");
					if($specificColumn) {
						//to revisit:  need to figure out a way to keep users without a device_id from seeing someone else's devices
						$sql = "SELECT " . filterStringForSqlEntities($specificColumn)  . ", device_id, DATE_ADD(recorded, INTERVAL " . $yearsAgo .  " YEAR) AS recorded FROM device_log WHERE  device_id IN (" . filterCommasAndDigits($locationIds) . ") ";
					} else {
						$weatherColumns = getGraphColumns($user["tenant_id"], $locationId);
						$sql = "SELECT " . implode(",", $weatherColumns) . ", device_id, DATE_ADD(recorded, INTERVAL " . $yearsAgo .  " YEAR) AS recorded FROM device_log WHERE device_id=" . $locationId;
					}
					if ($absoluteTimespanCusps == 1) {
						// Calculate starting point at the "cusp" of each period scale
						$startOfPeriod = sqlForStartOfPeriodScale($periodScale, $formatedDateTime);
						// Adjust SQL to break at cusps rather than present
						$sql .= " AND recorded > DATE_ADD(DATE_ADD(" . $startOfPeriod . ", INTERVAL -" . intval(($periodSize * ($periodAgo + 1) + $initialOffset)) . " " . $periodScale . " )" . ", INTERVAL -" . $yearsAgo . " YEAR)";
						if ($periodAgo > 0) {
							$sql .= " AND recorded < DATE_ADD(DATE_ADD(" . $startOfPeriod . ", INTERVAL -" . intval($periodSize * $periodAgo + $initialOffset) . " " . $periodScale . " )" . ", INTERVAL -" . $yearsAgo . " YEAR)";
						}
					} else {

						$sql .= " AND recorded > DATE_ADD(DATE_ADD('" . $formatedDateTime . "', INTERVAL -" . intval($periodSize * ($periodAgo + 1) + $initialOffset) . " " . $periodScale  . "), INTERVAL -" . $yearsAgo . " YEAR)";
					}
					if($periodAgo  > 0) {
						$sql .= " AND recorded < DATE_ADD(DATE_ADD('" . $formatedDateTime . "', INTERVAL -" . intval($periodSize * $periodAgo + $initialOffset) . " " . $periodScale  . "), INTERVAL -" . $yearsAgo . " YEAR)";
					} else if ($yearsAgo > 0){
						$sql .= " AND recorded < DATE_ADD(DATE_ADD('" . $formatedDateTime . "', INTERVAL -" . intval($periodSize * $periodAgo + $initialOffset) . " " . $periodScale  . "), INTERVAL -" . $yearsAgo . " YEAR)";
					}
					if($groupBy){
						$sql .= " GROUP BY " . $groupBy . " ";
					}
					$sql .= " ORDER BY device_log_id ASC";
					//die($sql);
					if($sql) {
						$result = mysqli_query($conn, $sql);
						$error = mysqli_error($conn);
						if($result && $canAccessData) {
							$out["records"] = mysqli_fetch_all($result, MYSQLI_ASSOC);
							$out["sql"] = $sql;
						}
						 //we need info about the devices if we are plotting data from multiple ones
						$out["devices"] = getDevices($tenant["tenant_id"], true);
						if(count($out) < 1){
							array_push($out, ["sql" => $sql, "error"=>$error]);
						}
					}
				}
				$method  = "read";	
			} 
			
			if ($mode == "saveData" || count($multipleSensorArray) > 0) { //save data
				//echo "saveDate or multipleSensorArray";
				//either we have a conventional saveData or we saved energy data and there is also weather data to save
				//test url;:
				//http://randomsprocket.com/weather/data.php?storagePassword=vvvvvvv&locationId=3&mode=saveData&data=10736712.76*12713103.20*1075869.28*NULL|0*0*1710464489*1710464504*1710464519*1710464534*1710464549*1710464563*1710464579*1710464593*
				
				//select * from weathertron.device_log where device_id=3 order by recorded desc limit 0,10;
				if(count($multipleSensorArray) == 0) {
					$multipleSensorArray = explode("!", $weatherInfoString);
				}
				$temperature = "NULL";
				$pressure = "NULL";
				$humidity = "NULL";
				$gasMetric = "NULL";
				$deviceFeatureId = "NULL";
				$windDirection = "NULL";
				$precipitation = "NULL";
				$windSpeed = "NULL";
				$windIncrement = "NULL";
				$sensorId = "NULL";
				$reserved1 = "NULL"; //we have four places for esoteric sensor data.  what they represent can be set in device.reservedX_name
				$reserved2 = "NULL";
				$reserved3 = "NULL";
				$reserved4 = "NULL";
				//$twelveVoltBatteryVoltage = NULL;
				$consolidateAllSensorsToOneRecord = 0; //if this is set to one by the first weather record, all weather data is stored in a single device_log record
				$weatherRecordCounter = 0;
				$doNotSaveBecauseNoData = true;
				foreach($multipleSensorArray  as $sensorDataString) { //if there is a ! in the weatherInfoString, 
					$arrWeatherData = explode("*", $sensorDataString);
					if(count($arrWeatherData) > 1) { //it's possible the $weatherInfoString began with a !, meaning the first weather record is technically empty
						$temperature = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $temperature, $arrWeatherData, "temperature", $deviceId, $tenantId);
						$pressure = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $pressure, $arrWeatherData, "pressure", $deviceId, $tenantId);
						$humidity = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $humidity, $arrWeatherData, "humidity", $deviceId, $tenantId);
						$gasMetric = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $gasMetric, $arrWeatherData, "gas_metric", $deviceId, $tenantId);
						$windDirection = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $windDirection, $arrWeatherData, "wind_direction", $deviceId, $tenantId);
						$windSpeed = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $windSpeed, $arrWeatherData, "wind_speed", $deviceId, $tenantId);
						$windIncrement = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $windIncrement, $arrWeatherData, "wind_increment", $deviceId, $tenantId);
						$precipitation = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $precipitation, $arrWeatherData, "precipitation", $deviceId, $tenantId);
						$reserved1 = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $reserved1, $arrWeatherData, "reserved1", $deviceId, $tenantId);
						$reserved2 = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $reserved2, $arrWeatherData, "reserved2", $deviceId, $tenantId);
						$reserved3 = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $reserved3, $arrWeatherData, "reserved3", $deviceId, $tenantId);
						$reserved4 = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $reserved4, $arrWeatherData, "reserved4", $deviceId, $tenantId);
						$sensorId = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $sensorId, $arrWeatherData, "sensor_id");
						//$twelveVoltBatteryVoltage = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $twelveVoltBatteryVoltage, $arrWeatherData, 16);
						if($consolidateAllSensorsToOneRecord){
							$deviceFeatureId = "NULL";
						} else {
							if(count($arrWeatherData)>13) {
								$deviceFeatureId = $arrWeatherData[13];
							} else {
								$deviceFeatureId = "NULL";
							}
						}
						if($deviceFeatureId == ""){
							$deviceFeatureId = "NULL";
						}
						//sensorName is $arrWeatherData[14] -- not used here
						//i put $consolidateAllSensorsToOneRecord on the sensor record so that some sensors could be separate and then later ones could be consolidated
						$consolidateAllSensorsToOneRecord = mergeWeatherDatum($consolidateAllSensorsToOneRecord, $consolidateAllSensorsToOneRecord, $arrWeatherData, "consolidate");

						//die("x" . $consolidateAllSensorsToOneRecord);
						$weatherSql = "INSERT INTO 
						device_log(device_id, device_feature_id, recorded, 
							temperature, pressure, humidity, 
							gas_metric, 
							wind_direction,  wind_speed, wind_increment, 
							precipitation, 
							reserved1, reserved2, reserved3, reserved4,
							sensor_id, voltage, ampage, latitude, longitude, elevation, millis, data_hash) 
						VALUES (" . 
						mysqli_real_escape_string($conn, $locationId) . "," .
						mysqli_real_escape_string($conn, $deviceFeatureId) . ",'" .  
						mysqli_real_escape_string($conn, $storageDateTime)  . "'," . 
						mysqli_real_escape_string($conn, $temperature) . "," . 
						mysqli_real_escape_string($conn, $pressure) . "," . 
						mysqli_real_escape_string($conn, $humidity) . "," . 
						mysqli_real_escape_string($conn, $gasMetric) . "," .  
						mysqli_real_escape_string($conn, $windDirection) . "," .  
						mysqli_real_escape_string($conn, $windSpeed) . "," .  
						mysqli_real_escape_string($conn, $windIncrement) . "," .
						mysqli_real_escape_string($conn, $precipitation) . "," .  
						mysqli_real_escape_string($conn, $reserved1) . "," .  
						mysqli_real_escape_string($conn, $reserved2) . "," .  
						mysqli_real_escape_string($conn, $reserved3) . "," .  
						mysqli_real_escape_string($conn, $reserved4) . "," .  
						mysqli_real_escape_string($conn, $sensorId) . "," .
						mysqli_real_escape_string($conn, $measuredVoltage)  . "," .
						mysqli_real_escape_string($conn, floatval($measuredAmpage))  . "," .
						mysqli_real_escape_string($conn, $latitude)  . "," .
						mysqli_real_escape_string($conn, $longitude) . "," .
						mysqli_real_escape_string($conn, $elevation) . "," .
						mysqli_real_escape_string($conn, $millis)  . ",'" .
						mysqli_real_escape_string($conn, $hashedData) .
						"')";
					}
					for($datumCounter = 0; $datumCounter < 12; $datumCounter++){
						if(count($arrWeatherData)> $datumCounter){
							$testValue = $arrWeatherData[$datumCounter];
							if(strtolower($testValue) != "null" && $testValue != "" && strtolower($testValue) != "nan"){
								$doNotSaveBecauseNoData = false;
								//echo $datumCounter . ": " . $testValue . ", should now be false<BR>";
							} else {
								//echo $datumCounter . ": " . $testValue . "<BR>";
							}
						}
					}
					//echo $doNotSaveBecauseNoData . "<BR>";
					if(!$doNotSaveBecauseNoData) { //if sensors are all null, do not attempt to store!
						//echo $weatherSql;
						if(intval($consolidateAllSensorsToOneRecord) != 1 || $weatherRecordCounter == count($multipleSensorArray) - 1) {
							$result = mysqli_query($conn, $weatherSql);
							$error = mysqli_error($conn);
							if($error != ""){
								$badSql = $weatherSql;
								logSql("bad weather data save sql:" .  $weatherSql);
							}
						}
					} else {
						logSql("no save sql:" .  $weatherSql);
					}
					$weatherRecordCounter++;
				}
				$method  = "saveWeatherData";
				$out =  addNodeIfPresent(addNodeIfPresent(Array("message" => "done", "method"=>$method), "error", $error), "sql", $badSql);
			}
	 
			if($mode == "getInitialDeviceInfo" ) { //return a double-delimited string of additional sensors, etc. this one begins with a "*" so we can identify it in the ESP8266. it will be the first data requested by the remote control
				$outString = "*" . deDelimitify($deviceName); 
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
						$outString .= "|" . $row["pin_number"] . "*" . $row["power_pin"] . "*" . $row["sensor_type"] . "*" . $row["sensor_sub_type"] . "*" . $row["via_i2c_address"] . "*" . $row["device_feature_id"] . "*" .  removeDelimiters($row["name"]) . "*" . $row["overwrite_ordinal"] . "*0";
					}
				}
				if(strpos($outString, "|") === false){
					$outString .= "|";
				}
				die($outString);
			} else if($mode == "getDeviceData" || $mode == "saveData" || $mode=="saveLocallyGatheredSolarData") {
				if($saveDeviceInfo) {			
					if(strpos($ipAddress, " ") > 0){ //was getting crap from some esp8266s here
						$ipAddress = explode(" ", $ipAddress)[0];
					}
					$deviceSql = "UPDATE device SET ip_address='" . $ipAddress . "', last_poll='" . $formatedDateTime  . "' ";
					if($measuredVoltage){
						$deviceSql .= ", voltage=" . $measuredVoltage;
					}
					if($sensorId){
						$deviceSql .= ", sensor_id=" . intval($sensorId);
					}
					$deviceSql .= " WHERE device_id=" . intval($deviceId);
					$deviceResult = mysqli_query($conn, $deviceSql);
					//echo $deviceSql;
				}
				if($latestCommandData) {
					//old way to do things:
					//$out = "!" . $latestCommandData["command_id"] . "|" . $latestCommandData["command"] . "|" . $latestCommandData["value"];
					//die($out);
				} 
				//var_dump($pinValuesKnownToDevice);
				$managementCache = []; //save us some database lookups
				//SELECT pin_number, f.name, value, enabled, can_be_analog, IFNULL(via_i2c_address, 0) AS i2c, device_feature_id FROM device_feature f LEFT JOIN device_type_feature t ON f.device_type_feature_id=t.device_type_feature_id WHERE device_id=11 ORDER BY i2c, pin_number;
				//the part where we include any data from our remote control system:
				$deviceSql = "SELECT 
						allow_automatic_management, 
						last_known_device_value, 
						pin_number, f.name, 
						value, 
						enabled, 
						can_be_analog, 
						IFNULL(via_i2c_address, 0) AS i2c, 
						device_feature_id, 
						automation_disabled_when, 
						restore_automation_after, 
						f.user_id, 
						f.modified,
						(SELECT beginning_state FROM device_feature_log  dfl WHERE dfl.device_feature_id=f.device_feature_id ORDER BY device_feature_log_id DESC LIMIT 0,1) as historic_was,
						(SELECT end_state FROM device_feature_log  dfl WHERE dfl.device_feature_id=f.device_feature_id ORDER BY device_feature_log_id DESC LIMIT 0,1) as historic_became,
						(SELECT mechanism FROM device_feature_log  dfl WHERE dfl.device_feature_id=f.device_feature_id ORDER BY device_feature_log_id DESC LIMIT 0,1) as historic_mechanism,
						(SELECT recorded FROM device_feature_log  dfl WHERE dfl.device_feature_id=f.device_feature_id ORDER BY device_feature_log_id DESC LIMIT 0,1) as historic_recorded
					FROM device_feature f 
					LEFT JOIN device_type_feature t ON f.device_type_feature_id=t.device_type_feature_id 
					WHERE pin_number IS NOT NULL AND sensor_type IS NULL AND enabled=1 AND device_id=" . intval($deviceId) . " ORDER BY i2c, pin_number;";
				//echo $deviceSql;
				$result = mysqli_query($conn, $deviceSql);
				if($result) {
					$rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
					$pinCursor = 0;
					//logSql($mustSaveLastKnownDeviceValueAsValue . "^" . $lines[2] . "^" . "------------------------------");
					foreach($rows as $row) {
						$canUpdateDeviceFeature = true;
						$historicWas = $row["historic_was"];
						$historicBecame = $row["historic_became"];
						$historicMechanism = $row["historic_mechanism"];
						$historicRecorded = $row["historic_recorded"];
						$allowAutomaticManagement = $row["allow_automatic_management"];
						$automationDisabledWhen = $row["automation_disabled_when"];
						$restoreAutomationAfter = $row["restore_automation_after"];
						$deviceFeatureId = $row["device_feature_id"];
						$pinNumber = $row["pin_number"];
						$userId = $row["user_id"];
						$modified = $row["modified"];
						if(!$userId){
							$userId = "NULL";
						}
						$sqlIfDataGoingUpstream = "";
						$managementRuleId = "NULL";
						$managementRuleName = "";
						$managementRuleIdIfNeeded = "";
						$undoDisabledAutomation = "";
						$mechanism = $ipAddress; //we will overwrite this if we end up making a change automatically
						$automatedChangeMade = false;
						//a good place to put automation code!  if an automated change happens, we will change $pinValuesKnownToDevice[$pinCursor] as if it happened via local remote, but then alter $mechanism and $managementRuleId
						//we also have to set $automatedChangeMade to true but TOTALLY SKIP the setting of $row['ss'], since from the local remote's perspective, the change happened server-side
						$inTheFutureWhenWeRestoreManagement = new DateTime($automationDisabledWhen);
						if(intval($restoreAutomationAfter) > 0){
							$inTheFutureWhenWeRestoreManagement->modify($restoreAutomationAfter . ' hours');
						}
						$formattedInTheFuture = $inTheFutureWhenWeRestoreManagement->format('Y-m-d H:i:s');
						if(gvfa("debug", $_REQUEST) == 'suspension'){
							echo "<BR>". $row["name"] ."; feature id: " . $deviceFeatureId;
							echo "<BR>formatted in future: " . $formattedInTheFuture;
							echo "<BR>now time: " . $formatedDateTime;
							echo "<BR>automationDisabledWhen: " . $automationDisabledWhen  ;
							echo "<BR>allowAutomaticManagement: " . $allowAutomaticManagement . "<BR>";
							echo "<hr>";
						}
						if($allowAutomaticManagement  && ($automationDisabledWhen == null || $formattedInTheFuture < $formatedDateTime)) {
							$automationSql = "SELECT m.* FROM management_rule m JOIN device_feature_management_rule d ON m.management_rule_id = d.management_rule_id AND m.tenant_id=d.tenant_id WHERE d.device_feature_id = " . $deviceFeatureId . " ORDER BY management_priority DESC";
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
										//you specify tokens of the form <logTableName[device_id_if_appropriate].columnName>, and these are replaced with values automatically
										//so <device_log[1].temperatureValue> is replaced with the most recent temperatureValue for device_id=1
										//for inverter_log, there is no device_id (at least not yet!), so <inverter_log[].solar_power> provides the latest solar_power
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
												$managementColumn = "";
												$managementLocationId = "";
												if(count($dotParts) > 1){
													$managementColumn = filterStringForSqlEntities($dotParts[1]);
												}
												$bracketParts = explode("[", $dotParts[0]);
												$managementTableName = filterStringForSqlEntities($bracketParts[0]);
												if(count($bracketParts) > 1){
													$managementLocationId =  str_replace("]", "", $bracketParts[1]);
												}
												$extraManagementWhereClause = "";
												if($managementLocationId != ""){
													$extraManagementWhereClause = " AND device_id=" . intval($managementLocationId);
												}
												$managmentValueLookupSql = "SELECT " . $managementColumn . " As value FROM " . $managementTableName . " WHERE recorded >= '" . $formatedDateTime20MinutesAgo . "' AND " . $managementTableName . "_id = (SELECT MAX(" . $managementTableName. "_id) FROM " . $managementTableName . " WHERE 1=1 " . $extraManagementWhereClause . ") " . $extraManagementWhereClause;
												$managementValueResult = mysqli_query($conn, $managmentValueLookupSql);

												if($managementValueResult) {
													$valueArray = mysqli_fetch_array($managementValueResult);
													if($valueArray) {
														$lookedUpValue = gvfa("value", $valueArray);
													}
												}
											}
											if($lookedUpValue != NULL) {
												$managementCache[$tokenContent] = $lookedUpValue;
												$tokenReplaced = true;
												//after all that, replace the token with the value in the condition!
												$conditions = str_replace($originalToken, $lookedUpValue, $conditions);

											} else {
												//logSql("BAD lookup SQL: " . $managmentValueLookupSql);
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
											if($managementJudgment == 1  && $row["value"] != $managementResultValue){
												//don't automate a change within five minutes of a user change
												if(timeDifferenceInMinutes($modified, $formatedDateTime) > 5) {
													$mechanism = "automation";
													$managementRuleId = $managementRuleIdIfNeeded;
													//echo intval($managementResultValue) . "<BR>";
													//we need to make this value explicitly numeric or we have trouble later:
													$pinValuesKnownToDevice[$pinCursor] = intval($managementResultValue);
													$automatedChangeMade = true;
												}
												//logSql($managementRuleName . "; setting #" . $deviceFeatureId . " to " . $pinValuesKnownToDevice[$pinCursor]);
											}
										}
									}
								}
							}
						}
						if($formattedInTheFuture < $formatedDateTime && $automationDisabledWhen) {
							$undoDisabledAutomation = " automation_disabled_when=NULL,";
						}
						$lastModified  = "";
						$sqlToUpdateDeviceFeature = "";
						//echo $deviceFeatureId . "*" . $pinCursor . "<BR>";
						//if we have a pinValuesKnownToDevice change AND there is allowAutomaticManagement then we need to take the $formatedDateTime, and use that to set automation_disabled_when 
						if(count($pinValuesKnownToDevice) > $pinCursor && is_numeric($pinValuesKnownToDevice[$pinCursor])) {
							//echo $deviceFeatureId   . "=" . $pinCursor . "<BR>";
							$lastModified = " last_known_device_modified='" . $formatedDateTime . "',";
							$lastKnownDevice = " last_known_device_value =  " . nullifyOrNumber($pinValuesKnownToDevice[$pinCursor]) . ","; //only do this when we actually have data from the microcontroller
							$sqlToUpdateDeviceFeature = "UPDATE device_feature SET <lastknowndevice/><lastmodified/><additional/>";
							$sqlIfDataGoingUpstream = " value =" . $pinValuesKnownToDevice[$pinCursor] . ",";
							if($deviceFeatureId == 3){
								//logSql("going upstream sql:" . $sqlIfDataGoingUpstream . " " . $lines[2]);
							}
							//suspect:  i had been doing this:
							//if($automatedChangeMade) {
								//$sqlToUpdateDeviceFeature = str_replace("<lastknowndevice/>", "", $sqlToUpdateDeviceFeature); 
							//} else {
								$sqlToUpdateDeviceFeature = str_replace("<lastknowndevice/>", $lastKnownDevice, $sqlToUpdateDeviceFeature);
							//}
						}
						//echo $deviceFeatureId  . "*" . $mustSaveLastKnownDeviceValueAsValue  . "*" . $automatedChangeMade . "<BR>";
						if($mustSaveLastKnownDeviceValueAsValue || $automatedChangeMade){ //actually update the pin values here too!
							//echo $sqlToUpdateDeviceFeature . "<BR>";
							if(count($pinValuesKnownToDevice) > $pinCursor && is_numeric($pinValuesKnownToDevice[$pinCursor])) {
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
								//mechanism is the ipAddress or "automation" at this point
								if($row["last_known_device_value"] !=  $row["value"]) {
									$mechanism = "server-side";
									$oldValue = $row["last_known_device_value"];
									$newValue = $row["value"];
								} 
								if(!$automationDisabledWhen && $allowAutomaticManagement && !$automatedChangeMade && intval($oldValue) != intval($newValue)) {  
									$sqlToUpdateDeviceFeature .= " automation_disabled_when='" . $formatedDateTime . "',";
								}
								//if this is an ipaddress-mechanism change undoing a recent automation change, then don't bother
								if($historicMechanism == "automation" && intval($historicBecame) != intval($newValue)  &&  $mechanism == $ipAddress && $historicRecorded > $formatedDateTimeAFewMinutesAgo){
									$canUpdateDeviceFeature = false;
								}
								//also log this change in the new device_feature_log table!  we're going to need that for when device_features get changed automatically based on data as well!
								$weJustHadALogItemLikeThis = intval($historicWas) == intval($oldValue) && intval($historicBecame) == intval($newValue) && $historicMechanism == $mechanism && $historicRecorded > $formatedDateTimeAFewMinutesAgo;

								if(!$weJustHadALogItemLikeThis && $canUpdateDeviceFeature) {
									$loggingSql = "INSERT INTO device_feature_log (device_feature_id, tenant_id, recorded, beginning_state, end_state, management_rule_id, mechanism, user_id) VALUES (";
									$loggingSql .= nullifyOrNumber($row["device_feature_id"]) . "," . $tenant["tenant_id"] . ",'" . $formatedDateTime . "'," . intval($oldValue) . "," . intval($newValue)  . "," . nullifyOrNumber($managementRuleId)  . ",'" . $mechanism . "'," . $userId .")";
									if($automatedChangeMade || $specificPin > -1 && $specificPin == $pinCursor  || $specificPin == -1){ //otherwise we get too much logging if we're in one-pin-at-a-mode time
										if(intval($oldValue) != intval($newValue) ) { //let's only log ch-ch-ch-changes
											$loggingResult = mysqli_query($conn, $loggingSql);
										}
										$error = mysqli_error($conn);
										if($error != ""){
											$badSql = $loggingSql;
											$out["error"] = $error;
											$out["sql"] = $badSql;
										}
									}
								}
							} else {
								$sqlToUpdateDeviceFeature = str_replace("<lastmodified/>", "", $sqlToUpdateDeviceFeature);
							}
						}
						if($specificPin == -1 || $specificPin ==  $pinCursor){
							unset($row["device_feature_id"]);//make things as lean as possible for IoT device
							unset($row["last_known_device_value"]);//make things as lean as possible for IoT device
							unset($row["allow_automatic_management"]);//make things as lean as possible for IoT device
							unset($row["automation_disabled_when"]);//make things as lean as possible for IoT device
							unset($row["restore_automation_after"]);//make things as lean as possible for IoT device
							unset($row["user_id"]);//make things as lean as possible for IoT device
							unset($row["modified"]);//make things as lean as possible for IoT device
							unset($row["historic_was"]);//make things as lean as possible for IoT device
							unset($row["historic_became"]);//make things as lean as possible for IoT device
							unset($row["historic_mechanism"]);//make things as lean as possible for IoT device
							unset($row["historic_recorded"]);//make things as lean as possible for IoT device
							$out["device_data"][] = $row;
						}
						//echo $sqlToUpdateDeviceFeature . "<BR>";
						//echo $deviceFeatureId . "*" . intval(count($pinValuesKnownToDevice) > $pinCursor) . "*" . is_numeric($pinValuesKnownToDevice[$pinCursor]) . "<BR>";
						if(count($pinValuesKnownToDevice) > $pinCursor  && is_numeric($pinValuesKnownToDevice[$pinCursor])) {
							if($undoDisabledAutomation != ""){
								$sqlToUpdateDeviceFeature .= $undoDisabledAutomation;
							}
							$sqlToUpdateDeviceFeature = trim($sqlToUpdateDeviceFeature);
							//echo "{" . $sqlToUpdateDeviceFeature . "}" . substr($sqlToUpdateDeviceFeature,-1) . "<BR>";
							$sqlToUpdateDeviceFeature = removeTrailingChar($sqlToUpdateDeviceFeature, ",", 1);
							$sqlToUpdateDeviceFeature .= " WHERE device_feature_id=" . $deviceFeatureId;
							if($deviceFeatureId == 3){
								//logSql("change sql:" . $sqlToUpdateDeviceFeature);
							}
							if(isset($loggingSql)){
								//logSql("logging sql:" . $loggingSql);
							}
							//echo $sqlToUpdateDeviceFeature . "<BR>";
							if($sqlToUpdateDeviceFeature != ""  && $canUpdateDeviceFeature) {
								if(gvfa("debug", $_REQUEST) == 'updatefeature'){
									echo "<BR>automated change: " . $automatedChangeMade;
									echo "<BR>" . $sqlToUpdateDeviceFeature . "<BR>";
								}
								$sqlToUpdateDeviceFeature = removeTrailingChar($sqlToUpdateDeviceFeature, ",", 2);
								$sqlToUpdateDeviceFeature = removeTrailingChar($sqlToUpdateDeviceFeature, ",", 3);
								//echo $sqlToUpdateDeviceFeature. "<BR>";
								$updateResult = mysqli_query($conn, $sqlToUpdateDeviceFeature);
								$error = mysqli_error($conn);
								if($error != ""){
									$badSql = $sqlToUpdateDeviceFeature;
									$out["error"] = $error;
									$out["sql"] = $badSql;
								}
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
		}
	} else {
		$out = ["error"=>"you lack permissions"];
		logSql("permission failed " . $formatedDateTime . ": " . $data);
	}
	
	if (endsWith(strtolower($mode), "nonjson")) {
		//let's just double-delimit using pipes and stars
		// Get the first item in the root level
		//note: this only works with a one-property object 
		$firstItem = reset($out); //this was the main thing i need to know but didn't
		// Initialize an array to hold the parsed values
		$parsedValues = [];
		foreach ($firstItem as $item) {
			$values = array_values($item);
			$parsedValues[] = implode('*', $values);
		}
		// Concatenate each item's values with "|" as the delimiter
		$result = implode('|', $parsedValues);	
	} else if($nonJsonPinData == '1' && !array_key_exists("error", $out)) { //create a very bare-bones non-JSON delimited data object to speed up data propagation to device
		$nonJsonOutString = "|";
		if(array_key_exists("device_data", $out)){
      foreach($out["device_data"] as $deviceDatum){
        if($deviceDatum["enabled"]) {
          if($deviceDatum["i2c"] > 0){
            $pinName = $deviceDatum["i2c"] . "." . $deviceDatum["pin_number"];
          } else {
            $pinName = $deviceDatum["pin_number"];
          }
          $nonJsonOutString .=  removeDelimiters($deviceDatum["name"]) . "*" . $pinName . "*" . intval($deviceDatum["value"]) .  "*" . intval($deviceDatum["can_be_analog"]) . "*" . $deviceDatum["ss"] . "|";
        }
      }
      $nonJsonOutString = substr($nonJsonOutString, 0, -1);
		}
		
		//new way to do it:
		if($latestCommandData) {
			$nonJsonOutString .= "!" . $latestCommandData["command_id"] . "|" . removeDelimiters($latestCommandData["command"]) . "|" . removeDelimiters($latestCommandData["value"]) . "|" . $latency;
		} else {
			$nonJsonOutString .= "!|||" . $latency;
		}
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

//this only works if you make sure your tenants all have distinct storagePasswords!
//which i am not actually enforcing 
function deriveDeviceIdsFromStoragePassword($storagePassword) {
	Global $conn;
	$deviceIds = [];
	$sql = "SELECT device_id FROM tenant t JOIN device d ON t.tenant_id=d.tenant_id WHERE storage_password='" . mysqli_real_escape_string($conn, $storagePassword)  . "' ORDER BY device_id ASC";
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

//needs to be made so users and tenants can be many to many
function deriveTenantFromStoragePassword($storagePassword) {
	Global $conn;
	$sql = "SELECT * FROM tenant WHERE storage_password='" . mysqli_real_escape_string($conn, $storagePassword)  . "'";
	$result = mysqli_query($conn, $sql);
	if($result){
		$tenant = mysqli_fetch_array($result, MYSQLI_ASSOC);
		return $tenant;
	}
}

function logPost($post){
	//return; //for when you don't actually want to log
	global $formatedDateTime;
	$myfile = file_put_contents('post.txt', "\n\n" . $formatedDateTime . ": " . $post, FILE_APPEND | LOCK_EX);
}

function logSql($sql){
	//return;
	//return; //for when you don't actually want to log
	global $formatedDateTime;
	$myfile = file_put_contents('sql.txt', "\n\n" . $formatedDateTime . ": " . $sql, FILE_APPEND | LOCK_EX);
}

function mergeWeatherDatum($consolidateAllSensorsToOneRecord, $existingValue, $sourceArray, $keyName, $deviceId = null, $tenantId = null) {
	Global $conn;
	//this is the order of data coming from the microcontroller in "*"-delimited form:
	$columnList = "temperature,pressure,humidity,gas_metric,wind_direction,wind_speed,wind_increment,precipitation,reserved1,reserved2,reserved3,reserved4,sensor_id,device_feature_id,sensor_name,consolidate";
	$columns = explode(",", $columnList);
	$itemNumber = array_search($keyName, $columns);
	//create an array of values keyed to the respective weather params so we can use these in the storage_function as needed
	$values = [];
	foreach($sourceArray as $index => $sourceValue) {
		$values[$columns[$index]] = $sourceValue;
	}
	if(count($sourceArray) > $itemNumber) {
		$value = trim($sourceArray[$itemNumber]);//someone might separate sensor value names with comma-space, so trim that hedge
		if($deviceId && $tenantId) {
			//where we do data manipulations involving storage_function -- which we cache so as not to look it up a million times!!
			$lookupKey = "storage_function" . "-" . $tenantId  . "-" . $deviceId . "-" . $keyName;
			$storageFunction = readMemoryCache($lookupKey, $persistTimeInMinutes = 10);
			if($storageFunction === null) {
				$sql = "SELECT storage_function FROM device_column_map WHERE device_id = " . intval($deviceId) . " AND tenant_id=" . intval($tenantId) . " AND table_name='device_log' AND column_name='" . $keyName . "'";
				$result = mysqli_query($conn, $sql);
				if($result) {
					$row = mysqli_fetch_array($result, MYSQLI_ASSOC);
					if($row){
						$storageFunction = $row["storage_function"];
						if($storageFunction == null) {
							//if we didn't find a storage function, that means there is none, so no point looking it up. so save it as "none" in the cache
							writeMemoryCache($lookupKey, "none");
						} else {
							writeMemoryCache($lookupKey, $storageFunction);
						}
					}
				}
			} else {
				if($storageFunction == "none") {
					//but never return "none" to the processing logic
					$storageFunction = null;
				}
			}
			if($storageFunction){
				$storageFunction = tokenReplace(storageFunction, $values);
				try {
				if($storageFunction) {
					eval('$value  =' . $storageFunction . ";"); //need to revisit this to make it so some bonehead admin doesn't cause chaos with bad code 
				}
				}
				catch(ParseError $error) { //this shit does not work. does try/catch ever work in PHP?
					logSql("problem with storage function: " . $storageFunction);
				}
			}
		}
	} else {
		$value = "";
	}
	if(intval($consolidateAllSensorsToOneRecord) == 1) {
		if(strtolower($existingValue) == "null"  || $existingValue == "" || strtolower($existingValue) == "nan") {
			if(count($sourceArray) > $itemNumber) {
				$out = $value;
			} else {
				$out = "NULL";
			}
		} else {
			$out = $existingValue;
		}
	} else {
		if(count($sourceArray) > $itemNumber) {
			$out = $value;
		} else {
			$out = "NULL";
		}
	}
	if($out === ""){
		$out = "NULL";
	}
	return $out;
}

function addNodeIfPresent($object, $key, $value){
	if($value != ""){
		$object[$key] = $value;
	}
	return $object;
}

function nullifyOrNumber($number){
	$out = $number;
	if($number === ""){
		$out = "NULL";
	}
	return $out;
}

function deDelimitify($inString){
	return str_replace("!", "", str_replace("|", "", str_replace("*", "", $inString)));
}

function getTenantById($tenantId){
	Global $conn;
	$sql = "SELECT * FROM tenant WHERE tenant_id=1";
	$result = mysqli_query($conn, $sql);
	if($result) {
		$row = mysqli_fetch_array($result, MYSQLI_ASSOC);
		return $row;
	}
}

function removeTrailingChar($inVal, $char, $stage = 0){
	$inVal = trim($inVal);
	if(endsWith($inVal, $char)){
		if($stage == 2){
			logSql("hanging comma: (" . $stage . ") "  . $inVal);
		}
		$inVal = substr($inVal, 0, -1);
	}
	return $inVal;
}

function sqlForStartOfPeriodScale($periodScale, $now) {
	switch ($periodScale) {
		case 'hour':
			$startOfPeriod = "DATE_FORMAT('" . $now . "', '%Y-%m-%d %H:00:00')";
			break;
		case 'day':
			$startOfPeriod = "DATE('" . $now . "')";  // Midnight of the current day
			break;
		case 'month':
			$startOfPeriod = "DATE_FORMAT('" . $now . "', '%Y-%m-01 00:00:00')";  // Start of the current month
			break;
		case 'year':
			$startOfPeriod = "DATE_FORMAT('" . $now . "', '%Y-01-01 00:00:00')";  // Start of the current year
			break;
		default:
			$startOfPeriod = "'" . $now . "'";  // Fallback to present if no match
	}
	return $startOfPeriod;
}

function getLatestCommandData($deviceId, $tenantId){
	Global $conn;
	$possibleTemporaryCommandFileName = "instant_command_" . $deviceId . ".txt";
	if(file_exists($possibleTemporaryCommandFileName)){
		$temporaryComandText = file_get_contents($possibleTemporaryCommandFileName);
		$commandArray = explode("|", $temporaryComandText);
		$value = null;
		if(count($commandArray)>1) {
			$value = $commandArray[1];
		}
		unlink($possibleTemporaryCommandFileName);
		return array("command_id"=> -2 , "command" => $commandArray[0], "value" => $value);
	}
	$sql = "SELECT * FROM command c JOIN command_type t ON c.command_type_id=t.command_type_id AND c.tenant_id=t.tenant_id WHERE device_id=" . intval($deviceId) . " AND c.tenant_id=" . $tenantId . " AND done=0 ORDER BY command_id ASC LIMIT 0,1";
	$result = mysqli_query($conn, $sql);
	if($result) {
		$row = mysqli_fetch_array($result, MYSQLI_ASSOC);
		if($row){
			$table = $row["associated_table"];
			$pk = $row["command_value"];
			$name = $row["name"];
			$value = $row["command_value"];
			$commandId = $row["command_id"];
			$valueColumn = $row["value_column"];
			//my framework kinda depends on single-column pks having the name of the table with "_id" tacked on the end. if you're doing something different, you might have to store the name of your pk
			if($valueColumn && $table) {
				$sql = "SELECT * FROM " . $table . " WHERE tenant_id=" . $tenantId . " AND " . $table . "_id=" . $pk;
				//echo $sql;
				$subResult = mysqli_query($conn, $sql);
				if($subResult) {
					$subRow = mysqli_fetch_array($subResult, MYSQLI_ASSOC);
					if($subRow && array_key_exists($valueColumn, $subRow)){
						$value = $subRow[$valueColumn];
						$value = str_replace(" ", ",", $value); //kinda a hack -- should have it better end-to-end
					}
					return array("command_id"=> $commandId , "command" => $name, "value" => $value);
				}
			} else {
				return array("command_id"=> $commandId , "command" => $name, "value" => $value);
			}
		}
	}
}

function markCommandDone($commandId, $tenantId){ 
	Global $conn, $timezone;
	$date = new DateTime("now", new DateTimeZone($timezone));//set the $timezone global in config.php
	$formatedDateTime =  $date->format('Y-m-d H:i:s');
	$sql = "UPDATE command SET done = 1, performed = '" . $formatedDateTime . "' WHERE done=0 AND command_id=" . intval($commandId) . " AND tenant_id=" . $tenantId;
	$result = mysqli_query($conn, $sql);
}

function calculateChecksum($in) {
    $checksum = 0;
    for ($i = 0; $i < strlen($in); $i++) {
        $checksum += ord($in[$i]);
    }
    return $checksum % 256;
}

function countZeroes($input) {
    $zeroCount = 0;
    for ($i = 0; $i < strlen($input); $i++) {
        if (ord($input[$i]) == 48) {  // ASCII code for '0'
            $zeroCount++;
        }
    }
    return $zeroCount % 256;
}

function rotateRight($value, $count) {
    return (($value >> $count) | ($value << (8 - $count))) & 0xFF;
}

function rotateLeft($value, $count) {
    return (($value << $count) | ($value >> (8 - $count))) & 0xFF;
}

function countSetBitsInString(string $input): int {
    $bitCount = 0;
    // Iterate over each character in the string
    for ($i = 0; $i < strlen($input); $i++) {
        $char = $input[$i];
        $asciiValue = ord($char); // Get the ASCII value of the character

        // Count the set bits in the ASCII value
        while ($asciiValue > 0) {
            $bitCount += $asciiValue & 1; // Add the least significant bit
            $asciiValue >>= 1; // Right shift to process the next bit
        }
    }
    return $bitCount % 256;
}

function simpleDecrypt($ciphertext, $key, $salt) {
    $decrypted = "";
    $keyLength = strlen($key);
    $saltLength = strlen($salt);
    $encryptedLength = strlen($ciphertext);

    for ($i = 0; $i < $encryptedLength; $i++) {
        $mix = ord($ciphertext[$i]); // Get ASCII value
        // Reverse the circular bit shift
        $mix = ($mix >> ($i % 5)) | ($mix << (8 - ($i % 5))) & 0xFF;
        // Reverse the XOR operations
        $original = $mix ^ ord($key[$i % $keyLength]) ^ ord($salt[$i % $saltLength]);
        $decrypted .= chr($original); // Convert back to character
    }
    
    return $decrypted;
}


function simpleDecrypt2($encryptedString, $dataString, $timestampString, $encryptionScheme){ //version 2!
    $nibbles = [];
	/*
    for ($i = 0; $i < 16; $i++) {
        $nibbles[$i] = ($encryptionScheme >> (4 * (15 - $i))) & 0xF;
    }
	*/
	for ($i = 0; $i < strlen($encryptionScheme); $i++) {
		// Get the i-th nibble (hex character)
		$nibble = $encryptionScheme[$i]; 
		// Convert the nibble (hex character) to an integer
		$nibbleInt = hexdec($nibble);
		$nibbles[$i] = $nibbleInt;
	}
    $dataStringChecksum = calculateChecksum($dataString);
    $timestampStringChecksum = calculateChecksum($timestampString);
    $dataStringSetBits = countSetBitsInString($dataString);
    $timestampStringSetBits = countSetBitsInString($timestampString);
    $dataStringZeroCount = countZeroes($dataString);
    $timestampStringZeroCount = countZeroes($timestampString);

    $counter = 0;
    $decryptedString = '';

    for ($i = 0; $i < strlen($encryptedString); $i+=2) {
 
		$thisByteOfDataString = ord($dataString[$counter % strlen($dataString)]);
		//echo substr($encryptedString, $i, 2) . ":" . $thisByteOfDataString . "\n";
		//echo $i  .  "==\n";

        $thisByteOfStoragePassword = hexdec(substr($encryptedString, $i, 2));
		$thisByteOfTimestamp  = ord($timestampString[$counter % strlen($timestampString)]);
		
		//echo $timestampString . ":" .  $timestampString[$counter % strlen($timestampString)] . ":" . $counter . ":" . $counter % strlen($timestampString) . "\n";
        // Reverse the encryption process based on nibbles
		$thisNibble = $nibbles[($i + 1) % (count($nibbles) )];
		//echo  PHP_INT_MAX . ":" . $encryptionScheme  . ":" . $thisNibble . ":" .  intval($i + 1) . ":" . intval(($i + 1) % (count($nibbles))) . "\n";
		//echo $i . ": " . $thisNibble . "=" . $thisByteOfStoragePassword . "\n";
		$thisByteResult = generateDecryptedByte($counter, $thisNibble, $thisByteOfStoragePassword, $thisByteOfDataString, $thisByteOfTimestamp, $dataStringChecksum, $timestampStringChecksum, $dataStringSetBits, $timestampStringSetBits, $dataStringZeroCount, $timestampStringZeroCount);
		
		//echo "&" . $thisByteOfStoragePassword . "," . $thisByteOfDataString . "|" . $thisByteOfTimestamp . "|" . $dataStringZeroCount . "|" . $timestampStringZeroCount . " via: ". $thisNibble . "=>" . $thisByteResult . "\n";
		$thisNibble = $nibbles[$i % (count($nibbles) )];
		$oldThisByteResult = $thisByteResult;
		$thisByteResult = generateDecryptedByte($counter, $thisNibble, $thisByteResult, $thisByteOfDataString, $thisByteOfTimestamp, $dataStringChecksum, $timestampStringChecksum, $dataStringSetBits, $timestampStringSetBits, $dataStringZeroCount, $timestampStringZeroCount);
		//echo "%" . $oldThisByteResult . "," . $thisByteOfDataString . "|" . $thisByteOfTimestamp . "|" . $dataStringZeroCount . "|" . $timestampStringZeroCount . " via: ". $thisNibble . "=>" . $thisByteResult . "\n";
        // Append the decrypted byte
        $decryptedString .= chr($thisByteResult);  // Append decrypted byte as char
        $counter++;
        
    }
	//echo $decryptedString . "=\n";
    return $decryptedString;


}


function generateDecryptedByte($counter, $thisNibble, $thisByteOfStoragePassword, $thisByteOfDataString, $thisByteOfTimestamp, $dataStringChecksum, $timestampStringChecksum, $dataStringSetBits, $timestampStringSetBits, $dataStringZeroCount, $timestampStringZeroCount) {
	switch ($thisNibble) {
		case 1:
			$thisByteResult = $thisByteOfStoragePassword ^ $dataStringZeroCount;
			break;
		case 2:
			$thisByteResult = $thisByteOfStoragePassword ^ $thisByteOfTimestamp;
			break;
		case 3:
			$thisByteResult = $thisByteOfStoragePassword ^ $thisByteOfDataString;
			break;
		case 4:
			$thisByteResult = $thisByteOfStoragePassword ^ rotateLeft($thisByteOfDataString, 1);
			break;
		case 5:
			$thisByteResult = $thisByteOfStoragePassword ^  rotateRight($thisByteOfDataString, 1);
			break;
		case 6:
			$thisByteResult = $thisByteOfStoragePassword ^ $dataStringChecksum;
			break;
		case 7:
			$thisByteResult = $thisByteOfStoragePassword ^ $timestampStringChecksum;
			break;
		case 8:
			$thisByteResult = $thisByteOfStoragePassword ^ $dataStringSetBits;
			break;
		case 9:
			$thisByteResult = $thisByteOfStoragePassword ^ $timestampStringSetBits;
			break;
		case 10:
			$thisByteResult = rotateRight($thisByteOfStoragePassword, 1);
			break;
		case 11:
			$thisByteResult = rotateLeft($thisByteOfStoragePassword, 1);
			break;
		case 12:
			$thisByteResult = rotateRight($thisByteOfStoragePassword, 2);
			break;
		case 13:
			$thisByteResult = rotateLeft($thisByteOfStoragePassword, 2);
			break;
		case 14:
			$thisByteResult = ~$thisByteOfStoragePassword;  // Invert the byte
			break;
		case 15:
			$thisByteResult = $thisByteOfStoragePassword ^ $timestampStringZeroCount;
			break;
		default:
			$thisByteResult = $thisByteOfStoragePassword;
			break;
	}
	return $thisByteResult;
}
//some helpful sql examples for creating sql users:
//CREATE USER 'weathertron'@'localhost' IDENTIFIED  BY 'your_password';
//GRANT CREATE, ALTER, DROP, INSERT, UPDATE, DELETE, SELECT, REFERENCES, RELOAD on *.* TO 'weathertron'@'localhost' WITH GRANT OPTION;
//GRANT ALL PRIVILEGES ON *.* TO 'weathertron'@'localhost' WITH GRANT OPTION;
 
