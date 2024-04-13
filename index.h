const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<title>circuits4you.com</title>
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

.lower{
  background:#00A8A9;
  border-radius:0 0 5px 5px;
  font-family:'Advent Pro';
  font-weight:200;
  height:130px;
  width:100%;
}

a{
 text-align: center;
 text-decoration: none;
 color: white;
 font-size: 15px;
 font-weight: 500;
}
</style>
<body>

<div class="widget"> 
  <div style="text-align: center;color:white">Pins on the Remote Controller</div>
  <div class="lower" id="lower">    
  </div>
</div>

<script>
setInterval(showPinValues, 7000);

function updateLocalValues(id, value) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
    }
    console.log(value);
    var onValue = "0";
    if(value) {
      onValue = "1";
    }
    xhttp.open("GET", "writeLocalData?id=" + id + "&on=" + onValue, true);
    xhttp.send();
}
    
function showPinValues(){
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        var txt = this.responseText;
        var arr = JSON.parse(txt);  
        if(arr.length> 0) {
         document.getElementById("lower").innerHTML = "";
        }
        let pinCursor = 0;
        //console.log(arr);
        for(let obj of arr) {
          //console.log(obj);
          var checked = "";
          var i2cString = "";
          var id = obj["id"];
          var pin = id;
          var friendlyName = obj["name"];
          if(id.indexOf(".")>-1) {
            var pinInfo = id.split(".");
            var i2c = pinInfo[0];
            pin = pinInfo[1];
            i2cString = " via I2C: " + i2c;
          }
          if(obj["value"] == '1') {
            checked = "checked";
          }
          document.getElementById("lower").innerHTML  +=  " <input onchange='updateLocalValues(this.name, this.checked)' " + checked + " type='checkbox' name=\"" + id +  "\" value='1' > <b>" + friendlyName +  "</b>, pin " + pin + " " + i2cString + "<br/>";
          pinCursor++;
        }
      }  
    };
   xhttp.open("GET", "readLocalData", true); //Handle readADC server on ESP8266
   xhttp.send();
}
</script>
</body>
</html>
)=====";
