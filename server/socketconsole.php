<?php 
include("other_page_login.php");
$date = new DateTime("now", new DateTimeZone($timezone));//set the $timezone global in config.php
$formattedDateTime =  $date->format('Y-m-d H:i:s');


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

$conn = mysqli_connect($servername, $username, $password, $database);
$cryptoKey = "";
$deviceId = gvfw("device_id");
$startupCommand = gvfw("command");
if($deviceId) {
  $deviceRow = getDevice($deviceId);
  $cryptoKey =  $deviceRow["last_known_key"];
}


?>


<!DOCTYPE html>
<html>
<head>
    <title>Device Fast Communication</title>
    <script>
      let cryptoKey = "<?php echo $cryptoKey?>";
      let host = "<?PHP echo $_SERVER['HTTP_HOST']; ?>";
    </script>
    <script src='tablesort.js?version=1777640125'></script>
    <script src='tool.js?version=1777640125'></script>
    <link rel='stylesheet' href='tool.css?version=1777640125'>
</head>

<body>
<?php
 

//var_dump($deviceRow);



  $out .= topmostNav();
  if($deviceId) {
    $out .= "<div class='logo'>" .$deviceRow["name"] . "</div>";
  }
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
 
if(!$deviceId) {
  echo "<br/><br/><div class='error'>There was no device selected.</div>";
}  
echo "<br/>Device:";
$thisDataSql = "SELECT name as text, device_id as value FROM device WHERE tenant_id=" . intval($user["tenant_id"]) . "  ORDER BY name ASC;";
$result = mysqli_query($conn, $thisDataSql);
$selectData = mysqli_fetch_all($result, MYSQLI_ASSOC);
//var_dump($selectData);
$handler = "let deviceId=document.getElementById('device_id');location.href='socketconsole.php?command=" . urlencode($startupCommand ). "&device_id=' + deviceId[deviceId.selectedIndex].value";
echo genericSelect("device_id", "device_id", $deviceId, $selectData, "onchange", $handler);
if($deviceId) {


  if($startupCommand && $user) {
    $sql = "INSERT INTO command_log(recorded, device_id, tenant_id, command_text) VALUES ('" . $formattedDateTime . "'," . intval($deviceId). "," . intval($user["tenant_id"]) . ",'" . mysqli_real_escape_string($conn, $startupCommand) . "')"; 
    $result = mysqli_query($conn, $sql);
    //echo $sql;
  }
?>
<div>
<form>
<input id="messageBox" type="text" style="width:400px" name="messageBox" autocomplete="on">
<button onclick="sendMessage();return false">Send</button>
</form>

</div>
 

<div id="log" style="width:100%;height:85vh;max-height:85vh;overflow:auto"></div>
<br><br>
<?php
}

?>
 
 
<script src='socket.js?version=1777640125'></script>
</body>
</html>
