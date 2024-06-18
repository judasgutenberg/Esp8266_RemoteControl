const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<title>local data</title>
</head>
<style>
*{margin: 0;padding: 0;}

body{
  background:#544947;
  font-family:sans-serif;
}
h2{
  font-size:14px;
}
.widget{
  margin:100px auto;
  height: 330px;
  position: relative;
  width: 500px;
}

.devices{
  background:#ccccff;
  border-radius:0 0 5px 5px;
  font-family:sans-serif;
  font-weight:200;
  height:130px;
  width:100%;
}

.sensorcluster {
  display:block;
  text-align: left;
  margin-bottom:1px;
  background-color:black
}

.sensors {
  border-collapse: collapse;
  width: 100%;
}
.sensors td, .sensors th {
  border: 1px solid #cccccc;
  background-color: #999999;
  padding: 2px;
}

.weatherdata{
  text-align: right;
}

.sensorname{
  text-align: right;
  font-weight: bold;
}
 
#devicename {
  text-align: center;
  color:white;
  background-color: #000000;
  font-size: 14px;
}
#sensorheader {
  text-align: center;
  color:white;
  font-size: 14px;
  display:none;
} 
</style>
<body>

<div class="widget"> 
  <div id='devicename'>Your Device</div>
  <div class="devices" id="devices">    
  </div>
  <div id='sensorheader'>Sensors</div>
  <table class="sensors">
    <tbody id="sensors"></tbody>    
  </table>
   
</div>

<script>
setInterval(showPinValues, 7000);

function updateWeatherDisplay() {
    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        let txt = this.responseText;
        let firstLine = txt.split("|")[0];
        //console.log(firstLine);
        let weatherLines = firstLine.split("!");
        let sensorsDiv = document.getElementById("sensors");
        let firstSensorDone = false;
        let sensorCursor = 0;
        for(weatherLine of weatherLines){  
          if(weatherLine.indexOf("*") > -1) {
            console.log(weatherLine);
            let weatherData = weatherLine.split("*");
            //temperatureValue*pressureValue*humidityValue*gasValue*sensorType*deviceFeatureId"*sensorName; //using delimited data instead of JSON to keep things simple
            let temperature = weatherData[0];
            let pressure = weatherData[1];
            let humidity = weatherData[2];
            let sensorName = weatherData[14];
            let potentialWeatherDisplay = "";
            let parentDiv = "";
            let weHadData = false;
            parentDiv += "<tr id='sensor" + sensorCursor + "'>";
            if(firstSensorDone) {
              potentialWeatherDisplay += "<td class='sensorname'>" + sensorName + "</td>";
            } else {
              potentialWeatherDisplay += "<td></td>";
            }
            if(temperature != "NULL" && !isNaN(temperature) && temperature != "") {
              potentialWeatherDisplay += "<td class='weatherdata'>" + (parseFloat(temperature) * 1.8 + 32).toFixed(2) + "&deg; F" + "</td>";
              //document.getElementById("temperature").innerHTML = (parseFloat(temperature) * 1.8 + 32).toFixed(2) + "&deg; F"; 
              weHadData = true;
            }
            if(pressure != "NULL"  && !isNaN(pressure) && pressure != "" ) {
              potentialWeatherDisplay += "<td class='weatherdata'>" +  parseFloat(pressure).toFixed(2) + "mm Hg" + "</td>";
              //document.getElementById("pressure").innerHTML = parseFloat(pressure).toFixed(2) + "mm Hg";
              weHadData = true;
            }
            if(humidity != "NULL" && !isNaN(humidity) && humidity != "") {
             potentialWeatherDisplay += "<td class='weatherdata'>" +  parseFloat(humidity).toFixed(2) + "% rel" + "</td>";
              //document.getElementById("humidity").innerHTML = parseFloat(humidity).toFixed(2) + "% rel";
             weHadData = true;
            } 
            parentDiv += "</tr>";
           
            if(weHadData) {
              document.getElementById('sensorheader').style.display = 'block';
              let particularSensorDiv = document.getElementById('sensor' + sensorCursor);
              if(!particularSensorDiv) {
                sensorsDiv.innerHTML += parentDiv;
              }
              particularSensorDiv = document.getElementById('sensor' + sensorCursor);
              particularSensorDiv.innerHTML = potentialWeatherDisplay;
            }
          }
          firstSensorDone = true;
          sensorCursor++;
        }
      }
    };
    xhttp.open("GET", "/weatherdata", true);
    xhttp.send();
}
    
function updateLocalValues(id, value) {
    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
    }
    console.log(value);
    let onValue = "0";
    if(value) {
      onValue = "1";
    }
    xhttp.open("GET", "writeLocalData?id=" + id + "&on=" + onValue, true);
    xhttp.send();
}
    
function showPinValues(){
    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        let txt = this.responseText;
        let arr = JSON.parse(txt);  
        let deviceName = arr.device;
        let pins = arr.pins;
        if(pins.length> 0) {
         document.getElementById("devices").innerHTML = "";
        }
        document.getElementById("devicename").innerHTML = deviceName;
        let pinCursor = 0;
        //console.log(arr);
        for(let obj of pins) {
          //console.log(obj);
          let checked = "";
          let i2cString = "";
          let id = obj["id"];
          let pin = id;
          let friendlyName = obj["name"];
          if(id.indexOf(".")>-1) {
            let pinInfo = id.split(".");
            let i2c = pinInfo[0];
            pin = pinInfo[1];
            i2cString = " via I2C: " + i2c;
          }
          if(obj["value"] == '1') {
            checked = "checked";
          }
          document.getElementById("devices").innerHTML  +=  " <input onchange='updateLocalValues(this.name, this.checked)' " + checked + " type='checkbox' name=\"" + id +  "\" value='1' > <b>" + friendlyName +  "</b>, pin " + pin + " " + i2cString + "<br/>";
          pinCursor++;
        }
      } 

    };
   xhttp.open("GET", "readLocalData", true); //Handle readADC server on ESP8266
   xhttp.send();
   setTimeout(function(){
      updateWeatherDisplay(); 
      }, Math.random()* 4000); //i make this a little non-deterministic so the DHT sensors won't get stuck in a pattern where they are read to frequently
}
</script>
</body>
</html>
)=====";
