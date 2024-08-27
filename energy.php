<!doctype html>
<?php 
include("config.php");
include("site_functions.php");
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
  <script src = "https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.7.3/Chart.min.js"></script>  
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

    <div style="text-align:center;"><b>Inverter Information Log</b></div>
		<div class="chart-container" position: relative; height:350px; width:100%">
			<canvas id="Chart" width="400" height="700"></canvas>
		</div>
		<div>
			<table id="dataTable">
			<?php 
			//lol, it's easier to specify an object in json and decode it than it is just specify it in PHP

			

			//$selectData = json_decode('[{"text":"Outside Cabin","value":1},{"text":"Cabin Downstairs","value":2},{"text":"Cabin Watchdog","value":3}]');
			//var_dump($selectData);
			//echo  json_last_error_msg();
			
			
			$handler = "getInverterData()";

			$scaleData = json_decode('[{"text":"ultra-fine","value":"ultra-fine"},{"text":"fine","value":"fine"},{"text":"hourly","value":"hour"}, {"text":"daily","value":"day"}]', true);
			echo "<tr><td>Time Scale:</td><td>" . genericSelect("scaleDropdown", "scale", "fine", $scaleData, "onchange", $handler) . "</td></tr>";
			?>
			</table>
		</div>
	</div>
<!--</div>-->
</div>
<script>
let glblChart = null;
//For graphs info, visit: https://www.chartjs.org
let panelValues = [];
let loadValues = [];
let batteryValues = [];
let batteryPercents = [];
let timeStamp = [];

function showGraph(locationId){
	//console.log(timeStamp);
	if(glblChart){
		glblChart.destroy();
	}
    let ctx = document.getElementById("Chart").getContext('2d');
 
    let Chart2 = new Chart(ctx, {
        type: 'line',
        data: {
            labels: timeStamp,  //Bottom Labeling
            datasets: [{
                label: "Panel Power",
                fill: false,  //Try with true
                backgroundColor: 'rgba( 243, 156, 18 , 1)', //Dot marker color
                borderColor: 'rgba( 243, 156, 18 , 1)', //Graph Line Color
                data: panelValues,
				yAxisID: 'A'
            },
            {
                label: "Load Power",
                fill: false,  //Try with true
                backgroundColor: 'rgba( 156, 243, 18 , 1)', //Dot marker color
                borderColor: 'rgba( 156, 243, 18 , 1)', //Graph Line Color
                data: loadValues,
				yAxisID: 'A'
            },
            {
            label: "Battery Power",
                fill: false,  //Try with true
                backgroundColor: 'rgba( 18, 243, 156 , 1)', //Dot marker color
                borderColor: 'rgba( 1, 243, 156 , 1)', //Graph Line Color
                data: batteryValues,
				yAxisID: 'A'
            },
            {
            label: "Battery Percentage",
                fill: false,  //Try with true
                backgroundColor: 'rgba( 111, 111, 156 , 1)', //Dot marker color
                borderColor: 'rgba( 243, 1, 156 , 1)', //Graph Line Color
                data: batteryPercents,
				//steppedLine: steppedLine,
				//cubicInterpolationMode: 'monotone',
				tension: 0,
				yAxisID: 'B'
            },
            ],
        },
        options: {
			
            hover: {mode: null},
            title: {
                    display: true,
                    text: "Inverter data"
                },
            maintainAspectRatio: false,
            elements: {
				point:{
                        radius: 0
                    },
				line: {
						//tension: 0.2 //Smoothening (Curved) of data lines
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

//getInverterData("<?php echo gvfw("locationId")?>");
//setInterval(function() {
  // Call a function repetatively with 5 Second interval
  //getInverterData(locationId)
  //;}, 5000)
  //; //50000mSeconds update rate
 
function getInverterData() {
	//console.log("got data");
	let scale = document.getElementById('scaleDropdown')[document.getElementById('scaleDropdown').selectedIndex].value;
	let xhttp = new XMLHttpRequest();
	let endpointUrl = "./data.php?scale=" + scale + "&mode=getInverterData";
	xhttp.onreadystatechange = function() {
	    if (this.readyState == 4 && this.status == 200) {
	     //Push the data in array
			panelValues = [];
			loadValues = [];
			batteryValues = [];
			batteryPercents = [];
			timeStamp = [];
			let time = new Date().toLocaleTimeString();
			//console.log(this.responseText);
		
			let dataObject = JSON.parse(this.responseText); 
			//let tbody = document.getElementById("tableBody");
			//tbody.innerHTML = '';
			for(let datum of dataObject[0]) {
				//console.log(datum);
				//console.log("!");
				let time = datum["recorded"];
				let panel = datum["solar_power"];
	 
 
				let load = datum["load_power"];
				let battery = datum["battery_power"];
				let batteryPercent = datum["battery_percentage"];
				panelValues.push(panel);
				loadValues.push(load);
 
				batteryValues.push(battery);
 
				batteryPercents.push(parseInt(batteryPercent));
 
				timeStamp.push(time);
			}
			batteryPercents = smoothArray(batteryPercents, 19, 1); //smooth out the battery percentages, which are integers and too jagged
			//console.log(batteryPercents);
			glblChart = showGraph();  //Update Graphs
	    }
		document.getElementsByClassName("outercontent")[0].style.backgroundColor='#ffffff';
	  };
  xhttp.open("GET", endpointUrl, true); //Handle getData server on ESP8266
  xhttp.send();
}

getInverterData(<?php echo $locationId?>);
</script>
</body>

</html>

 