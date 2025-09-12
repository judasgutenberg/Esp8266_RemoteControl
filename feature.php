<!doctype html>
<?php 
//simple feature app page
//displays buttons showing the state of device_features that can be toggled off and on
//all you need to do is pass this page the correct device_feature_ids, separated by commas, and it handles all the details
//of presenting the interface.
//Gus Mueller
//May 31 2025
include("config.php");
include("site_functions.php");
include("device_functions.php");

normalizePostData(); 
//ini_set('display_errors', 1);
//ini_set('display_startup_errors', 1);
//error_reporting(E_ALL);
$version = 1.711570360;

$out = "";
$conn = mysqli_connect($servername, $username, $password, $database);
$user = autoLogin();
$tenantSelector = "";
$content = "";
$action = gvfw("action");
$deviceFeatureIds = gvfw("device_feature_id"); //can be muliple, separated by commas
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
  <link rel='stylesheet' href='dashboard.css?version=<?php echo $version;?>'>
  <link rel="manifest" href="tool.php?action=manifest&bgcolor=%23ffffff&icon=lightbulb.png&themecolor=%2366cc66&name=ESP+Remote+Control&shortname=ESPRemote&url=<?php echo  $_SERVER['REQUEST_URI'];?>">
  <script src='tool.js?version=<?php echo $version;?>'></script>
  <script src='dashboard.js?version=<?php echo $version;?>'></script>
  <meta name="mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <link rel="icon" type="image/x-icon" href="./favicon.ico" />
</head>
<body>
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
  <div style='margin: 15px'>
    <button class="smart-button" id="toggleButton_<?php echo $deviceFeatureId;?>">Toggle Feature</button>
    <div style='font-size:10px;color:#999999' id='statedisplay_<?php echo $deviceFeatureId;?>'></div>
  </div>
  <script>
    toggleDeviceFeature(<?php echo $deviceFeatureId?>,  null, false);
    document.getElementById('toggleButton_<?php echo $deviceFeatureId;?>').addEventListener('click', () => {
      toggleDeviceFeature(<?php echo $deviceFeatureId;?>, '<?php echo $hashedEntries;?>', true);
    });
  </script>
<?php
    }
?> 
  <script> 
    updateFeatureDetailButtons();
  </script>
</body>
</html>