<!doctype html>
<?php 
//simple feature app page
//displays a button showing the state of a device_feature that can be toggled off and on
//all you need to do is pass this page the correct device_feature_id and it handles all the details
//of presenting the interface.
//Gus Mueller
//May 31 2025
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
$content = "";
$action = gvfw("action");
$deviceFeatureId = gvfw("device_feature_id");
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
  <title>Device Feature Toggle</title>
  <!--For offline ESP graphs see this tutorial https://circuits4you.com/2018/03/10/esp8266-jquery-and-ajax-web-server/ -->
  <!--<script src = "https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.7.3/Chart.min.js"></script>--> 
  <link rel='stylesheet' href='tool.css?version=1711570359'>
  <script src='tool.js'></script>
  <link rel="icon" type="image/x-icon" href="./favicon.ico" />
</head>

<body>
<?php
	//$out .= topmostNav();
	//$out .= "<div class='logo'>Inverter Data</div>\n";
	if($user) {
		//$out .= "<div class='outercontent'>";
		if($poser) {
			$poserString = " posing as <span class='poserindication'>" . $poser["email"] . "</span> (<a href='?action=disimpersonate'>unpose</a>)";

		}
		//$out .= "<div class='loggedin'>You are logged in as <b>" . $user["email"] . "</b>" .  $poserString . "  on " . $user["name"] . " <div class='basicbutton'><a href=\"?action=logout\">logout</a></div></div>\n";
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

  <button id="toggleButton">Toggle Light</button><div id='statedisplay'></div>

  <script>
    let state = false; // start as OFF
    toggleLight(false);
    

    document.getElementById('toggleButton').addEventListener('click', () => {
      toggleLight(true);
    });
    
    
    
  function toggleLight(actuallySetState) {
  
      state = !state;
      const value = state ? 'true' : 'false';
      let url = 'tool.php?action=genericFormSave&table=device_feature&primary_key_name=device_feature_id&primary_key_value=<?php echo $deviceFeatureId;?>&value=' + value + '&name=value&hashed_entities=neeueWZRf95X6';
      if(!actuallySetState) {
        url = "data:application/json,{}";
        state = !state;  //oh, we didn't send a change state, so flip it back to the way things were
      }
      const urlGet = 'tool.php?action=json&table=device_feature&device_feature_id=<?php echo $deviceFeatureId;?>';
      fetch(url)
        .then(response => {
          if (response.ok) {
            fetch(urlGet).then(response => {
              if(response.ok) {
                console.log(response);
                return response.json();

              }
            }).then(data => {
                console.log(data);
                if(data["value"] == 1) {
                  state = true;
                  stateDisplay = "Currently ON";
                } else {
                  state = false;
                  stateDisplay = "Currently OFF";
                }
                document.getElementById("statedisplay").innerHTML = stateDisplay;
                document.getElementById("toggleButton").innerHTML = data["name"];
                //alert(`Light turned ${state ? 'ON' : 'OFF'}`);
            });
          } else {
            //alert('Failed to send command');
          }
        })
        .catch(err => alert('Error: ' + err));
  
 }
  </script>





	</div>


</body>

</html>
 

 