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
//$scaleConfig = '[{"text":"ultra-fine","value":"ultra-fine", "period_size": 1, "period_scale": "hour"},{"text":"fine","value":"fine", "period_size": 1, "period_scale": "day"},{"text":"hourly","value":"hour", "period_size": 7, "period_scale": "day"}, {"text":"daily","value":"day", "period_size": 1, "period_scale": "year"}]';

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

 
 
?>
<html>
<head>
  <title>Inverter Information</title>
  <!--For offline ESP graphs see this tutorial https://circuits4you.com/2018/03/10/esp8266-jquery-and-ajax-web-server/ -->
  <!--<script src = "https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.7.3/Chart.min.js"></script>--> 
  <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.7.0/chart.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-date-fns@2.0.0/dist/chartjs-adapter-date-fns.bundle.min.js"></script>
  <script>
	let scaleConfig = JSON.parse('<?php echo json_encode(timeScales()); ?>');
	window.timezone ='<?php echo $timezone ?>';
  </script>
  <link rel='stylesheet' href='tool.css?version=1711570359'>
  <script src='tool.js'></script>
  <link rel="icon" type="image/x-icon" href="./favicon.ico" />
</head>

<body>
<?php
	$out .= topmostNav();
	$out .= "<div class='logo'>Inverter Data</div>\n";
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

    <div style="text-align:center;"><b><span id='greatestTime'></span></b></div>
		<div class="chart-container" style="width: 100%; height: 70vh;">
			<canvas id="Chart"></canvas>
		</div>
		<div>
			<table id="dataTable">
			<?php 
			//lol, it's easier to specify an object in json and decode it than it is just specify it in PHP
			//$selectData = json_decode('[{"text":"Outside Cabin","value":1},{"text":"Cabin Downstairs","value":2},{"text":"Cabin Watchdog","value":3}]');
			//var_dump($selectData);
			//echo  json_last_error_msg();
			$handler = "getInverterData(0)";

			//$scaleConfig = json_decode('[{"text":"ultra-fine","value":"ultra-fine"},{"text":"fine","value":"fine"},{"text":"hourly","value":"hour"}, {"text":"daily","value":"day"}]', true);
			echo "<tr><td>Time Scale:</td><td>";
			echo genericSelect("scaleDropdown", "scale",  defaultFailDown(gvfw("scale"), "day"), $scaleConfig, "onchange", $handler);
			echo "</td></tr>";
			echo "<tr><td>Date/Time Begin:</td><td id='placeforscaledropdown'></td></tr>";
			echo "<tr><td>Use Absolute Timespan Cusps</td><td><input type='checkbox' value='absolute_timespan_cusps' id='atc_id' onchange='" . $handler . "'/></td></tr>";
			//echo "<script>createTimescalePeriodDropdown(scaleConfig, 31, 'fine', 'change', 'getInverterData()');</script>";
			?>
			</table>
		</div>
	</div>
<!--</div>-->
</div>
<script>
let glblChart = null;
let graphDataObject = {};
let columnsWeCareAbout = ["solar_power","load_power","battery_power","battery_percentage", "weather"]; //these are the inverter columns to be graphed from inverter_log. if you have more, you can include them
let yearsIntoThePastWeCareAbout = [0,1,2,3];
//For graphs info, visit: https://www.chartjs.org
let timeStamp = [];
let weatherColorMap = [];
let segmentRects = [];

resetGraphData();

function resetGraphData(){
	graphDataObject = {};
	pastYearsViewed = [];
	for (let year of yearsIntoThePastWeCareAbout) { 
		for (let column of columnsWeCareAbout) {
			if (!graphDataObject[year]) {
				graphDataObject[year] = {};
			}
			if (!graphDataObject[year][column]) {
				graphDataObject[year][column] = [];
			}
		}
	}
}

function stringToColor(str) {
  let hash = 0;
  for (let i = 0; i < str.length; i++) {
    // Simple hash function: shift and sum character codes
    hash = str.charCodeAt(i) + ((hash << 5) - hash);
    hash = hash & hash; // Convert to 32bit integer
  }

  // Generate HSL values
  const hue = Math.abs(hash) % 360; // Hue between 0 and 359
  const saturation = 70; // Fixed saturation
  const lightness = 60;  // Fixed lightness
  const alpha = 0.3; // Adjust this value for desired transparency
  return `hsl(${hue}, ${saturation}%, ${lightness}%, ${alpha})`;
}

