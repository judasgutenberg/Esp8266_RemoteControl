<?php 



function deviceFeatures($userId, $deviceId) {
  Global $conn;
  $table = "device_feature";
  $sql = "SELECT df.name as name, pin_number, df.enabled, df.device_type_feature_id, df.device_feature_id, df.created, last_known_device_value, last_known_device_modified, df.value  FROM " . $table . " df JOIN device_type_feature dtf ON df.device_type_feature_id=dtf.device_type_feature_id ";
  //echo $sql;
  if($deviceId){
    $sql .= "WHERE df.device_id=" . intval($deviceId);

  }
  $result = mysqli_query($conn, $sql);
  $out = "";
  $out .= "<div class='listtitle'>Your " . $table . "s</div>\n";
  $out .= "<div class='listtools'><div class='basicbutton'><a href='?action=startcreate&table=" . $table  . "'>Create</a></div> a new " . $table  . "</div>\n";
  //$out .= "<hr style='width:100px;margin:0'/>\n";
  $headerData = array(
    [
      'label' => 'enabled',
      'name' => 'enabled',
      'liveChangeable' => true,
      'name' => 'enabled',
      'type' => 'bool'
    ],
    [
      'label' => 'name',
      'name' => 'name' 
    ],
    [
      'label' => 'pin number',
      'name' => 'pin_number' 
    ],
    [
      'label' => 'device type feature id',
      'name' => 'device_type_feature_id' 
    ],
    [
      'label' => 'power on',
      'name' => 'value',
      'liveChangeable' => true,
      'type' => 'bool'
    ],
    [
      'label' => 'device feature id',
      'name' => 'device_feature_id' 
    ],
    [
      'label' => 'created',
      'name' => 'created' 
    ],
    [
      'label' => 'last known device value',
      'name' => 'last_known_device_value' 
    ],
    [
      'label' => 'last known device modified',
      'name' => 'last_known_device_modified' 
    ],
    );
    $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a>  | ";
    $toolsTemplate .= "<a href='?action=log&table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>View Log</a>  | ";
    $toolsTemplate .= "<a onclick='return confirm(\"Are you sure you want to delete this " . $table . "?\")' href='?table=" . $table . "&action=delete&" . $table . "_id=<" . $table . "_id/>'>Delete</a>";
    if($result) {
      $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
      
      if($rows) {
        $out .= genericTable($rows, $headerData, $toolsTemplate, null,  $table, $table . "_id", $sql);
      }
    }
    return $out;

}

function devices($userId) {
  Global $conn;
  $table = "device";
  $sql = "SELECT *  FROM " . $table . "  ";
  //echo $sql;
  $result = mysqli_query($conn, $sql);
  $out = "";
  $out .= "<div class='listtitle'>Your " . $table . "s</div>\n";
  $out .= "<div class='listtools'><div class='basicbutton'><a href='?action=startcreate&table=" . $table  . "'>Create</a></div> a new " . $table  . "</div>\n";
  
  //$out .= "<hr style='width:100px;margin:0'/>\n";
  $headerData = array(
    [
	    'label' => 'id',
      'name' => $table . "_id"
	  ],
    [
      'label' => 'name',
      'name' => 'name' 
    ],
    [
      'label' => 'name',
      'name' => 'name' 
    ],
    [
      'label' => 'location name',
      'name' => 'location_name' 
    ],
    [
      'label' => 'ip address',
      'name' => 'ip_address' 
    ],
    );
    $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a> ";
    $toolsTemplate .= " | <a href='?table=device_feature&device_id=<" . $table . "_id/>'>Device Features</a>";
    $toolsTemplate .= " | <a onclick='return confirm(\"Are you sure you want to delete this " . $table . "?\")' href='?table=" . $table . "&action=delete&" . $table . "_id=<" . $table . "_id/>'>Delete</a>";
    if($result) {
      $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
      
      if($rows) {
        $out .= genericTable($rows, $headerData, $toolsTemplate, null,  $table, $table . "_id", $sql);
      }
    }
    return $out;

}

function managementRuleForm($error,  $userId) {
  Global $conn;
  $table = "management_rule";
  $pk = gvfw($table . "_id");
  
  $submitLabel = "save management rule";
  if($pk  == "") {
    $submitLabel = "create management rule";
    $source = $_POST;
  } else {
    $sql = "SELECT * from " . $table . " WHERE " . $table . "_id=" . intval($pk) . " AND user_id=" . intval($userId);
    $result = mysqli_query($conn, $sql);
    if($result) {
      $source = mysqli_fetch_array($result);
    }
  }
  $formData = array(
    [
	    'label' => '',
      'name' => $table . "_id",
      'type' => 'hidden',
	    'value' => gvfa($table . "_id", $source)
	  ],
		[
	    'label' => 'name',
      'name' => 'name',
      'width' => 400,
	    'value' => gvfa("name", $source), 
      'error' => gvfa('name', $error)
	  ],
		[
	    'label' => 'description',
      'name' => 'description',
      'width' => 400,
      'height'=> 50,
	    'value' => gvfa("description", $source), 
      'error' => gvfa('description', $error)
	  ],
    [
	    'label' => 'change to',
      'name' => 'result_value',
      'width' => 100,
	    'value' => gvfa("result_value", $source), 
      'error' => gvfa('result_value', $error)
	  ],
    [
	    'label' => 'time start',
      'name' => 'time_valid_start',
      'width' => 100,
      'type' => "time",
	    'value' => gvfa("time_valid_start", $source), 
      'error' => gvfa('time_valid_start', $error)
	  ],
    [
	    'label' => 'time end',
      'name' => 'time_valid_end',
      'width' => 100,
      'type' => "time",
	    'value' => gvfa("time_valid_end", $source), 
      'error' => gvfa('time_valid_end', $error)
	  ],
    [
	    'label' => 'conditions',
      'name' => 'conditions',
      'width' => 400,
      'height'=> 200,
	    'value' => gvfa("management_script", $source), 
      'error' => gvfa('management_script', $error)
	  ],
    );
  $form = genericForm($formData, $submitLabel);
  return $form;
}

