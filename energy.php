<?php
include("other_page_login.php");
//we only need this so we have SOME deviceId for the backend. but we should change how that gets auth'ed
if(array_key_exists( "deviceId", $_REQUEST)) {
	$deviceId = $_REQUEST["deviceId"];
} else {
	$deviceId = 0; //ignored, but must be a number
}

?>
<html>
<head>
  <title>Inverter Information</title>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.7.0/chart.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-date-fns@2.0.0/dist/chartjs-adapter-date-fns.bundle.min.js"></script>
  <script>
  let deviceId = <?php echo $deviceId ?>;
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
			$poserString = " posing as <span class='poserindication'>" . userDisplayText($poser) . "</span> (<a href='?action=disimpersonate'>unpose</a>)";

		}
		$out .= "<div class='loggedin'>You are logged in as <b>" . userDisplayText($user) . "</b>" .  $poserString . "  on " . $user["name"] . " <div class='basicbutton'><a href=\"?action=logout\">logout</a></div></div>\n";
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
			$handler = "getInverterData(0)";
			echo "<tr><td>Time Scale:</td><td>";
			echo genericSelect("scaleDropdown", "scale",  defaultFailDown(gvfw("scale"), "day"), $scaleConfig, "onchange", $handler);
			echo "</td></tr>";
			echo "<tr><td>Date/Time Begin:</td><td id='placeforscaledropdown'></td></tr>";
			echo "<tr><td>Use Absolute Timespan Cusps</td><td><input type='checkbox' value='absolute_timespan_cusps' id='atc_id' onchange='" . $handler . "'/></td></tr>";
			?>
			</table>
		</div>
	</div>
<!--</div>-->
</div>
<script src='energy.js'></script>
</body>

</html>
 

 