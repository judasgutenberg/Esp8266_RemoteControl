<!DOCTYPE html>
<html>
<head>
    <title>ESP8266 WebSocket Frontend</title>
    <script src='tablesort.js?version=1777640125'></script>
    <script src='tool.js?version=1777640125'></script>
    <link rel='stylesheet' href='tool.css?version=1777640125'>
</head>

<body>

<h1>ESP8266 Console</h1>

<div id="log" style="width:600px;height:400px;scroll:auto"></div>
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
        "ws://<?PHP echo $_SERVER['HTTP_HOST']; ?>:8080/?type=frontend&device_id=" +
        encodeURIComponent(deviceId)
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