function deviceFeatureLog($deviceFeatureId, $userId){
  Global $conn;
  $headerData = array(
    [
	    'label' => 'recorded',
      'name' => "recorded"
	  ],
    [
      'label' => 'was',
      'name' => 'beginning_state' 
    ],
    [
      'label' => 'became',
      'name' => 'end_state' 
    ],
    [
      'label' => 'mechanism',
      'name' => 'mechanism' 
    ]
    );
  $deviceFeatureName = getDeviceFeature($deviceFeatureId, $userId)["name"];
  $sql = "SELECT * FROM device_feature_log WHERE user_id =" . intval($userId) . " AND device_feature_id=" . intval($deviceFeatureId) . " ORDER BY recorded DESC";
  $result = mysqli_query($conn, $sql);
  $out = "<div class='listheader'>Device Feature Log: " . $deviceFeatureName . "</div>";
  if($result) {
    $rows = $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
    //var_dump($rows);
    if($rows) {
      $out .= genericTable($rows, $headerData, null, null);
    }
    
  }
  return $out;
}

function getDeviceFeature($deviceFeatureId, $userId){
  return getGeneric("device_feature", $deviceFeatureId, $userId);
}

function getGeneric($table, $pk, $userId){
  Global $conn;
  $sql = "SELECT * FROM " . $table . " WHERE " . $table . "_id='" . mysqli_real_escape_string($conn, $pk)  . "' AND user_id=" . intval($userId);
	$result = mysqli_query($conn, $sql);
	if($result) {
		$row = mysqli_fetch_array($result, MYSQLI_ASSOC);
		return $row;
	}
}

//reads data from the cloud about our particular solar installation
function getCurrentSolarData($user) {
    $plantId = $user["energy_api_plant_id"];
    $url = 'https://pv.inteless.com/oauth/token';
    $headers = [
            'Content-Type: application/json;charset=UTF-8', // Set Content-Type header to application/json
            'accept: application/json',
            'Sec-Fetch-Mode: cors',
            'Origin: https://pv.inteless.com',
            'Accept: application/json',
            'Accept-Encoding:   ',
            'Accept-Language: en-US,en;q=0.9',
            'Sec-Ch-Ua: "Chromium";v="124", "Google Chrome";v="124", "Not-A.Brand";v="99"',
            'Sec-Ch-Ua-Platform: Windows',
            'Sec-Ch-Ua-Mobile: ?0',
            'Priority: u=1, i',
            'Referer: https://pv.inteless.com/login',
            'Content-Length: 127',
            'Content-Type: application/json;charset=UTF-8',
            'Sec-Fetch-Dest: empty',
            'Sec-Fetch-Mode: cors',
            'Sec-Fetch-Site: same-origin',
            'User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36',
    ];
    $params = [
            'client_id' => 'csp-web',
            'grant_type' => 'password',
            'password' => $user["energy_api_password"],
            'username' => $user["energy_api_username"],
            'source' => 'elinter',
    ];

    $ch = curl_init();
    // Set cURL options
    curl_setopt($ch, CURLOPT_URL, $url);
    curl_setopt($ch, CURLOPT_POST, true);
    curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode($params));
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HTTPHEADER, $headers); // Set custom headers

    $response = curl_exec($ch);

    if(curl_errno($ch)){
        echo 'Curl error: ' . curl_error($ch);
    }

    curl_close($ch);

    $bodyData = json_decode($response, true);
    $data = $bodyData["data"];
    $access_token = $data['access_token'];
    $currentDateTime = new DateTime('now', new DateTimeZone('America/New_York')); 
    //echo $currentDateTime->format('Y-m-d h:i:s');
    $currentDate =  $currentDateTime->format('Y-m-d');
    $actionUrl = 'https://pv.inteless.com/api/v1/plant/energy/' . $plantId  . '/day?date=' . $currentDate . "&id=" . $plantId . "&lan=en";
    $actionUrl = 'https://pv.inteless.com/api/v1/plant/energy/' . $plantId  . '/flow?date=' . $currentDate . "&id=" . $plantId . "&lan=en"; 
    $actionUrl = 'https://pv.inteless.com/api/v1/plant/energy/' . $plantId  . '/flow';
    $userParams =   [
            'access_token' => $access_token,
            'date' => $currentDate,
            'id' => 16588,
            'lan' => 'en'         
    ];

    // Make GET request to user endpoint
    $queryString = http_build_query($userParams);
    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL, $actionUrl . "?" . $queryString);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

    $dataResponse = curl_exec($ch);
    curl_close($ch);

    $dataBody = json_decode($dataResponse, true);

    return $dataBody["data"];
}
