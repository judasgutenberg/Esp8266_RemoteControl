<!doctype html>
<?php 
include("config.php");
include("site_functions.php");
include("device_functions.php");
//ini_set('display_errors', 1);
//ini_set('display_startup_errors', 1);
//error_reporting(E_ALL);
 
$poser = null;
$poserString = "";
$out = "";
$conn = mysqli_connect($servername, $username, $password, $database);
$user = autoLogin();
$deviceId = 0;
 
if($user){
	$devices = getDevices($user["tenant_id"], false);
	$weatherColumns = getGraphColumns($user["tenant_id"]);
	//var_dump($devices);
	if(array_key_exists( "device_id", $_REQUEST)) {
		$deviceId = $_REQUEST["device_id"];
	} else {
		if($devices  && $devices[0]){
			$deviceId = $devices[0]["device_id"]; //picking the first device now, but this could be in the tenant config
		}
	}
}
 

$tenantSelector = "";
$scaleConfig =  timeScales();

$content = "";
$action = gvfw("action");
if ($action == "login") {
	$tenantId = gvfa("tenant_id", $_GET);
	$tenantSelector = loginUser($tenantId);
} else if ($action == 'settenant') {
	setTenant(gvfw("encrypted_tenant_id"));
} else if ($action == "logout") {
	logOut();
	header("Location: ?action=login");
	die();
	
}
if(!$user) {
	if(gvfa("password", $_POST) != "" && $tenantSelector == "") {
		$content .= "<div class='genericformerror'>The credentials you entered have failed.</div>";
	}
    if(!$tenantSelector) {
		$content .= loginForm();
	} else {
		$content .= $tenantSelector;
	}


	echo bodyWrap($content, $user, "", null);
	die();
}
 
function multiDevicePicker($tenantId, $devices) {
	
	$out = "";
	foreach($devices as $device){
		if($device["location_name"]){
			$out .= "<div><input onchange='getWeatherData()' name='specificDevice' type='checkbox' value='" . $device["device_id"] . "'/> ";
			$out .= $device["location_name"];
			$out .= "</div>";
		}
	}
	return $out;
}

function plotTypePicker($type, $handler){
	$checked = "";
	if(gvfw("plot_type") == $type || gvfw("plot_type")=="" && $type == "single"){
		$checked = "checked";
	}
	$out = "<div class='listtitle' style='opacity:1'><input type='radio' id='plottype' " . $checked . " name='plottype' onchange='" . $handler . "' value='" . $type . "'>" . ucfirst($type) . "-location Plot</div>";
	return $out;
}
?>
<html>

<head>
  <title>Weather Information</title>
  <!--For offline ESP graphs see this tutorial https://circuits4you.com/2018/03/10/esp8266-jquery-and-ajax-web-server/ -->
  <!--<script src = "https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.7.3/Chart.min.js"></script>--> 
  <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.7.0/chart.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-date-fns@2.0.0/dist/chartjs-adapter-date-fns.bundle.min.js"></script>
  <script src = "tinycolor.js"></script>  
  <script>
  	let scaleConfig = JSON.parse('<?php echo json_encode(timeScales()); ?>');
	let devices = [];//JSON.parse('<?php echo json_encode($devices); ?>');
	window.timezone ='<?php echo $timezone ?>';
  </script>
  <script src='tool.js'></script>
  <link rel='stylesheet' href='tool.css?version=1711570359'>
  <link rel="icon" type="image/x-icon" href="./favicon.ico" />

</head>

