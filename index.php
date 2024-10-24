<!doctype html>
<?php 
include("config.php");
include("site_functions.php");
include("device_functions.php");
//ini_set('display_errors', 1);
//ini_set('display_startup_errors', 1);
//error_reporting(E_ALL);
 
if(array_key_exists( "locationId", $_REQUEST)) {
	$locationId = $_REQUEST["locationId"];
} else {
	$locationId = 1;

}
$poser = null;
$poserString = "";
$out = "";
$conn = mysqli_connect($servername, $username, $password, $database);
$user = autoLogin();
$tenantSelector = "";
$scaleConfig =  timeScales();

$content = "";
$action = gvfw("action");
if ($action == "login") {
	$tenantId = gvfa("tenant_id", $_GET);
	$tenantSelector = loginUser($tenantId);
} else if ($action == 'settenant') {
	setTenant(gvfw("encrypted_tenant_id"));
} else if ($action == "logout") {
	logOut();
	header("Location: ?action=login");
	die();
}
if(!$user) {
	if(gvfa("password", $_POST) != "" && $tenantSelector == "") {
		$content .= "<div class='genericformerror'>The credentials you entered have failed.</div>";
	}
    if(!$tenantSelector) {
		$content .= loginForm();
	} else {
		$content .= $tenantSelector;
	}
 
	echo bodyWrap($content, $user, "", null);
	die();
}
 
function multiDevicePicker($tenantId) {
	$devices = getDevices($tenantId);
	$out = "";
	foreach($devices as $device){
		$out .= "<div><input onchange='getWeatherData()' name='specificDevice' type='checkbox' value='" . $device["device_id"] . "'/> ";
		$out .= $device["location_name"];
		$out .= "</div>";
	}
	return $out;
}
?>
<html>

<head>
  <title>Weather Information</title>
  <!--For offline ESP graphs see this tutorial https://circuits4you.com/2018/03/10/esp8266-jquery-and-ajax-web-server/ -->
  <script src = "https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.7.3/Chart.min.js"></script>  
  <script>
  	let scaleConfig = JSON.parse('<?php echo json_encode(timeScales()); ?>');
  </script>
  <script src='tool.js'></script>
  <link rel='stylesheet' href='tool.css?version=1711570359'>
  <link rel="icon" type="image/x-icon" href="./favicon.ico" />

</head>

