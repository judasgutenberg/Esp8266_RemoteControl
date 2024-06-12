const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<title>local data</title>
</head>
<style>
@import url(https://fonts.googleapis.com/css?family=Montserrat);
@import url(https://fonts.googleapis.com/css?family=Advent+Pro:400,200);
*{margin: 0;padding: 0;}

body{
  background:#544947;
  font-family:Montserrat,Arial,sans-serif;
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
  background:#00A8A9;
  border-radius:0 0 5px 5px;
  font-family:'Advent Pro';
  font-weight:200;
  height:130px;
  width:100%;
}

.sensorcluster {
  display:block;
  text-align: center;
  margin-bottom:10px;
  background-color:black
}

.weatherdata {
  display:inline-block;
  text-align: center;
  padding-left:10px;
  color:white
}
 
#deviceName {
  text-align: center;
  color:white;
  font-size: 14px;
}
 
</style>
<body>

<div class="widget"> 
  <div id='deviceName'>Your Device</div>
  <div class="devices" id="devices">    
  </div>
  <div class="sensors" id="sensors">    
  </div>
   
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
        let sensorDiv = document.getElementById("sensors");
        sensorDiv.innerHTML = "";
        let firstSensorDone = false;
        for(weatherLine of weatherLines){  
          if(weatherLine.indexOf("*") > -1) {
            console.log(weatherLine);
            let weatherData = weatherLine.split("*");
            //temperatureValue*pressureValue*humidityValue*gasValue*sensorType*deviceFeatureId"*sensorName; //using delimited data instead of JSON to keep things simple
            let temperature = weatherData[0];
            let pressure = weatherData[1];
            let humidity = weatherData[2];
            let sensorName = weatherData[6];
            sensorDiv.innerHTML += "<div class='sensorcluster'>";
            if(firstSensorDone) {
              sensorDiv.innerHTML += "<div class='weatherdata'><b>" + sensorName + "</b></div>";
            }
            if(temperature != "NULL" && !isNaN(temperature)) {
              sensorDiv.innerHTML += "<div class='weatherdata'>" + (parseFloat(temperature) * 1.8 + 32).toFixed(2) + "&deg; F" + "</div>";
              //document.getElementById("temperature").innerHTML = (parseFloat(temperature) * 1.8 + 32).toFixed(2) + "&deg; F"; 
            }
            if(pressure != "NULL"  && !isNaN(pressure)) {
              sensorDiv.innerHTML += "<div class='weatherdata'>" +  parseFloat(pressure).toFixed(2) + "mm Hg" + "</div>";
              //document.getElementById("pressure").innerHTML = parseFloat(pressure).toFixed(2) + "mm Hg";
            }
            if(humidity != "NULL" && !isNaN(humidity)) {
              sensorDiv.innerHTML += "<div class='weatherdata'>" +  parseFloat(humidity).toFixed(2) + "% rel" + "</div>";
              //document.getElementById("humidity").innerHTML = parseFloat(humidity).toFixed(2) + "% rel";
            }
            sensorDiv.innerHTML += "</div>";
            firstSensorDone = true;
          }
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
        document.getElementById("deviceName").innerHTML = deviceName;
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
      updateWeatherDisplay(); 
    };
   xhttp.open("GET", "readLocalData", true); //Handle readADC server on ESP8266
   xhttp.send();
}
</script>
</body>
</html>
)=====";
