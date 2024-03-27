<!doctype html>
<?php 
include("config.php");
include("site_functions.php");
//ini_set('display_errors', 1);
//ini_set('display_startup_errors', 1);
//error_reporting(E_ALL);

function genericSelect($id, $name, $defaultValue, $data, $event = "", $handler= "") {
	$out = "";
	$out .= "<select name='" . $name. "' id='" . $id . "' " . $event . "=\"" . $handler . "\">\n";
	foreach($data as $datum) {
		$value = $datum["value"];
		$text = $datum["text"];
		$selected = "";
		if($defaultValue == $value) {
			$selected = " selected='true'";
		}
		$out.= "<option " . $selected . " value=\"" . $value . "\">";
		$out.= $text;
		$out.= "</option>";
	}
	$out.= "</select>";
	return $out;
}
if(array_key_exists( "locationId", $_REQUEST)) {
	$locationId = $_REQUEST["locationId"];
} else {
	$locationId = 1;

}
$poser = null;
$poserString = "";
$out = "";
$conn = mysqli_connect($servername, $username, $password, $database);
$user = logIn();
$content = "";
$action = gvfw("action");
if ($action == "login") {
	loginUser();
} else if ($action == "logout") {
	logOut();
	header("Location: ?action=login");
	die();
}
if(!$user) {
	if(gvfa("password", $_POST) != "") {
		$content .= "<div class='genericformerror'>The credentials you entered have failed.</div>";
	}
	$content .= loginForm();
	echo bodyWrap($content, $user, "", null);
	die();
}
 
?>
<html>

<head>
  <title>Weather Information</title>
  <!--For offline ESP graphs see this tutorial https://circuits4you.com/2018/03/10/esp8266-jquery-and-ajax-web-server/ -->
  <script src = "https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.7.3/Chart.min.js"></script>  
  <link rel='stylesheet' href='tool.css?version=1711570359'>
</head>

<body>
<?php
  if($user) {
    if($poser) {
      $poserString = " posing as <span class='poserindication'>" . $poser["email"] . "</span> (<a href='?action=disimpersonate'>unpose</a>)";

    }
    $out .= "<div class='loggedin'>You are logged in as <b>" . $user["email"] . "</b>" .  $poserString . " <div class='basicbutton'><a href=\"?action=logout\">logout</a></div></div>\n";
	}
	else
	{
    //$out .= "<div class='loggedin'>You are logged out.  </div>\n";
	} 
	$out .= "<div>\n";
    $out .= "<div class='documentdescription'>";
	echo $out; 
  /*
  if($documentId){
    $document = getDocumentFromDocumentId($documentId);
    $out .= "Current xxxx: <em>" . $document["name"] . "</em> (" . $document["file_name"] . ")";
  } else {
    $out .= "No xxxx selected\n";

  }
  */
  $out .= "</div>";
  ?>

    <div style="text-align:center;"><b>Weather Information Log</b></div>
    <div class="chart-container" position: relative; height:350px; width:100%">
        <canvas id="Chart" width="400" height="700"></canvas>
    </div>
<div>
<table id="dataTable">
<?php 
//lol, it's easier to specify an object in json and decode it than it is just specify it in PHP

$thisDataSql = "SELECT location_name as text, device_id as value FROM device WHERE user_id=" . intval($user["user_id"]) . " ORDER BY location_name ASC;";
$result = mysqli_query($conn, $thisDataSql);
if($result) {
  $selectData = mysqli_fetch_all($result, MYSQLI_ASSOC); 
}

//$selectData = json_decode('[{"text":"Outside Cabin","value":1},{"text":"Cabin Downstairs","value":2},{"text":"Cabin Watchdog","value":3}]');
//var_dump($selectData);
//echo  json_last_error_msg();
$selectId = "locationDropdown";
$handler = "getData(document.getElementById('" . $selectId . "')[document.getElementById('" . $selectId  . "').selectedIndex].value)";

echo "<tr><td>Location:</td><td>" . genericSelect($selectId, "locationId", $locationId, $selectData, "onchange", $handler) . "</td></tr>";

$handler = "getData(document.getElementById('" . $selectId . "')[document.getElementById('" . $selectId  . "').selectedIndex].value)";

$scaleData = json_decode('[{"text":"detailed","value":"fine"},{"text":"hourly","value":"hour"}, {"text":"daily","value":"day"}]', true);
echo "<tr><td>Time Scale:</td><td>" . genericSelect("scaleDropdown", "scale", "fine", $scaleData, "onchange", $handler) . "</td></tr>";
?>
</table>
</div>
<br>
<br>  

