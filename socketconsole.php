<!DOCTYPE html>
<html>
<head>
    <title>ESP8266 WebSocket Frontend</title>
    <script src='tablesort.js?version=1777640125'></script>
    <script src='tool.js?version=1777640125'></script>
    <link rel='stylesheet' href='tool.css?version=1777640125'>
</head>

<body>
<?php
include("config.php");
include("site_functions.php");
include("device_functions.php");
$conn = mysqli_connect($servername, $username, $password, $database);

$deviceId = gvfw("device_id");
$deviceRow = getDevice($deviceId);
//var_dump($deviceRow);
?>

<div class='issuesheader'><?php echo $deviceRow["name"] ?> Fast Console</div>

<div id="log" style="width:600px;height:400px;max-height:400px;overflow:auto"></div>
<br><br>

<input id="messageBox" type="text">
<button onclick="sendMessage()">Send</button>

<script>
const params = new URLSearchParams(window.location.search);
const deviceId = params.get("device_id");
let log = document.getElementById("log");
let ws = null;
let reconnectTimer = null;

function connectWebSocket(){
    console.log("Connecting...");
    ws = new WebSocket(
        "wss://<?PHP echo $_SERVER['HTTP_HOST']; ?>/socket?type=frontend&device_id=<?php echo $deviceId?>&k2=<?php echo $deviceRow["last_known_key"]?>"
    );

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

connectWebSocket();

</script>
 

</body>
</html>