<body>
<?php
  $out .= topmostNav();
  $out .= "<div class='logo'>Weather Data</div>";
  		
  if($user) {
	$out .= "<div class='outercontent'>";
    if($poser) {
      $poserString = " posing as <span class='poserindication'>" . $poser["email"] . "</span> (<a href='?action=disimpersonate'>unpose</a>)";

    }
    $out .= "<div class='loggedin'>You are logged in as <b>" . $user["email"] . "</b>" .  $poserString . "  on " . $user["name"] . " <div class='basicbutton'><a href=\"?action=logout\">logout</a></div></div>\n";
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

		<div style="text-align:center;"><b>Weather Information Log</b></div>
		<div class="chart-container" position: relative; height:350px; width:100%">
			<canvas id="Chart" width="400" height="700"></canvas>
		</div>
		<div>
		<div style='display:inline-block;vertical-align:top'>
			<?php
				$selectId = "locationDropdown";
				$handler = "getWeatherData()";
				echo "\n<table>\n";
				echo "<tr><td>Time Scale:</td><td>";
				echo genericSelect("scaleDropdown", "scale", defaultFailDown(gvfw("scale"), "day"), $scaleConfig, "onchange", $handler);
				echo "</td></tr>";
				echo "<tr><td>Date/Time Begin:</td><td id='placeforscaledropdown'></td></tr>";
				echo "\n</table>\n";
				?>
				</div>

				<div style='display:inline-block;vertical-align:top'>
				<div class='listtitle'><input type='radio' id='plottype' name='plottype' checked onchange="<?php echo $handler;?>" value='single'>Single-location Plot</div>
				<table id="dataTable">
				<?php 
				//lol, it's easier to specify an object in json and decode it than it is just specify it in PHP

				$thisDataSql = "SELECT location_name as text, device_id as value FROM device WHERE location_name <> '' AND location_name IS NOT NULL AND tenant_id=" . intval($user["tenant_id"]) . " ORDER BY location_name ASC;";
				$result = mysqli_query($conn, $thisDataSql);
				if($result) {
					$selectData = mysqli_fetch_all($result, MYSQLI_ASSOC); 
				}

				//$selectData = json_decode('[{"text":"Outside Cabin","value":1},{"text":"Cabin Downstairs","value":2},{"text":"Cabin Watchdog","value":3}]');
				//var_dump($selectData);
				//echo  json_last_error_msg();
				
				echo "<tr><td>Location:</td><td>" . genericSelect($selectId, "locationId", defaultFailDown(gvfw("location_id"), $locationId), $selectData, "onchange", $handler) . "</td></tr>";
				?>
				</table>
		</div>
		<div style='display:inline-block;vertical-align:top' id='multiplot'>
		<?php
			echo "<div class='listtitle'><input type='radio' id='plottype' name='plottype' onchange='" . $handler . "' value='multi'>Multi-location Plot</div>";
			echo "<div>Weather Column: ";
			$weatherColumns = [["value"=>"temperature", "text"=>"temperature"], ["value"=>"pressure", "text"=>"pressure"], ["value"=>"humidity", "text"=>"humidity"]];
			echo "</div>";
			echo genericSelect("specific_column", "specific_column", defaultFailDown(gvfw("specific_column"), "temperature"), $weatherColumns, "onchange", $handler);
			echo multiDevicePicker($user["tenant_id"]);
		?>
		</div>
		<div style='display:inline-block;vertical-align:top' id='officialweather'>
		</div>
	</div>
<!--</div>-->
</div>
</div>
<script>
let glblChart = null;
//For graphs info, visit: https://www.chartjs.org
let temperatureValues = [];
let humidityValues = [];
let pressureValues = [];
let timeStamp = [];
let locations = [];
let devices = [];

function showGraph(locationId){
	if(glblChart){
		glblChart.destroy();
	}
    let ctx = document.getElementById("Chart").getContext('2d');
	let timeStampLabels = [];
	let chartDataSet = [{
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
            
            ];
	timeStampLabels = timeStamp;
	let graphSubtitle = findObjectByColumn(devices, "device_id", locationId)["location_name"] + " data";
	if(locations.length > 0){
		timeStampLabels = [];
		chartDataSet = [];
		//console.log("what gets graphed", locations);
		for(let key in locations){
			let value = locations[key];
			//console.log("key:", key);
			//console.log(value["values"]);
			chartDataSet.push(
				{
				label: findObjectByColumn(devices, "device_id", key)["location_name"],
					fill: false,  //Try with true
					backgroundColor: 'rgba(' + Math.floor(Math.random() * 255) + ',' + Math.floor(Math.random() * 255)+ ',' +  Math.floor(Math.random() * 255) + ',' +  Math.random() + ')', //Dot marker color
					borderColor: 'rgba(' + Math.floor(Math.random() * 255) + ',' + Math.floor(Math.random() * 255)+ ',' +  Math.floor(Math.random() * 255) + ',' +  Math.random() + ')', //Graph Line Color
					data: value["values"],
					tension: 0.1,
					yAxisID: 'A'
				}
			);
			//console.log(value.timeStamps);
			//if(key == 1) {
				//timeStampLabels = [];
				timeStampLabels.push(...value.timeStamps);
			//}
		}
		//console.log(timeStampLabels);
		graphSubtitle = document.getElementById("specific_column")[document.getElementById("specific_column").selectedIndex].value + " data";

	}
	timeStampLabels.sort();
	//console.log(timeStampLabels);
    let Chart2 = new Chart(ctx, {
        type: 'line',
        data: {
            labels: timeStampLabels,  //Bottom Labeling
            datasets: chartDataSet,
        },
        options: {
 
            hover: {mode: null},
            title: {
                    display: true,
                    text: graphSubtitle
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
            },
			spanGaps: true  // Connects the dots, even if there are gaps (null values)
        }
    });
	//console.log(timeStamp.length);
	//console.log(timeStamp);
	return Chart2;
}

//On Page load show graphs
window.onload = function() {
  console.log(new Date().toLocaleTimeString());
  let locationId = document.getElementById("locationDropdown")[document.getElementById("locationDropdown").selectedIndex].value;
  officialWeather(locationId);
  //showGraph(5,10,4,58);W
};

 

 

//Ajax script to get ADC voltage at every 5 Seconds 
//Read This tutorial https://circuits4you.com/2018/02/04/esp8266-ajax-update-part-of-web-page-without-refreshing/

 
let currentStartDate; //a global that needs to persist through HTTP sessions in the frontend
let justLoaded = true;

function getWeatherData() {
	//console.log("got data");

	const queryParams = new URLSearchParams(window.location.search);
	let locationIdArray = [];
	let scale = queryParams.get('scale');
	let locationId = queryParams.get('location_id');
	let periodAgo = queryParams.get('period_ago');
	let locationIds = queryParams.get('location_ids');
	let plotType = "single";
	let specificColumn = queryParams.get('specific_column');
	for(let radio of document.getElementsByName("plottype")){
		if(radio.checked){
			plotType = radio.value;
		}
	}
	console.log(plotType);
	if(locationIds == null) {
		locationIds = "";
	}
	if(specificColumn == null) {
		specificColumn = "";
	}
	if(!scale){
		scale = "day";
	}
	let locationIdDropdown = document.getElementById('locationDropdown');
	if(!locationId){
		locationId  =locationIdDropdown[locationIdDropdown.selectedIndex].value
	}
	if(!locationId){
		locationId = <?php echo $locationId ?>;
	}
	if(document.getElementById('scaleDropdown')  && !justLoaded){
		scale = document.getElementById('scaleDropdown')[document.getElementById('scaleDropdown').selectedIndex].value;
	}	
	

	let specificColumnSelect = document.getElementById('specific_column');
	if(!justLoaded){
		specificColumn = document.getElementById('specific_column')[document.getElementById('specific_column').selectedIndex].value;
	} else {
		
		if(specificColumnSelect) {
			let index = 0;
			for(let option of specificColumnSelect.options) {
				if (option == specificColumn){
					specificColumnSelect.selectedIndex = index;
				}
				index++;
			}
		}
	}

	let specificDevices = document.getElementsByName('specificDevice');
	if(!justLoaded){
		locationIds = "";
		for(let device of specificDevices){
			if(device.checked){
				locationIds += device.value + ",";
			}
		}
		locationIds = locationIds.slice(0, -1);
	} else {
		let locationIdArray = locationIds.split(",");
		for(let device of specificDevices){
			if(locationIdArray.includes(device.value)){
				device.checked = true;
			}
		}
	}

	//make the startDateDropdown switch to the appropriate item on the new scale:
	let periodAgoDropdown = document.getElementById('startDateDropdown');	

	if(periodAgoDropdown){
		if(!justLoaded){
			periodAgo = periodAgoDropdown[periodAgoDropdown.selectedIndex].value;
		} else {
			periodAgo = 0;
		}
		if(currentStartDate == periodAgoDropdown[periodAgoDropdown.selectedIndex].text){
			thisPeriod = periodAgo;
			periodAgo = false;
			console.log("dates unchanged");
		}
		currentStartDate = periodAgoDropdown[periodAgoDropdown.selectedIndex].text;
	}	
	periodAgo = calculateRevisedTimespanPeriod(scaleConfig, periodAgo, scale, currentStartDate);
	
	let xhttp = new XMLHttpRequest();
	let endpointUrl = "./data.php?scale=" + scale + "&period_ago=" + periodAgo + "&mode=getWeatherData&locationId=" + locationId;
	if(plotType == 'multi'){
		endpointUrl += "&specific_column=" + specificColumn + "&location_ids=" + locationIds;
		locationIdDropdown.disabled = true;
		specificColumnSelect.disabled = false;
		for(let specificDevice of specificDevices) {
			specificDevice.disabled = false;
		}
	} else {
		locationIdDropdown.disabled = false;
		specificColumnSelect.disabled = true;
		for(let specificDevice of specificDevices) {
			specificDevice.disabled = true;
		}
	}
	console.log(endpointUrl);
	xhttp.onreadystatechange = function() {
	    if (this.readyState == 4 && this.status == 200) {
	     //Push the data in array
			temperatureValues = [];
			humidityValues = [];
			pressureValues = [];
			timeStamp = [];
			locations = [];
			if(plotType == "multi"){
				locationIdArray = locationIds.split(',');
				for(let specificLocationId of locationIdArray){
					locations[specificLocationId] = {"values": [], "timeStamps": []};
				}
			}
			let time = new Date().toLocaleTimeString();
			//console.log(this.responseText);
			let dataObject = JSON.parse(this.responseText); 
			//let tbody = document.getElementById("tableBody");
			//tbody.innerHTML = '';
			console.log(dataObject);
			if(dataObject) {
				if(dataObject[0] && dataObject[0]["sql"]){
					console.log(dataObject[0]["sql"], dataObject[0]["error"]);
				} else {
					devices = dataObject["devices"]; //for proper labels in the graph
					//console.log(devices);
					for(let datum of dataObject["records"]) {
						let time = datum["recorded"];
						if(plotType == "multi") {
;							locationId = datum["location_id"];
							let value = datum[specificColumn];
							if(specificColumn == "temperature"){
								value = value * (9/5) + 32;
							}
							if(locations[locationId]) {
								locations[locationId]["values"].push(value);

								for(let specificLocationId of locationIdArray){
									if(specificLocationId != locationId){
										locations[specificLocationId]["values"].push(null);
									}
								}
								locations[locationId]["timeStamps"].push(time);
							}
						} else {
							let temperature = datum["temperature"];
							temperature = temperature * (9/5) + 32;
							//convert temperature to fahrenheitformula
							let pressure = datum["pressure"];
							let humidity = datum["humidity"];
							temperatureValues.push(temperature);
							humidityValues.push(humidity);
							pressureSkewed = pressure;//so we can see some detail in pressure
							if(pressure > 0) {
								pressureValues.push(pressure); 
							}
							timeStamp.push(time)


						}
 

						
					}
				}
			}
			glblChart = showGraph(locationId);  //Update Graphs
			officialWeather(locationId);
 
	    }
		document.getElementsByClassName("outercontent")[0].style.backgroundColor='#ffffff';
		
	  };

  xhttp.open("GET", endpointUrl, true); //Handle getData server on ESP8266
  xhttp.send();
  createTimescalePeriodDropdown(scaleConfig, periodAgo, scale, currentStartDate, 'change', 'getWeatherData()', 'weather_data', locationId);
  
  justLoaded = false;
}

function timeConverter(UNIX_timestamp){
  var a = new Date(UNIX_timestamp * 1000);
  var months = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
  var year = a.getFullYear();
  var month = months[a.getMonth()];
  var date = a.getDate();
  var hour = a.getHours();
  var min = a.getMinutes();
  var sec = a.getSeconds();
  var time = date + ' ' + month + ' ' + year + ' ' + hour + ':' + min + ':' + sec ;
  return time;
}

function ctof(inVal){
	return inVal * (9/5) + 32;
}

function officialWeather(locationId) {
	let xhttp = new XMLHttpRequest();
	let endpointUrl = "./data.php?mode=getOfficialWeatherData&locationId=" + locationId;
	xhttp.onreadystatechange = function() {
	    if (this.readyState == 4 && this.status == 200) {
	     //Push the data in array
			 
			//console.log(this.responseText);
			let dataObject = JSON.parse(this.responseText); 
			let weatherDescription = dataObject["official_weather"]["weather"][0]["description"];
			let temperature = dataObject["official_weather"]["main"]["temp"];
			let temperatureMin = dataObject["official_weather"]["main"]["temp_min"];
			let temperatureMax = dataObject["official_weather"]["main"]["temp_max"];
			let pressure = dataObject["official_weather"]["main"]["pressure"];
			let humidity = dataObject["official_weather"]["main"]["humidity"];
			let wind = dataObject["official_weather"]["wind"]["speed"];
			let gust = dataObject["official_weather"]["wind"]["gust"];
			let sunrise = dataObject["official_weather"]["sys"]["sunrise"];
			let sunset = dataObject["official_weather"]["sys"]["sunset"];
			let weatherdiv = document.getElementById("officialweather");
			let out = "<table>";
			//out += "<tr><td>Location</td><td>" + locationId + "</td></tr>\n";
			out += "<tr><td>Weather</td><td>" + weatherDescription + "</td></tr>\n";
			out += "<tr><td>Temperature</td><td>" + ctof(temperature).toFixed(1); + "</td></tr>\n";
			out += "<tr><td>Extremes</td><td>" + ctof(temperatureMin).toFixed(1); + " to " + ctof(temperatureMax).toFixed(1); + "</td></tr>\n";
			out += "<tr><td>Pressure</td><td>" + pressure.toFixed(1); + "</td></tr>\n";
			out += "<tr><td>Humidity</td><td>" + humidity.toFixed(1); + "</td></tr>\n";
			out += "<tr><td>Wind</td><td>" + wind.toFixed(1); + "</td></tr>\n";
			//out += "<tr><td>Gust</td><td>" + gust.toFixed(1); + "</td></tr>\n"; //i guess they got rid of this one
			out += "<tr><td>Sunrise</td><td>" + timeConverter(sunrise) + "</td></tr>\n";
			out += "<tr><td>Sunset</td><td>" + timeConverter(sunset) + "</td></tr>\n";
			out += "</table>";
			weatherdiv.innerHTML = out;
 
	    }
	  };
  xhttp.open("GET", endpointUrl, true); //Handle getData server on ESP8266
  xhttp.send();

}

getWeatherData();
</script>
</body>

</html>

 