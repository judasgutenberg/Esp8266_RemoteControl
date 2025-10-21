<?php
include("other_page_login.php");
if($deviceId == ""){
  $deviceId = 21; //hard coded for me, probably not what you want
}
?>
<html>
<head>
  <title>Messages</title>
  <link rel='stylesheet' href='tool.css?version=<?php echo $version?>'>
  <script src='tool.js?version=<?php echo $version?>'></script>
  <script src='message.js?version=<?php echo $version?>'></script>
  <script src='tablesort.js?version=<?php echo $version?>'></script>
  <script src='tinycolor.js?version=<?php echo $version?>'></script>
  <link rel="icon" type="image/x-icon" href="./favicon.ico" />
 
</head>
<body>
<?php
	$out .= topmostNav();
	$out .= "<div class='logo'>Map</div>\n";
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
	  $out .= "<div class='waitingouter' id='waitingouter'><div class='waiting' id='waiting'><img width='200' height='200' src='./images/signs.gif'></div><div id='waitingmessage' class='waitingmessage'>Waiting...</div></div>\n";
  $out .= "<div>\n";
  $out .= "<div class='documentdescription'>";
  $out .= "</div>";
 
  echo $out; 
 
  ?>
  
    <div style="text-align:center;"><b><span id='greatestTime'></span></b></div>
		<div class='generalerror'><?php echo $error ?></div>
		<div style="text-align:center;"><b><span id='greatestTime'></span></b></div>
		<div id="map"></div>
    <div style='display:inline-block;vertical-align:top' >
      <div  id='singleplotdiv'>
      <textarea style='width:400px;height:100px' id='messagearea'></textarea><br/>
      <button onclick='sendMessage("messagearea")'>send</button>
      Target device:
      <?php 
      $thisDataSql = "SELECT name as text, device_id as value FROM device WHERE manufacture_id IS NOT NULL AND tenant_id=" . intval($user["tenant_id"]) . " ORDER BY name ASC;";
      $result = mysqli_query($conn, $thisDataSql);
      if($result) {
        $selectData = mysqli_fetch_all($result, MYSQLI_ASSOC); 
        echo genericSelect("target_device_id", "target_device_id", defaultFailDown(gvfw("device_id"), $deviceId), $selectData);
      }

      echo showLatestMessages($user["tenant_id"]); 
      
      
      ?>
    </div>
	</div>
</div>
 
   
</body>
</html>