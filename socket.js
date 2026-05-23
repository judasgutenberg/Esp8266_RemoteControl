const params = new URLSearchParams(window.location.search);
let deviceId = params.get("device_id");
let log = document.getElementById("log");
let ws = null;
let reconnectTimer = null;



function connectWebSocket(){
    connectTimeout = setTimeout(function() {
        console.log("WebSocket connection timeout");
        getDeviceInfo(null, deviceId);

    }, 2000);

    console.log("Connecting...");
    ws = new WebSocket("wss://" + host + "/socket?type=frontend&device_id=" + deviceId + "&k2=" + cryptoKey);

    ws.onopen = () => {

        console.log("CONNECTED");
        if(reconnectTimer){
            clearTimeout(reconnectTimer);
            clearTimeout(connectTimeout);
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
      getDeviceInfo(connectWebSocket, deviceId);
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


function getDeviceInfo(nextCommand, deviceId) {
		let xmlhttp = new XMLHttpRequest();
		xmlhttp.onreadystatechange = function() {
      if (xmlhttp.readyState === 4 && xmlhttp.status === 200) {
        //console.log(xmlhttp.responseText);
        let data = JSON.parse(xmlhttp.responseText);
        console.log(data);
        //i need to look this up periodically in case it has changed
        cryptoKey = data["last_known_key"];
        if(nextCommand) {
          nextCommand();
        }
			}
		}
		let url = "tool.php?table=device&action=json&device_id=" + deviceId;
		xmlhttp.open("GET", url, true);
		xmlhttp.send();

}

getDeviceInfo(connectWebSocket, deviceId);

//connectWebSocket();



document
    .getElementById("messageBox")
    .addEventListener("keydown", function(event){

        if(event.key === "Enter"){

            event.preventDefault();

            sendMessage();
        }
    });