const weatherPlugin = {
  id: 'weatherSegments',
  beforeDatasetsDraw(chart, args, options) {
    const { ctx, chartArea: { bottom } } = chart;
    const xScale = chart.scales['x']; // Access the x-axis scale by its ID
    const segmentHeight = 20;
    const segments = options.segments || [];

    if (!xScale) {
      console.warn('X-axis scale not found');
      return;
    }

    segments.forEach(segment => {
		if (!weatherColorMap[segment.weather]) {
			weatherColorMap[segment.weather] = stringToColor(segment.weather);
		}
		const fillColor = weatherColorMap[segment.weather];
		const xStart = xScale.getPixelForValue(new Date(segment.start).getTime());
		const xEnd = xScale.getPixelForValue(new Date(segment.end).getTime());
		const width = xEnd - xStart;


		ctx.fillStyle = fillColor;
		ctx.fillRect(xStart, bottom, width, segmentHeight);

 
		// Store segment boundaries for interaction
		segmentRects.push({
			x: xStart,
			y: bottom,
			width: width,
			height: segmentHeight,
			weather: segment.weather
		});

    });
  }
};


function showGraph(yearsAgo){
	let colorSeries = ["#ff0000", "#00ff00", "#0000ff", "#009999", "#3300ff", "#ff0033", "#ff3300", "33ff00", "#0033ff", "#6600cc", "#ff0066", "#cc6600", "66cc00", "#0066cc"];
	//console.log(timeStamp);
	if(glblChart){
		glblChart.destroy();
	}
	if(!yearsAgo){
		yearsAgo = 0;
	}
    let ctx = document.getElementById("Chart").getContext('2d');
	let columnCount = 0;
	let chartDataSet = [];
	for (let column of columnsWeCareAbout){
		let yAxisId = "A";
		if(column == "battery_percentage"){ //if you have a percentage instead of a kilowatt value, this is the scale you want
			yAxisId = "B";
		}
		//console.log(graphDataObject[0][column]);
		chartDataSet.push(
			{
				label: column,
				fill: false,  //Try with true
				backgroundColor: colorSeries[columnCount],
				borderColor: colorSeries[columnCount],
				data: graphDataObject[0][column],
				yAxisID: yAxisId
			}
		);
		columnCount++;
	}

	const energyChart = new Chart(ctx, {
		type: 'line',
		data: {
			labels: timeStamp,
			datasets: chartDataSet
		},
		options: {
			responsive: true,
			maintainAspectRatio: false,
			plugins: {
			weatherSegments: {
				segments:  [  { start: '2025-05-09T12:00:00Z', end: '2025-05-09T18:00:00Z', weather: 'overcast clouds' },
				{ start: '2025-05-09T18:00:00Z', end: '2025-05-09T23:59:59Z', weather: 'clear sky' }]
			},
			title: {
				display: true,
				text: 'Inverter data'
			}
			},
			elements: {
			point: {
				radius: 0
			},
			line: {
				 tension: 0.2 // Smoothening (Curved) of data lines
			}
			},
			scales: {
			x: {
				type: 'time',
				title: {
				display: true,
				text: 'Time'
				}
			},
			A: {
				type: 'linear',
				position: 'left',
				title: {
				display: true,
				text: 'Power (Watts)'
				}
			},
			B: {
				type: 'linear',
				position: 'right',
				title: {
				display: true,
				text: 'Percentage'
				},
				grid: {
				drawOnChartArea: false
				}
			}
			}
		},
		plugins: [weatherPlugin]
	});
	console.log(energyChart.options.plugins.weatherSegments.segments);
	return energyChart;
}

const canvas = document.getElementById("Chart");


canvas.addEventListener("mousemove", (event) => {
  const rect = canvas.getBoundingClientRect();
  const mouseX = event.clientX - rect.left;
  const mouseY = event.clientY - rect.top;

  // Check if mouse is over any segment
  const hoveredSegment = segmentRects.find(seg =>
    mouseX >= seg.x &&
    mouseX <= seg.x + seg.width &&
    mouseY >= seg.y &&
    mouseY <= seg.y + seg.height
  );

  if (hoveredSegment) {
    // Display tooltip or perform desired action
    showTooltip(event.clientX, event.clientY, hoveredSegment.weather);
  } else {
    hideTooltip();
  }
});

const tooltip = document.createElement("div");
tooltip.style.position = "absolute";
tooltip.style.background = "rgba(0, 0, 0, 0.7)";
tooltip.style.color = "#fff";
tooltip.style.padding = "5px 10px";
tooltip.style.borderRadius = "4px";
tooltip.style.pointerEvents = "none";
tooltip.style.transition = "opacity 0.2s";
tooltip.style.opacity = 0;
document.body.appendChild(tooltip);

function showTooltip(x, y, text) {
  tooltip.textContent = text;
  tooltip.style.left = `${x + 10}px`;
  tooltip.style.top = `${y + 10}px`;
  tooltip.style.opacity = 1;
}

