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

.weatherdata {
  display:inline-block;
  text-align: center;
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
   <div id='temperature' class='weatherdata'></div>
   <div id='pressure' class='weatherdata'></div>
   <div id='humidity' class='weatherdata'></div>
  
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
        let weatherData = firstLine.split("*");
        temperature = weatherData[0];
        pressure = weatherData[1];
        humidity = weatherData[2];
        if(temperature != "NULL" && !isNaN(temperature)) {
          document.getElementById("temperature").innerHTML = (parseFloat(temperature) * 1.8 + 32).toFixed(2) + "&deg; F"; 
        }
        if(pressure != "NULL"  && !isNaN(pressure)) {
          document.getElementById("pressure").innerHTML = parseFloat(pressure).toFixed(2) + "mm Hg";
        }
        if(humidity != "NULL" && !isNaN(humidity)) {
          document.getElementById("humidity").innerHTML = parseFloat(humidity).toFixed(2) + "% rel";
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
