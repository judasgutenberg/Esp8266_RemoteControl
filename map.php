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
$tenantSelector = "";
$scaleConfig =  timeScales();
$credential = getCredential($user, "googlemap");
$credentialWeather = getCredential($user, "openweather");
$error = "";
$deviceId = gvfw("device_id");
$version = 1.1;
if($deviceId == ""){
  $deviceId = 21; //hard coded for me, probably not what you want
}
if(!$credential) {
	$error = "No Google API Key found.";
}
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
?>
<html>
<head>
  <title>Maps</title>
  <?php
  if(!$error){?>
  <script>
	let scaleConfig = JSON.parse('<?php echo json_encode(timeScales()); ?>');
	window.timezone ='<?php echo $timezone ?>';
  </script>
  <?php
 }
 ?>
 <script src="https://unpkg.com/leaflet@1.7.1/dist/leaflet.js"></script>
 <link rel="stylesheet" href="https://unpkg.com/leaflet@1.7.1/dist/leaflet.css" />
  <link rel='stylesheet' href='tool.css?version=1711570359'>
  <script src='tool.js?version=<?php echo $version?>'></script>
  <script src='tinycolor.js?version=<?php echo $version?>'></script>
  <link rel="icon" type="image/x-icon" href="./favicon.ico" />
   <style>
    /* The map needs a set height to display */
    #map {
      height: calc(100vh - 150px); /* assuming header is 60px tall */
      width: 100%;
    }
  </style
</head>
<body>
<?php
	$out .= topmostNav();
	$out .= "<div class='logo'>Map</div>\n";
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
  $selectId = "locationDropdown";
  $handler = "initMap()";
  ?>
    <div style="text-align:center;"><b><span id='greatestTime'></span></b></div>
		<div class='generalerror'><?php echo $error ?></div>
		<div style="text-align:center;"><b><span id='greatestTime'></span></b></div>
		<div id="map"></div>
    <div style='display:inline-block;vertical-align:top' >
      <div  id='singleplotdiv'>
        <table id="dataTable">
        <?php 
        echo "<tr><td class='genericformelementlabel'>Time Scale:</td><td>";
        echo genericSelect("scaleDropdown", "scale", defaultFailDown(gvfw("scale"), "day"), $scaleConfig, "onchange", $handler);
        echo "</td>";
        echo "<td class='genericformelementlabel'>Show All Data:</td><td><input type='checkbox' value='1' id='all_data' onchange='" . $handler . "'/></td>";
        echo "</tr>\n";
        echo "<tr><td class='genericformelementlabel'>Date/Time Begin:</td><td id='placeforscaledropdown'></td>";
        echo "<td class='genericformelementlabel'>Live Update:</td><td><input type='checkbox' value='1' id='live_data' onchange='" . $handler . "'/></td>";
        echo "</tr>\n";
        echo "<tr><td>Use Absolute Timespan Cusps:</td><td><input type='checkbox' value='absolute_timespan_cusps' id='atc_id' onchange='" . $handler . "'/></td>";
        $thisDataSql = "SELECT location_name as text, device_id as value FROM device WHERE location_name <> '' AND location_name IS NOT NULL AND tenant_id=" . intval($user["tenant_id"]) . " AND can_move=1 ORDER BY location_name ASC;";

        $result = mysqli_query($conn, $thisDataSql);
        if($result) {
          $selectData = mysqli_fetch_all($result, MYSQLI_ASSOC); 
        }
        echo "<td class='genericformelementlabel'>Tracker:</td><td>" . genericSelect($selectId, "deviceId", defaultFailDown(gvfw("device_id"), $deviceId), $selectData, "onchange", $handler) . "</td>";
        echo "</tr>\n";

        $device = getGeneric("device", $deviceId, $user);
        ?>
        </table>
    </div>
	</div>
</div>
<script src='map_osm.js?version=<?php echo $version?>'></script>
 <script>
    
    let currentStartDate; //a global that needs to persist through HTTP sessions in the frontend
    let justLoaded = true;
    let googleApiKey = '<?php echo $credential["password"];?>';
    let baseLatitude = '<?php echo $device["latitude"];?>';
    let baseLongitude = '<?php echo $device["longitude"];?>';
    let openWeatherApiKey = '<?php echo $credentialWeather["password"];?>';
    initMap();
  </script>
 
 
   
</body>
</html>