<body>
<?php
  $out .= topmostNav();
  $out .= "<div class='logo'>Weather Data</div>";
  		
  if($user) {
	$out .= "<div class='outercontent'>";
    if($poser) {
      $poserString = " posing as <span class='poserindication'>" . userDisplayText($poser) . "</span> (<a href='?action=disimpersonate'>unpose</a>)";

    }
    $out .= "<div class='loggedin'>You are logged in as <b>" . userDisplayText($user) . "</b>" .  $poserString . "  on " . $user["name"] . " <div class='basicbutton'><a href=\"?action=logout\">logout</a></div></div>\n";
	}
	else
	{
    //$out .= "<div class='loggedin'>You are logged out.  </div>\n";
	} 
	$out .= "<div>\n";
    $out .= "<div class='documentdescription'>";
	
 
	$out .= "</div>";
	//$out .= "<div class='innercontent'>";
	echo $out; 
  ?>

		<div style="text-align:center;"><b><span id='greatestTime'></span></b></div>
		<div class="chart-container" style="width: 100%; height: 70vh;">
			<canvas id="Chart"></canvas>
		</div>
		<div>
		<div style='display:inline-block;vertical-align:top'>
			<?php
				$selectId = "locationDropdown";
				$handler = "getWeatherData()";
				echo "\n<table>\n";
				echo "<tr><td>Time Scale:</td><td>";
				echo genericSelect("scaleDropdown", "scale", defaultFailDown(gvfw("scale"), "day"), $scaleConfig, "onchange", $handler);
				echo "</td></tr>";
				echo "<tr><td>Date/Time Begin:</td><td id='placeforscaledropdown'></td></tr>";
				//absolute_timespan_cusps
				echo "<tr><td>Use Absolute Timespan Cusps</td><td><input type='checkbox' value='absolute_timespan_cusps' id='atc_id' onchange='" . $handler . "'/></td></tr>";
				echo "\n</table>\n";
				echo "<br/><button onclick='getWeatherData(1);return false'>show data from a year before</button>";
				echo "<br/><button onclick='editColumns();return false'>edit weather columns</button>";
				
				?>
				</div>

				<div style='display:inline-block;vertical-align:top' >
					<?php echo plotTypePicker("single", $handler); ?>
					<div  id='singleplotdiv'>
					<table id="dataTable">
					<?php 
					//lol, it's easier to specify an object in json and decode it than it is just specify it in PHP

					$thisDataSql = "SELECT location_name as text, device_id as value FROM device WHERE location_name <> '' AND location_name IS NOT NULL AND tenant_id=" . intval($user["tenant_id"]) . " ORDER BY location_name ASC;";
					$result = mysqli_query($conn, $thisDataSql);
					if($result) {
						$selectData = mysqli_fetch_all($result, MYSQLI_ASSOC); 
					}

					//$selectData = json_decode('[{"text":"Outside Cabin","value":1},{"text":"Cabin Downstairs","value":2},{"text":"Cabin Watchdog","value":3}]');
					//var_dump($selectData);
					//echo  json_last_error_msg();
					
					echo "<tr><td>Location:</td><td>" . genericSelect($selectId, "deviceId", defaultFailDown(gvfw("device_id"), $deviceId), $selectData, "onchange", $handler) . "</td></tr>";
					?>
					</table>
				</div>
		</div>
		<div style='display:inline-block;vertical-align:top'>
		<?php
			echo plotTypePicker("multi", $handler);
			echo "<div  id='multiplotdiv'>";
			echo "<div>Weather Column: ";
			//$weatherColumns = ["temperature", "pressure", "humidity"]; //now derived programmatically
			echo "</div>";
			echo genericSelect("specific_column", "specific_column", defaultFailDown(gvfw("specific_column"), "temperature"), $weatherColumns, "onchange", $handler);
			echo multiDevicePicker($user["tenant_id"], $devices);
			echo "</div>";
		?>

		</div>
		<div style='display:inline-block;vertical-align:top' id='officialweather'>
		</div>
	</div>
<!--</div>-->
</div>
</div>
<script>
let defaultdeviceId = <?php echo $deviceId?>;
let columnsWeCareAbout = <?php echo json_encode($weatherColumns)?>;

</script>
<script src='weather.js'></script>
 
</body>
</html>