<script>
let glblChart = null;
//For graphs info, visit: https://www.chartjs.org
let temperatureValues = [];
let humidityValues = [];
let pressureValues = [];
let timeStamp = [];

function showGraph(locationId){
	if(glblChart){
		glblChart.destroy();
	}
    let ctx = document.getElementById("Chart").getContext('2d');
 
    let Chart2 = new Chart(ctx, {
        type: 'line',
        data: {
            labels: timeStamp,  //Bottom Labeling
            datasets: [{
                label: "Temperature",
                fill: false,  //Try with true
                backgroundColor: 'rgba( 243, 156, 18 , 1)', //Dot marker color
                borderColor: 'rgba( 243, 156, 18 , 1)', //Graph Line Color
                data: temperatureValues,
				yAxisID: 'A'
            },
            {
                label: "Humidity",
                fill: false,  //Try with true
                backgroundColor: 'rgba( 156, 243, 18 , 1)', //Dot marker color
                borderColor: 'rgba( 156, 243, 18 , 1)', //Graph Line Color
                data: humidityValues,
				yAxisID: 'A'
            },
            {
            label: "Pressure",
                fill: false,  //Try with true
                backgroundColor: 'rgba( 18, 243, 156 , 1)', //Dot marker color
                borderColor: 'rgba( 1, 243, 156 , 1)', //Graph Line Color
                data: pressureValues,
				yAxisID: 'B'
            },
            
            ],
        },
        options: {
 
            hover: {mode: null},
            title: {
                    display: true,
                    text: "Probe data"
                },
            maintainAspectRatio: false,
            elements: {
            line: {
                    tension: 0.5 //Smoothening (Curved) of data lines
                }
            },
            scales: {
			  yAxes: [
			  	{
			        id: 'A',
			        type: 'linear',
			        position: 'left'
			      }, 
				  {
			        id: 'B',
			        type: 'linear',
			        position: 'right'
			 
	            }
				]
            }
        }
    });
	return Chart2;
}

//On Page load show graphs
window.onload = function() {
  console.log(new Date().toLocaleTimeString());
  //showGraph(5,10,4,58);
};

//Ajax script to get ADC voltage at every 5 Seconds 
//Read This tutorial https://circuits4you.com/2018/02/04/esp8266-ajax-update-part-of-web-page-without-refreshing/

getData("<?php echo gvfw("locationId")?>");
//setInterval(function() {
  // Call a function repetatively with 5 Second interval
  //getData(locationId)
  //;}, 5000)
  //; //50000mSeconds update rate
 
function getData(locationId) {
	//console.log("got data");
	let scale = document.getElementById('scaleDropdown')[document.getElementById('scaleDropdown').selectedIndex].value;
	let xhttp = new XMLHttpRequest();
	let endpointUrl = "http://randomsprocket.com/weather/data.php?storagePassword=<?php echo $user['storage_password'];?>&scale=" + scale + "&mode=getData&locationId=" + locationId;
	xhttp.onreadystatechange = function() {
	    if (this.readyState == 4 && this.status == 200) {
	     //Push the data in array
			temperatureValues = [];
			humidityValues = [];
			pressureValues = [];
			timeStamp = [];
			let time = new Date().toLocaleTimeString();
			//console.log(this.responseText);
			let dataObject = JSON.parse(this.responseText); 
			//let tbody = document.getElementById("tableBody");
			//tbody.innerHTML = '';
			
			for(let datum of dataObject) {
				//console.log(datum);
				let time = datum[2];
				let temperature = datum[3];
				temperature = temperature * (9/5) + 32;
				//convert temperature to fahrenheitformula
				let pressure = datum[4];
				let humidity = datum[5];
				temperatureValues.push(temperature);
				humidityValues.push(humidity);
				pressureSkewed = pressure;//so we can see some detail in pressure
				if(pressure > 0) {
					pressureValues.push(pressure); 
				}
				timeStamp.push(time);
				
 
			}
 
			glblChart = showGraph(locationId);  //Update Graphs
	
	    }
	  };
  xhttp.open("GET", endpointUrl, true); //Handle getData server on ESP8266
  xhttp.send();
}

getData(<?php echo $locationId?>);
</script>
</body>

</html>

 
