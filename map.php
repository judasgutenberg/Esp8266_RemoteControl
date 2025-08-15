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
      height: 80vh;
      width: 100%;
    }
  </style
</head>

<body>

<?php

	$out .= topmostNav();
	$out .= "<div class='logo'>Inverter Data</div>\n";
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

        $thisDataSql = "SELECT location_name as text, device_id as value FROM device WHERE location_name <> '' AND location_name IS NOT NULL AND tenant_id=" . intval($user["tenant_id"]) . " ORDER BY location_name ASC;";
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
    window.initMap = async function () {

      const queryParams = new URLSearchParams(window.location.search);
      let locationIdArray = [];
      let scale = queryParams.get('scale');
      let locationId = queryParams.get('location_id');
      let periodAgo = queryParams.get('period_ago');
      let locationIds = queryParams.get('location_ids');
      let plotType = "single";
      let absoluteTimespanCusps = queryParams.get('absolute_timespan_cusps');
      let atcCheckbox = document.getElementById("atc_id");
      let yearsAgoToShow = queryParams.get('years_ago');
      let greatestTime = "2000-01-01 00:00:00";

      let url = new URL(window.location.href);

 
      if(!scale){
        scale = "day";
      }
      let locationIdDropdown = document.getElementById('locationDropdown');
      if(!justLoaded){
        locationId  = locationIdDropdown[locationIdDropdown.selectedIndex].value
      }
 
 
      if(document.getElementById('scaleDropdown')  && !justLoaded){
        scale = document.getElementById('scaleDropdown')[document.getElementById('scaleDropdown').selectedIndex].value;
      }	
      url.searchParams.set("scale", scale);


      let periodAgoDropdown = document.getElementById('startDateDropdown');	

      if(periodAgoDropdown){
        if(!justLoaded){
          periodAgo = periodAgoDropdown[periodAgoDropdown.selectedIndex].value;
        } else {
          periodAgo = 0;
        }
        if(currentStartDate == periodAgoDropdown[periodAgoDropdown.selectedIndex].text){
          thisPeriod = periodAgo;
          periodAgo = false;
          //console.log("dates unchanged");
        }
        currentStartDate = periodAgoDropdown[periodAgoDropdown.selectedIndex].text;
      }	
      periodAgo = calculateRevisedTimespanPeriod(scaleConfig, periodAgo, scale, currentStartDate);
      url.searchParams.set("period_ago", periodAgo);
 
      //update the URL without changing actual location
      history.pushState({}, "", url);










      let deviceDropdown = document.getElementById("locationDropdown");
      let deviceId = deviceDropdown[deviceDropdown.selectedIndex].value;
      // Create the map centered somewhere reasonable
      map = new google.maps.Map(document.getElementById('map'), {
        center: { lat: 40, lng: -100 }, // fallback center
        zoom: 4
      });

      try {
        // Fetch your JSON data from backend
        const response = await fetch("data.php?scale=" + scale + "&period_ago=" + periodAgo + "&mode=getMap&device_id=" + deviceId);
        const locations = await response.json();
		    console.log(locations);
        // If the JSON looks like: [{latitude: 40.7, longitude: -74.0}, ...]
        locations.points.forEach(loc => {
          new google.maps.Marker({
            position: { lat: parseFloat(loc.latitude), lng: parseFloat(loc.longitude) },
            map: map
          });
        });

        // Optional: re-center map to fit all markers
        if (locations.points.length) {
          const bounds = new google.maps.LatLngBounds();
          locations.points.forEach(loc => bounds.extend({
            lat: parseFloat(loc.latitude), lng: parseFloat(loc.longitude)
          }));
          map.fitBounds(bounds);
        }
        //console.log(scaleConfig);
        createTimescalePeriodDropdown(scaleConfig, periodAgo, scale, currentStartDate, 'change', 'initMap()', 'device_log', deviceId);
        justLoaded = false;
      } catch (err) {
        console.error('Error loading location data:', err);
      }
    }
  </script>
    <script async src="https://maps.googleapis.com/maps/api/js?key=<?php echo $credential["password"]?>&callback=initMap"></script>
</body>

</html>
 

 