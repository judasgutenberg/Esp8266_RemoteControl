//produces a web page viewable only on the local network allowing control of devices and reading of sensors
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
.webapp{
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

<div class="webapp"> 
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
let columnNames = "temperature*pressure*humidity*gas*windDirection*windSpeed*windIncrement*precipitation*reserved1*reserved2*reserved3*reserved4".split("*");
let units = "&deg; F*mm Hg*% rel*gas*&deg;*mph*/min*/min*reserved1*reserved2*reserved3*reserved4".split("*");

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
        for(let weatherLine of weatherLines){
          let parentDiv = "";  
          let potentialWeatherDisplay = "";
          if(weatherLine.indexOf("*") > -1) {
            console.log(weatherLine);
            let weatherData = weatherLine.split("*");
            let sensorName = weatherData[14];
            //temperature*pressure*humidity*gas*windDirection*windSpeed*windIncrement*precipitation*reserved1*reserved2*reserved3*reserved4 
            //using delimited data instead of JSON to keep things concise and simple
            let weHadData = false;
            let columnCursor = 0;
            parentDiv += "<tr id='sensor" + sensorCursor + "'>";
            if(firstSensorDone) {
              potentialWeatherDisplay += "<td class='sensorname'>" + sensorName + "</td>";
            } else {
              potentialWeatherDisplay += "<td></td>";
            }
            for(let column of columnNames) {
              if(weatherData.length > columnCursor) {
                let value = weatherData[columnCursor];
                let unitName = units[columnCursor];
                if(column == "temperature") {//units are in C but we need F because AMERICA F YEAH!!
                  value = (value * 9/5) + 32;
                }
                if(value != "NULL" && !isNaN(value) && value != "") {
                  potentialWeatherDisplay += "<td class='weatherdata'>" +  parseFloat(value).toFixed(2) + unitName + "</td>";
                  weHadData = true;
                }
              }
              columnCursor++;
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
