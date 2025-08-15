<!doctype html>
<?php 
include("config.php");
include("site_functions.php");
include("device_functions.php");
//ini_set('display_errors', 1);
//ini_set('display_startup_errors', 1);
//error_reporting(E_ALL);

if(array_key_exists( "locationId", $_REQUEST)) {
	$locationId = $_REQUEST["locationId"];
} else {
	$locationId = 1;
}
$poser = null;
$poserString = "";
$out = "";
$conn = mysqli_connect($servername, $username, $password, $database);
$user = autoLogin();
$tenantSelector = "";
$scaleConfig =  timeScales();
$credential = getCredential($user, "googlemap");
$error = "";
$deviceId = gvfw("device_id");
if($deviceId == 0){
  $deviceId = 21;
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

  <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.7.0/chart.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-date-fns@2.0.0/dist/chartjs-adapter-date-fns.bundle.min.js"></script>
  <script>
	let scaleConfig = JSON.parse('<?php echo json_encode(timeScales()); ?>');
	window.timezone ='<?php echo $timezone ?>';
  </script>
  <?php
 }
 ?>
  <link rel='stylesheet' href='tool.css?version=1711570359'>
  <script src='tool.js'></script>
  <link rel="icon" type="image/x-icon" href="./favicon.ico" />
   <style>
    /* The map needs a set height to display */
    #map {
      height: calc(100vh - 120px); /* assuming header is 60px tall */
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
			$poserString = " posing as <span class='poserindication'>" . $poser["email"] . "</span> (<a href='?action=disimpersonate'>unpose</a>)";

		}
		$out .= "<div class='loggedin'>You are logged in as <b>" . $user["email"] . "</b>" .  $poserString . "  on " . $user["name"] . " <div class='basicbutton'><a href=\"?action=logout\">logout</a></div></div>\n";
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
		<div id="map"></div>


    
    <div style='display:inline-block;vertical-align:top' >
 
      <div  id='singleplotdiv'>
        <table id="dataTable">
        
        <?php 
        echo "<tr><td>Time Scale:</td><td>";
        echo genericSelect("scaleDropdown", "scale", defaultFailDown(gvfw("scale"), "day"), $scaleConfig, "onchange", $handler);
        echo "</td></tr>";
        echo "<tr><td>Date/Time Begin:</td><td id='placeforscaledropdown'></td></tr>";
        //lol, it's easier to specify an object in json and decode it than it is just specify it in PHP

        $thisDataSql = "SELECT location_name as text, device_id as value FROM device WHERE location_name <> '' AND location_name IS NOT NULL AND tenant_id=" . intval($user["tenant_id"]) . " AND can_move=1 ORDER BY location_name ASC;";
        $result = mysqli_query($conn, $thisDataSql);
        if($result) {
          $selectData = mysqli_fetch_all($result, MYSQLI_ASSOC); 
        }

        //$selectData = json_decode('[{"text":"Outside Cabin","value":1},{"text":"Cabin Downstairs","value":2},{"text":"Cabin Watchdog","value":3}]');
        //var_dump($selectData);
        //echo  json_last_error_msg();
        
        echo "<tr><td>Tracker:</td><td>" . genericSelect($selectId, "deviceId", defaultFailDown(gvfw("device_id"), $deviceId), $selectData, "onchange", $handler) . "</td></tr>";
        ?>
        </table>
    </div>
	</div>

</div>
<script src='map.js'></script>
 <script>
    let map;
    let currentStartDate; //a global that needs to persist through HTTP sessions in the frontend
    let justLoaded = true;
    let defaultDeviceId = <?php echo $deviceId?>;

  </script>
    <script async src="https://maps.googleapis.com/maps/api/js?key=<?php echo $credential["password"]?>&callback=initMap"></script>
</body>

</html>
 

 