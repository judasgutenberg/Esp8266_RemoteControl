<?php 
include("other_page_login.php");

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

$deviceId = gvfw("device_id");
if($deviceId) {
  $deviceRow = getDevice($deviceId);
}
$cryptoKey =  $deviceRow["last_known_key"];

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
  $out .= "<div class='logo'>" .$deviceRow["name"] . "</div>";
  		
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
  echo "<div class='error'>There was no device selected.</div>";
} else {

?>
<div>
<form>
<input id="messageBox" type="text" style="width:400px" name="messageBox" autocomplete="on">
<button onclick="sendMessage();return false">Send</button>
</form>

</div>
 

<div id="log" style="width:100%;height:600px;max-height:600px;overflow:auto"></div>
<br><br>
<?php
}

?>
 
 
<script src='socket.js?version=1777640125'></script>
</body>
</html>