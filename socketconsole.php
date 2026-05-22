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
?>


<!DOCTYPE html>
<html>
<head>
    <title>Device Fast Communication</title>
    <script src='tablesort.js?version=1777640125'></script>
    <script src='tool.js?version=1777640125'></script>
    <link rel='stylesheet' href='tool.css?version=1777640125'>
</head>

<body>
<?php
 
$conn = mysqli_connect($servername, $username, $password, $database);

$deviceId = gvfw("device_id");
$deviceRow = getDevice($deviceId);
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
 



?>
<input id="messageBox" type="text">
<button onclick="sendMessage()">Send</button>

 

<div id="log" style="width:100%;height:100%;max-height:100%;overflow:auto"></div>
<br><br>


<script>
const params = new URLSearchParams(window.location.search);
let deviceId = params.get("device_id");
let log = document.getElementById("log");
let ws = null;
let reconnectTimer = null;
let cryptoKey = "<?php echo $deviceRow["last_known_key"]?>";

function connectWebSocket(){
    console.log("Connecting...");
    ws = new WebSocket("wss://<?PHP echo $_SERVER['HTTP_HOST']; ?>/socket?type=frontend&device_id=<?php echo $deviceId?>&k2=" + cryptoKey);

    ws.onopen = () => {

        console.log("CONNECTED");
        if(reconnectTimer){
            clearTimeout(reconnectTimer);
            reconnectTimer = null;
        }
    };

    ws.onmessage = (event) => {
      log.innerHTML += "<PRE>" + event.data + "</PRE>\n";
      log.scrollTop = log.scrollHeight;
      console.log("RX:", event.data);
    };

    ws.onclose = () => {
      //log.innerHTML += "DISCONNECTED\n";
      console.log("DISCONNECTED");
      getDeviceInfo(connectWebSocket);
      scheduleReconnect();
    };

    ws.onerror = (error) => {
        console.log("SOCKET ERROR", error);
        /*
            Important:
            some browsers only fire onclose afterward,
            so don't reconnect here directly
        */
    };
}

function scheduleReconnect(){
    if(reconnectTimer){
        return;
    }
    reconnectTimer = setTimeout(() => {
        reconnectTimer = null;
        connectWebSocket();
    }, 3000);
}

function sendMessage(){
  const box = document.getElementById("messageBox");
    if(!ws || ws.readyState !== WebSocket.OPEN){
        console.log("Socket not connected");
        return;
    }
    log.innerHTML +=  "<div style='color:#009900'>" + box.value + "</div>\n";
    ws.send(box.value);
}


function getDeviceInfo(nextCommand) {
		let xmlhttp = new XMLHttpRequest();
		xmlhttp.onreadystatechange = function() {
      if (xmlhttp.readyState === 4 && xmlhttp.status === 200) {
        //console.log(xmlhttp.responseText);
        let data = JSON.parse(xmlhttp.responseText);
        console.log(data);
        //i need to look this up periodically in case it has changed
        cryptoKey = data["last_known_key"];
        nextCommand();
			}
		}
		let url = "tool.php?table=device&action=json&device_id=<?php echo $deviceId?>";
		xmlhttp.open("GET", url, true);
		xmlhttp.send();

}

getDeviceInfo(connectWebSocket);

//connectWebSocket();



document
    .getElementById("messageBox")
    .addEventListener("keydown", function(event){

        if(event.key === "Enter"){

            event.preventDefault();

            sendMessage();
        }
    });
</script>
 

</body>
</html>