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

$out = "";
$conn = mysqli_connect($servername, $username, $password, $database);
$user = autoLogin();
$tenantSelector = "";
$content = "";
$action = gvfw("action");
$deviceFeatureIds = gvfw("device_feature_id");
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
  <link rel='stylesheet' href='tool.css?version=1711570359'>
  <script src='tool.js'></script>
  <link rel="icon" type="image/x-icon" href="./favicon.ico" />
</head>

<body>
<script>
  function toggleDeviceFeature(deviceFeatureId, hashedEntries,  actuallySetState) {
      state = !state;
      const value = state ? 'true' : 'false';
      let url = 'tool.php?action=genericFormSave&table=device_feature&primary_key_name=device_feature_id&primary_key_value=' + deviceFeatureId + '&value=' + value + '&name=value&hashed_entities=' + hashedEntries;
      if(!actuallySetState) {
        url = "data:application/json,{}";
        state = !state;  //oh, we didn't send a change state, so flip it back to the way things were
      }
      let bgColor = "#ccccff";
      
      let urlGet = 'tool.php?action=json&table=device_feature';
      if(deviceFeatureId) {
        urlGet += '&device_feature_id=' + deviceFeatureId;
      }
      console.log(urlGet);
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
                if("value" in data) {
                  state = changeButton(data)
                } else {
                  Object.keys(data).forEach(key => {
                    state = changeButton(data[key])
                  });
                }
            });
          } else {
            //alert('Failed to send command');
          }
        })
        .catch(err => alert('Error: ' + err));
  
 }
 
 function changeButton(data) {
    if(data["value"] == 1) {
      state = true;
      stateDisplay = "Currently ON";
      bgColor = "#ffe7cc"
    } else {
      state = false;
      stateDisplay = "Currently OFF";
      bgColor = "#e7e7ff"
    }
    if(data["last_known_device_value"] === "1" && data["value"] === "1") {
      bgColor = "#ffffcc"
    }
    if(data["last_known_device_value"] === "0" && data["value"] === "0") {
      bgColor = "#ccccff"
    }
    let deviceFeatureId = data["device_feature_id"];
    if(document.getElementById("statedisplay_" + deviceFeatureId)) {
      document.getElementById("statedisplay_" + deviceFeatureId).innerHTML = stateDisplay;
      document.getElementById("toggleButton_" + deviceFeatureId).style.backgroundColor  = bgColor;
      document.getElementById("toggleButton_" + deviceFeatureId).innerHTML = data["name"];
    }
    return state
 }
 
function updateButtons() {
  setTimeout(()=>{
  toggleDeviceFeature(null, null,  false);
  updateButtons();
  },
  5000);
}


  </script>

		
<?php
		$deviceFeatureIdArray = explode(",", $deviceFeatureIds);
		for($i = 0; $i< count($deviceFeatureIdArray); $i++) {
      $deviceFeatureId = $deviceFeatureIdArray[$i];
      $table = "device_feature";
      $primaryKeyName = $table . "_id";
      $primaryKeyValue = $deviceFeatureId;
      $name  = "value";
      //echo $name . $table . $primaryKeyName  . $primaryKeyValue . "<br/>\n";
      $hashedEntries =  hash_hmac('sha256', $name . $table . $primaryKeyName . $primaryKeyValue, $encryptionPassword);
  ?>

  <div style='margin: 15px'><button id="toggleButton_<?php echo $deviceFeatureId;?>">Toggle Feature</button><div id='statedisplay_<?php echo $deviceFeatureId;?>'></div></div>

  <script>
    var state = false; // start as OFF
    toggleDeviceFeature(<?php echo $deviceFeatureId?>,  '<?php echo $hashedEntries;?>', false);
    

    document.getElementById('toggleButton_<?php echo $deviceFeatureId;?>').addEventListener('click', () => {
      toggleDeviceFeature(<?php echo $deviceFeatureId;?>, '<?php echo $hashedEntries;?>', true);
    });
    
     updateButtons();
  </script>
   <?php
  }
  ?> 
  




	</div>


</body>

</html>
 

 