function hideTooltip() {
  tooltip.style.opacity = 0;
}

//On Page load show graphs
window.onload = function() {
	console.log(new Date().toLocaleTimeString());
	Chart.register(weatherPlugin);
	
};

let currentStartDate; //a global that needs to persist through HTTP sessions in the frontend
let justLoaded = true;

function getInverterData(yearsAgo) {
	const queryParams = new URLSearchParams(window.location.search);
	let scale = queryParams.get('scale');
	let absoluteTimespanCusps = queryParams.get('absolute_timespan_cusps');
	let atcCheckbox = document.getElementById("atc_id");
	let url = new URL(window.location.href);
	let greatestTime = "2000-01-01 00:00:00";

	if(justLoaded){
		if(absoluteTimespanCusps == 1){
			atcCheckbox.checked = true;
		}
	}
	if(atcCheckbox.checked) {
		absoluteTimespanCusps = 1;
	} else {
		absoluteTimespanCusps = 0;
	}
	url.searchParams.set("absolute_timespan_cusps", absoluteTimespanCusps);
	if(!scale){
		scale = "day";
	}
	let periodAgo = queryParams.get('period_ago');
 
	//console.log("got data");

	if(document.getElementById('scaleDropdown') && !justLoaded){
		scale = document.getElementById('scaleDropdown')[document.getElementById('scaleDropdown').selectedIndex].value;
	}
	url.searchParams.set("scale", scale);
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
		}
		currentStartDate = periodAgoDropdown[periodAgoDropdown.selectedIndex].text;
	}	
	periodAgo = calculateRevisedTimespanPeriod(scaleConfig, periodAgo, scale, currentStartDate);
	url.searchParams.set("period_ago", periodAgo);
	resetGraphData();
	segmentRects = [];
	history.pushState({}, "", url);
	let xhttp = new XMLHttpRequest();
	let endpointUrl = "./data.php?scale=" + scale + "&period_ago=" + periodAgo + "&mode=getInverterData&absolute_timespan_cusps=" + absoluteTimespanCusps;
	console.log(endpointUrl);

	xhttp.onreadystatechange = function() {
		let weatherSegmentsLocal = [];
		let datumCount = 0;
		let lastWeather = "";
		let lastWeatherTime = "";
	    if (this.readyState == 4 && this.status == 200) {
			timeStamp = [];
			let time = new Date().toLocaleTimeString();
			let dataObject = JSON.parse(this.responseText); 
			if(dataObject && dataObject[0]) {
				if(dataObject[0]["sql"]){
					console.log(dataObject[0]["sql"], dataObject[0]["error"]);
				} else {
					
					
					for(let datum of dataObject) {
						datumCount++;
            			//also show: weatherSegments 
						let time = datum["recorded"];
						for (let column of columnsWeCareAbout){
								let value = datum[column];
								if(column != "weather") {
									graphDataObject[yearsAgo][column].push(parseInt(value)); //parseInt is important because smoothArray was thinking the values might be strings
								} else {
									if(lastWeather != value) {
										if(lastWeather == "") { //we're at the very beginning
										} else {
											weatherSegmentsLocal.push({"start": lastWeatherTime, "end": time, "weather": lastWeather});
										}
										lastWeather = value;
										lastWeatherTime = time;
									}
									if (datumCount == dataObject.length) { //get the last one too
										weatherSegmentsLocal.push({"start": lastWeatherTime, "end": time, "weather": lastWeather});
									}
									
								}
									
							}
							timeStamp.push(time);
							if(time> greatestTime) {
								greatestTime = time;
							}
						}
						
					}
			} else {
				console.log("No data was found.");
			}
			console.log(weatherSegmentsLocal);
			if(scale == "three-hour"  || scale == "day"){
				let batteryPercents =  graphDataObject[yearsAgo]["battery_percentage"];
				graphDataObject[yearsAgo]["battery_percentage"] = smoothArray(batteryPercents, 19, 1);
			}
			glblChart = showGraph();  //Update Graphs
			glblChart.options.plugins.weatherSegments.segments = weatherSegmentsLocal;
			glblChart.update();
			document.getElementById('greatestTime').innerHTML = " Latest Data: " + timeAgo(greatestTime);
	    }
		document.getElementsByClassName("outercontent")[0].style.backgroundColor='#ffffff';
		justLoaded = false;
	};
	  

  xhttp.open("GET", endpointUrl, true);  
  xhttp.send();
  createTimescalePeriodDropdown(scaleConfig, periodAgo, scale, currentStartDate, 'change', 'getInverterData(0)', 'inverter_log', '');
}
 
getInverterData(0);
</script>
</body>

</html>
 

 