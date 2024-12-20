<?php 



function deviceFeatures($tenantId, $deviceId) {
  Global $conn;
  $table = "device_feature";
  $sql = "SELECT df.name as name, pin_number, allow_automatic_management, df.enabled, df.device_type_feature_id, df.device_feature_id, df.created, last_known_device_value, last_known_device_modified, df.value  
          FROM " . $table . " df 
          JOIN device_type_feature dtf 
            ON df.device_type_feature_id=dtf.device_type_feature_id AND df.tenant_id=dtf.tenant_id WHERE df.tenant_id=" . intval($tenantId);
  //echo $sql;
  if($deviceId){
    $sql .= " AND df.device_id=" . intval($deviceId);

  }
  $result = mysqli_query($conn, $sql);
  $out = "";
  $out .= "<div class='listtitle'>Your " . $table . "s</div>\n";
  $additionalValueQueryString = "";
  foreach($_REQUEST as $key=>$value){ //slurp up values passed in
    if(endsWith($key, "_id")){
      $additionalValueQueryString .= "&" . $key . "=" . urlencode($value);
    }
  }
  $out .= "<div class='listtools'><div class='basicbutton'><a href='?action=startcreate&table=" . $table  . $additionalValueQueryString . "'>Create</a></div> a new device feature</div>\n";
  //$out .= "<hr style='width:100px;margin:0'/>\n";
  $headerData = array(
    [
      'label' => 'enabled',
      'name' => 'enabled',
      'changeable' => true,
      'name' => 'enabled',
      'accent_color' => "red",
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
      'label' => 'auto-manage',
      'name' => 'allow_automatic_management',
      'changeable' => true,
      'accent_color' => "red",
      'type' => 'bool'
    ],
    [
      'label' => 'power on',
      'name' => 'value',
      'changeable' => true,
      'accent_color' => "blue",
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
      'name' => 'last_known_device_modified',
      "type" => "datetime",
      "function" => 'timeAgo("<last_known_device_modified/>")'
    ],
    );
    $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a>  | ";
    $toolsTemplate .= "<a href='?action=log&table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>View Log</a>  | ";
    $toolsTemplate .= deleteLink($table, $table. "_id" ); 
    if($result) {
      $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
      
      if($rows) {
        $out .= genericTable($rows, $headerData, $toolsTemplate, null,  $table, $table . "_id", $sql);
      }
    }
    return $out;
}

function timeScales() {
  $out = json_decode('[
    {"text":"three-hour","value":"three-hour", "period_size": 3, "period_scale": "hour", "initial_offset": 5},
    {"text":"day","value":"day", "period_size": 1, "period_scale": "day", "group_by": "YEAR(recorded), DAYOFYEAR(recorded), HOUR(recorded), MINUTE(recorded)"},
    {"text":"week","value":"week", "period_size": 7, "period_scale": "day", "group_by": "YEAR(recorded), DAYOFYEAR(recorded), HOUR(recorded)"},
    {"text":"month","value":"month", "period_size": 1, "period_scale": "month", "group_by": "YEAR(recorded), DAYOFYEAR(recorded), FLOOR(HOUR(recorded)/6)"},
    {"text":"year","value":"year", "period_size": 1, "period_scale": "year", "group_by": "YEAR(recorded), DAYOFYEAR(recorded)" }
  ]', true);
  return $out;
}

function devices($tenantId) {
  Global $conn;
  $table = "device";
  $sql = "SELECT *  FROM " . $table . "  WHERE tenant_id=" . intval($tenantId);
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
      'label' => 'location name',
      'name' => 'location_name' 
    ],
    [
      'label' => 'ip address',
      'name' => 'ip_address' 
    ],
    [
      'label' => 'last poll',
      'name' => 'last_poll',
       'function' => 'timeAgo("<last_poll/>")'
    ],
    );
    $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a> ";
    $toolsTemplate .= " | <a href='?table=device_feature&device_id=<" . $table . "_id/>'>Device Features</a>";
    $toolsTemplate .= " | " . deleteLink($table, $table. "_id" ); 
    if($result) {
      $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
      if($rows) {
        $out .= genericTable($rows, $headerData, $toolsTemplate, null,  $table, $table . "_id", $sql);
      }
    }
    return $out;

}

function reports($tenantId, $user) {
  Global $conn;
  $table = "report";
  $sql = "SELECT *  FROM " . $table . "  WHERE tenant_id=" . intval($tenantId);
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
      'label' => 'role',
      'name' => 'role' 
    ],
    [
      'label' => 'created',
      'name' => 'created' 
    ],
    [
      'label' => 'modified',
      'name' => 'modified' 
    ],
    );
    $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>&action=fetch'>Run</a>";
    if($user["role"] == "super") {
      $toolsTemplate .= " | <a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a> ";
      $toolsTemplate .= " | " . deleteLink($table, $table. "_id" ); 
    }
    if($result) {
      $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
      
      if($rows) {
        $out .= genericTable($rows, $headerData, $toolsTemplate, null,  $table, $table . "_id", $sql);
      }
    }
    return $out;

}



function mergeValues($dataArray, $valuesArray) {
  foreach ($dataArray as &$item) {
      if (isset($valuesArray[$item['name']])) {
          $item['value'] = $valuesArray[$item['name']];
      }
  }
  return $dataArray;
}

function copyValuesFromSourceToDest($dest, $source) {
  // Create an associative array from the source array for quick lookup
  $sourceValues = [];
  foreach ($source as $srcItem) {
      if (isset($srcItem['name']) && isset($srcItem['value'])) {
          $sourceValues[$srcItem['name']] = $srcItem['value'];
      }
  }
  // Update the dest array with values from the source array
  foreach ($dest as &$destItem) {
      if (isset($destItem['name']) && isset($sourceValues[$destItem['name']])) {
          $destItem['value'] = $sourceValues[$destItem['name']];
      }
  }
  return $dest;
}

function users(){
  Global $conn;
  $table = "user";
  $out = "<div class='listheader'>Users</div>";
  $out .= "<div class='listtools'><div class='basicbutton'><a href='?action=startcreate&table=" . $table  . "'>Create</a></div> a new user</div>\n";
  $headerData = array(
    [
	    'label' => 'id',
      'name' => $table . "_id"
	  ],
		[
	    'label' => 'email',
      'name' => 'email'
	  ] ,
    [
	    'label' => 'expired',
      'name' => 'expired'
	  ],
		[
	    'label' => 'role',
      'name' => 'role'
    ],
		[
	    'label' => 'created',
      'name' => 'created'
	  ] 
    );
 
  $sql = "SELECT * FROM " . $table . " ORDER BY created DESC";
  $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a> ";
  $toolsTemplate .= " | " . deleteLink($table, $table. "_id" ); 
  $result = mysqli_query($conn, $sql);
  
  if($result) {
    $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
    //var_dump($rows);
    if($rows) {
      // genericTable($rows, $headerData, $toolsTemplate, null,  $table, $table . "_id", $sql);
      $out .= genericTable($rows, $headerData, $toolsTemplate, null, $table, $table . "_id");
    }
    
  }
  return $out;
}

function editUser($error){
  Global $conn;
  $table = "user";
  $pk = gvfw($table . "_id");
  
  
  $submitLabel = "save user";
  if($pk  == "") {
    $submitLabel = "create user";
    $source = $_POST;
  } else {
    $sql = "SELECT * FROM " . $table . " WHERE " . $table . "_id=" . intval($pk);
    $result = mysqli_query($conn, $sql);
    if($result) {
      $source = mysqli_fetch_array($result);
    }
  }
  if(!$pk){
    $pk = "NULL";
  }
  $formData = array(
    [
	    'label' => 'id',
      'name' => $table . "_id",
      'type' => 'read_only'
	  ],
		[
	    'label' => 'email',
      'name' => 'email',
      'type' => 'text',
      'value' => gvfa("email", $source)
	  ],
    [
	    'label' => 'password',
      'name' => 'password',
      'type' => 'plaintext_password',
      'value' => gvfa("password", $source)
	  ] ,
    [
	    'label' => 'expired',
      'name' => 'expired',
      'type' => 'datetime',
      'value' => gvfa("expired", $source)
	  ] ,
    [
	    'label' => 'role',
      'name' => 'role',
      'type' => 'select',
      'value' => gvfa("role", $source),
      'values'=> availableRoles()
	  ],
		[
	    'label' => 'created',
      'name' => 'created',
      'type' => 'datetime',
      'value' => gvfa("created", $source)
    ],
    [
	    'label' => 'tenants',
      'name' => 'tenant_id',
      'type' => 'many-to-many',
      'mapping_table' => 'tenant_user',
 
	    'value' => gvfa("tenant_id", $source), 
      'error' => gvfa("tenant_id", $error),
      'item_tool' => 'tenantTool',
      'values' => "SELECT 
              t.tenant_id, 
              t.name AS 'text', 
              MAX(tu.user_id = " . $pk . ") AS has
          FROM 
              tenant t
          LEFT JOIN 
              tenant_user tu
          ON 
              t.tenant_id = tu.tenant_id 
          GROUP BY 
              t.tenant_id, t.name
          ORDER BY 
              t.name ASC;"
	  ] 
  );
 
  $form = genericForm($formData, $submitLabel);
  return $form;
}

function tenants($user){
  Global $conn;
  $table = "tenant";
  $out = "<div class='listheader'>Users</div>";
  $out .= "<div class='listtools'><div class='basicbutton'><a href='?action=startcreate&table=" . $table  . "'>Create</a></div> a new user</div>\n";
  $headerData = array(
    [
	    'label' => 'id',
      'name' => $table . "_id"
	  ],
		[
	    'label' => 'name',
      'name' => 'name'
	  ] ,
    [
	    'label' => 'expired',
      'name' => 'expired'
	  ],
		//[
	  //  'label' => 'role',
    //  'name' => 'role'
    //],
		[
	    'label' => 'created',
      'name' => 'created'
	  ] 
    );
  if($user["role"] == "super"){
    $sql = "SELECT * from " . $table . " ORDER BY name ASC";
  } else if($user["role"] == "admin"){
    $sql = "SELECT * from " . $table . " t JOIN tenant_user tu ON t.tenant_id=tu.tenant_id WHERE tu.user_id=" . intval($user["user_id"]) . " AND tu.tenant_id=" . intval($user["tenant_id"]) . " ORDER BY t.name";
 
  }
  //echo $sql;
  $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a> ";
  $toolsTemplate .= " | " . deleteLink($table, $table. "_id" ); 
  $result = mysqli_query($conn, $sql);
  
  if($result) {
    $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
    //var_dump($rows);
    if($rows) {
      // genericTable($rows, $headerData, $toolsTemplate, null,  $table, $table . "_id", $sql);
      $out .= genericTable($rows, $headerData, $toolsTemplate, null, $table, $table . "_id");
    }
    
  }
  return $out;
}

function editTenant($error, $user){
  Global $conn;
  $table = "tenant";
  $pk = gvfw($table . "_id");
  $source = null;
  
  $submitLabel = "save tenant";
  if($pk  == "") {
    $submitLabel = "create tenant";
    $source = $_POST;
  } else {
    if ($user["role"] == "super"){
      $sql = "SELECT * from " . $table . " WHERE  " . $table . "_id=" . intval($pk);
    } else {
      $sql = "SELECT * from " . $table . " t JOIN tenant_user tu ON t.tenant_id=tu.tenant_id WHERE tu.user_id=" . $user["user_id"] . " AND  t." . $table . "_id=" . intval($pk);
    }
    
    $result = mysqli_query($conn, $sql);
    if($result) {
      $source = mysqli_fetch_array($result);
    }
  }
  if(!$pk){
    $pk = "NULL";
  }
  $formData = array(
    [
	    'label' => 'id',
      'name' => $table . "_id",
      'type' => 'read_only'
	  ],
 
    [
	    'label' => 'name',
      'name' => 'name',
      'type' => 'text',
      'value' => gvfa("name", $source)
	  ] ,
    [
	    'label' => 'latitude',
      'name' => 'latitude',
      'type' => "number",
      'width' => 200,
	    'value' => gvfa("latitude", $source), 
      'error' => gvfa('latitude', $error)
	  ],
    [
	    'label' => 'longitude',
      'name' => 'longitude',
      'type' => "number",
      'width' => 200,
	    'value' => gvfa("longitude", $source), 
      'error' => gvfa('longitude', $error)
	  ],
    [
	    'label' => 'about',
      'name' => 'about',
      'type' => 'text',
      'width' => 400,
      'height'=> 50,
      'value' => gvfa("about", $source)
	  ] ,
    [
	    'label' => 'expired',
      'name' => 'expired',
      'type' => 'datetime',
      'value' => gvfa("expired", $source)
	  ] 
 
    ,
    [
	    'label' => 'storage_password',
      'name' => 'storage_password',
      'type' => 'text',
      'value' => gvfa("storage_password", $source)
	  ] 
    ,
    [
	    'label' => 'Energy API username',
      'name' => 'energy_api_username',
      'type' => 'text',
      'value' => gvfa("energy_api_username", $source)
	  ],
		[
	    'label' => 'Energy API password',
      'name' => 'energy_api_password',
      'type' => 'text',
      'value' => gvfa("energy_api_password", $source)
    ],
    [
	    'label' => 'Energy API plant id',
      'name' => 'energy_api_plant_id',
      'type' => 'text',
      'value' => gvfa("energy_api_plant_id", $source)
    ],
    [
	    'label' => 'Open_weather API key',
      'name' => 'open_weather_api_key',
      'type' => 'text',
      'value' => gvfa("open_weather_api_key", $source)
    ],
    [
	    'label' => 'Preferences',
      'name' => 'preferences',
      'type' => 'json',
      'value' => gvfa("preferences", $source),
      'template'=>" {  \"location_id\": \"\"  } "
    ]
    

  );
  //only allow super users to manage users belonging to a tenant
  if($user["role"] == "super" || $user["role"] == "admin"){ //it's okay if admins can see other users on the server and can add or subtract them from their tenant
    $formData[] =
    [
	    'label' => 'users',
      'name' => 'user_id',
      'type' => 'many-to-many',
      'mapping_table' => 'tenant_user',
 
	    'value' => gvfa("user_id", $source), 
      'error' => gvfa("user_id", $error),
      'item_tool' => 'userTool',
      'values' => "SELECT 
              u.user_id, 
              u.email AS 'text', 
              MAX(tu.tenant_id = " . $pk . ") AS has
          FROM 
              user u
          LEFT JOIN 
              tenant_user tu
          ON 
              u.user_id = tu.user_id 
 
          GROUP BY 
              u.user_id, u.email
          ORDER BY 
              u.email ASC;"
	  ];

  }
  $form = genericForm($formData, $submitLabel);
  return $form;
}

function editDeviceFeature($error,  $user) {
  Global $conn;
  $tenantId = $user["tenant_id"];
  $table = "device_feature";
  $pk = gvfw($table . "_id");
  
  $submitLabel = "save device feature";
  if($pk  == "") {
    $submitLabel = "create device feature";
    $source = $_POST;
  } else {
    $sql = "SELECT * from " . $table . " WHERE " . $table . "_id=" . intval($pk) . " AND tenant_id=" . intval($tenantId);
    $result = mysqli_query($conn, $sql);
    if($result) {
      $source = mysqli_fetch_array($result);
    }
  }
  if(!$pk){
    $pk = "NULL";
  }
  $formData = array(
    [
	    'label' => 'id',
      'name' => $table . "_id",
      'type' => 'read_only',
	    'value' => gvfa($table . "_id", $source)
	  ],
    [
	    'label' => 'created',
      'name' => "created",
      'type' => 'read_only',
	    'value' => gvfa("created", $source)
	  ],
    [
	    'label' => 'user modified',
      'name' => "modified",
      'type' => 'read_only',
	    'value' => gvfa("modified", $source)
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
	    'label' => 'device feature type',
      'name' => 'device_type_feature_id',
      'type' => 'select',
	    'value' => gvfa("device_type_feature_id", $source), 
      'error' => gvfa("device_type_feature_id", $error),
      'values' => "SELECT device_type_feature_id, name as 'text' FROM device_type_feature WHERE tenant_id='" . $tenantId  . "' ORDER BY name ASC",
	  ] ,
    [
	    'label' => 'enabled',
      'accent_color' => "red",
      'name' => 'enabled',
      'type' => 'bool',
	    'value' => gvfa("enabled", $source), 
      'error' => gvfa('enabled', $error)
	  ],

    [
	    'label' => 'value',
      'accent_color' => "blue",
      'name' => 'value',
      'type' => 'number',
      'width' => 100,
	    'value' => gvfa("value", $source), 
      'error' => gvfa('value', $error)
	  ],
    [
	    'label' => 'device',
      'name' => 'device_id',
      'type' => 'select',
	    'value' => gvfa("device_id", $source), 
      'error' => gvfa("device_id", $error),
      'values' => "SELECT device_id, name as 'text' FROM device WHERE tenant_id='" . $tenantId  . "' ORDER BY name ASC",
	  ] ,
    [
	    'label' => 'allow automation',
      'accent_color' => "red",
      'name' => 'allow_automatic_management',
      'type' => 'bool',
	    'value' => gvfa("allow_automatic_management", $source), 
      'error' => gvfa('allow_automatic_management', $error)
	  ],
    [
	    'label' => 'temporary automation suspension time (in hours)',
      'name' => 'restore_automation_after',
      'type' => 'number',
	    'value' => gvfa("restore_automation_after", $source), 
      'error' => gvfa('restore_automation_after', $error)
	  ],
    [
	    'label' => 'when automation was suspended',
      'name' => 'automation_disabled_when',
      'type' => 'datetime',
	    'value' => gvfa("automation_disabled_when", $source), 
      'error' => gvfa('automation_disabled_when', $error)
	  ],
 
 
    [
	    'label' => 'management rules',
      'name' => 'management_rule_id',
      'type' => 'many-to-many',
      'mapping_table' => 'device_feature_management_rule',
      'counting_column' => 'management_priority',
	    'value' => gvfa("management_rule_id", $source), 
      'error' => gvfa("management_rule_id", $error),
      'item_tool' => 'managementRuleTool',
      //SELECT m.management_rule_id, name as 'text', (d.device_feature_id IS NOT NULL) AS has FROM management_rule m LEFT JOIN device_feature_management_rule d ON m.management_rule_id=d.management_rule_id AND   m.tenant_id=d.tenant_id WHERE d.device_feature_id IS NULL OR d.device_feature_id=3 AND m.tenant_id='1'  ORDER BY m.name ASC
      //SELECT m.management_rule_id, name as 'text', (d.device_feature_id = 3) AS has FROM management_rule m LEFT JOIN device_feature_management_rule d ON m.management_rule_id=d.management_rule_id AND   m.tenant_id=d.tenant_id group by m.management_rule_id, name   ORDER BY m.name ASC 
      'values' => "SELECT 
                      m.management_rule_id, 
                      m.name AS 'text', 
                      MAX(CASE WHEN d.device_feature_id = " . $pk . " AND d.tenant_id = " . $tenantId . " THEN 1 ELSE 0 END) AS has
                  FROM 
                      management_rule m
                  LEFT JOIN 
                      device_feature_management_rule d 
                  ON 
                      m.management_rule_id = d.management_rule_id 
                      AND m.tenant_id = d.tenant_id
                  WHERE 
                      m.tenant_id = " . $tenantId . " 
                  GROUP BY 
                      m.management_rule_id, m.name
                  ORDER BY 
                      m.name ASC;"
	  ] 
    );
  $form = genericForm($formData, $submitLabel);
  return $form;
}



function editCommandType($error,  $user) {
  Global $conn;
  $tenantId = $user["tenant_id"];
  $table = "command_type";
  $pk = gvfw($table . "_id");
  
  $submitLabel = "save command type";
  if($pk  == "") {
    $submitLabel = "create command type";
    $source = $_POST;
  } else {
    $sql = "SELECT * from " . $table . " WHERE " . $table . "_id=" . intval($pk) . " AND tenant_id=" . intval($tenantId);
    $result = mysqli_query($conn, $sql);
    if($result) {
      $source = mysqli_fetch_array($result);
    }
  }
  if(!$pk){
    $pk = "NULL";
  }
  $formData = array(
    [
	    'label' => 'id',
      'name' => $table . "_id",
      'type' => 'read_only',
	    'value' => gvfa($table . "_id", $source)
	  ],
    [
	    'label' => 'created',
      'name' => "created",
      'type' => 'read_only',
	    'value' => gvfa("created", $source)
	  ],
    [
	    'label' => 'Name',
      'name' => "name",
      'type' => 'string',
	    'value' => gvfa("name", $source), 
      'error' => gvfa("name", $error),
	  ],
    [
	    'label' => 'Associated Table',
      'name' => "associated_table",
      'type' => 'select',
	    'value' => gvfa("associated_table", $source), 
      'error' => gvfa("associated_table", $error),
      'change-function' => "getColumnsForTable('associated_table', 'value_column', 'name_column')",
      'values' => schemaTables()
	  ],
  
    [
	    'label' => 'Value Column',
      'name' => "value_column",
      'type' => 'select',
	    'value' => gvfa("value_column", $source), 
      'error' => gvfa("value_column", $error),
      'values' => getColumns(gvfa("associated_table", $source))
	  ],
    [
	    'label' => 'Name Column',
      'name' => "name_column",
      'type' => 'select',
	    'value' => gvfa("name_column", $source), 
      'error' => gvfa("name_column", $error),
      'values' => getColumns(gvfa("associated_table", $source))
	  ],
    );
  $form = genericForm($formData, $submitLabel);
  return $form;
}

function commandTypes($tenantId, $deviceId){
  Global $conn;
  $table = "command_type";
  $out = "<div class='listheader'>Command Types</div>";
  $out .= "<div class='listtools'><div class='basicbutton'><a href='?action=startcreate&table=" . $table  . "'>Create</a></div> a command</div>\n";
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
	    'label' => 'associated table',
      'name' => 'associated_table'
	  ],
    [
	    'label' => 'value column',
      'name' => 'value_column'
	  ],
    [
	    'label' => 'created',
      'name' => 'created'
	  ] ,
    );
    $sql = "SELECT * from command_type WHERE tenant_id =" . intval($tenantId) . " ORDER BY name DESC";
    $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a> ";
    $toolsTemplate .= " | " . deleteLink($table, $table. "_id" ); 
    $result = mysqli_query($conn, $sql);
   
    if($result) {
      $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
      //var_dump($rows);
      if($rows) {
        $out .= genericTable($rows, $headerData, $toolsTemplate, null, $table, $table . "_id", $sql);
      }
      
    }
    return $out;
}


function editCommand($error,  $user) {
  Global $conn;
  $tenantId = $user["tenant_id"];
  $table = "command";
  $pk = gvfw($table . "_id");
  
  $submitLabel = "save command";
  if($pk  == "") {
    $submitLabel = "create command";
    $source = $_POST;
  } else {
    $sql = "SELECT * from " . $table . " WHERE " . $table . "_id=" . intval($pk) . " AND tenant_id=" . intval($tenantId);
    $result = mysqli_query($conn, $sql);
    if($result) {
      $source = mysqli_fetch_array($result);
    }
  }
  if(!$pk){
    $pk = "NULL";
  }
  $script = "getValuesFromCommandTypeTable('command_type_id', 'command_value')";
  $formData = array(
    [
	    'label' => 'id',
      'name' => $table . "_id",
      'type' => 'read_only',
	    'value' => gvfa($table . "_id", $source)
	  ],
    [
	    'label' => 'created',
      'name' => "created",
      'type' => 'read_only',
	    'value' => gvfa("created", $source)
	  ],
      
    [
	    'label' => 'Device',
      'accent_color' => "red",
      'name' => 'device_id',
      'type' => 'select',
	    'value' => gvfa("device_id", $source), 
      'error' => gvfa('device_id', $error),
      'values' => "SELECT device_id, name as 'text' FROM device WHERE tenant_id='" . $tenantId  . "' ORDER BY name ASC",
	  ],
    [
	    'label' => 'Command Type',
      'name' => "command_type_id",
      'type' => 'select',
	    'value' => gvfa("command_type_id", $source), 
      'error' => gvfa("command_type_id", $error),
      'values' => "SELECT command_type_id, name as 'text' FROM command_type WHERE tenant_id='" . $tenantId  . "' ORDER BY name ASC",
      'change-function' => $script,
	  ],
 

    [
	    'label' => 'Command Value',
      'name' => "command_value",
      'type' => 'string',
	    'value' => gvfa("command_value", $source), 
      'error' => gvfa("command_value", $error),
	  ],
    [
	    'label' => 'done',
      'accent_color' => "red",
      'name' => 'done',
      'type' => 'bool',
	    'value' => gvfa("done", $source), 
      'error' => gvfa('done', $error)
	  ],
    );
  $form = genericForm($formData, $submitLabel, "Saving command...", $user, $script);
  return $form;
}

function commands($tenantId, $deviceId){
  Global $conn;
  $table = "command";
  $out = "<div class='listheader'>Commands</div>";
  $out .= "<div class='listtools'><div class='basicbutton'><a href='?action=startcreate&table=" . $table  . "'>Create</a></div> a command</div>\n";
  $headerData = array(
    [
	    'label' => 'id',
      'name' => $table . "_id"
	  ],
		[
	    'label' => 'command type',
      'name' => 'command'
	  ] ,
    [
	    'label' => 'device',
      'name' => 'device'
	  ] ,
    [
	    'label' => 'command value',
      'name' => 'command_value'
	  ] ,
    [
	    'label' => 'done',
      'name' => 'done',
      'type' => 'bool',
      'changeable' => true
	  ] ,
    [
      'label' => 'performed',
      'name' => 'performed',
       'function' => 'timeAgo("<performed/>")'
    ],
		[
	    'label' => 'created',
      'name' => 'created'
	  ] 
    );
 
  $sql = "SELECT c.name AS command, d.name AS device, t.command_id, t.done, t.created, performed, command_value, associated_table, value_column FROM " . $table . " t 
  LEFT JOIN command_type c ON t.command_type_id = c.command_type_id  AND t.tenant_id = c.tenant_id 
  LEFT JOIN device d ON t.device_id = d.device_id  AND t.tenant_id = d.tenant_id
  WHERE t.tenant_id =" . intval($tenantId) . " ORDER BY t.created DESC";
  $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a> ";
  $toolsTemplate .= " | " . deleteLink($table, $table. "_id" ); 
  $result = mysqli_query($conn, $sql);
 
  if($result) {
    $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
    //var_dump($rows);
    if($rows) {
      $out .= genericTable($rows, $headerData, $toolsTemplate, null, $table, $table . "_id", $sql);
    }
    
  }
  return $out;
}

function editReport($error,  $tenantId) {
  Global $conn;
  $table = "report";
  $pk = gvfw($table . "_id");
  
  $submitLabel = "save report";
  if($pk  == ""  && $_POST) {
 
    $submitLabel = "create device";
    $source = $_POST;
  } else {
    $sql = "SELECT * FROM " . $table . " WHERE " . $table . "_id=" . intval($pk) . " AND tenant_id=" . intval($tenantId);
    $result = mysqli_query($conn, $sql);
    if($result) {
      $source = mysqli_fetch_array($result);
    }
  }
  if(!$pk){
    $pk = "NULL";
  }
  $formData = array(
    [
	    'label' => 'created',
      'name' => 'created',
      'type' => 'read_only',
	    'value' => gvfa("created", $source), 
      'error' => gvfa('created', $error)
	  ],
    [
	    'label' => 'modified',
      'name' => 'modified',
      'type' => 'read_only',
	    'value' => gvfa("modified", $source), 
      'error' => gvfa('modified', $error)
	  ],
    [
	    'label' => 'id',
      'name' => $table . "_id",
      'type' => 'read_only',
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
	    'label' => 'role',
      'name' => 'role',
      'type' => 'select',
      'value' => gvfa("role", $source),
      'values'=> availableRoles()
	  ],
    //templateable, when set to true, makes it so this report is good enough (and secure enough) to be part of the starter-pack for new tenants
    [
	    'label' => 'templateable',
      'name' => 'templateable',
      'type' => 'bool',
      'value' => gvfa("templateable", $source)
	  ],
		[
	    'label' => 'form',
      'name' => 'form',
      'code_language' => 'json',
      'width' => 500,
      'height'=> 200,
	    'value' => gvfa("form", $source), 
      'error' => gvfa('form', $error),
      'frontend_validation' => "checkJsonSyntax('form')"
	  ],
    [
	    'label' => 'sql',
      'name' => 'sql',
      'code_language' => 'sql',
      'width' => 500,
      'height'=> 200,
	    'value' => gvfa("sql", $source), 
      'error' => gvfa('sql', $error),
      'frontend_validation' => "checkSqlSyntax('sql')"
	  ]
    );
  $form = genericForm($formData, $submitLabel);
  return $form;
}

function editDevice($error,  $tenantId) {
  Global $conn;
  $table = "device";
  $pk = gvfw($table . "_id");
  
  $submitLabel = "save device";
  if($pk  == ""  && $_POST) {
 
    $submitLabel = "create device";
    $source = $_POST;
  } else {
    $sql = "SELECT * FROM " . $table . " WHERE " . $table . "_id=" . intval($pk) . " AND tenant_id=" . intval($tenantId);
    $result = mysqli_query($conn, $sql);
    if($result) {
      $source = mysqli_fetch_array($result);
    }
  }
  if(!$pk){
    $pk = "NULL";
  }
  $formData = array(
    [
	    'label' => 'id',
      'name' => $table . "_id",
      'type' => 'read_only',
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
	    'label' => 'created',
      'name' => 'created',
      'type' => 'read_only',
	    'value' => gvfa("created", $source), 
      'error' => gvfa('created', $error)
	  ],
    [
	    'label' => 'device type',
      'name' => 'device_type_id',
      'type' => 'select',
	    'value' => gvfa("device_type_id", $source), 
      'error' => gvfa('device_type_id', $error),
      'values' => "SELECT device_type_id, name as 'text' FROM device_type WHERE tenant_id='" . $tenantId  . "' ORDER BY name ASC",
	  ],
    [
	    'label' => 'location name',
      'name' => 'location_name',
      'width' => 200,
	    'value' => gvfa("location_name", $source), 
      'error' => gvfa('location_name', $error)
	  ],
    [
	    'label' => 'latitude',
      'name' => 'latitude',
      'type' => "number",
      'width' => 200,
	    'value' => gvfa("latitude", $source), 
      'error' => gvfa('latitude', $error)
	  ],
    [
	    'label' => 'longitude',
      'name' => 'longitude',
      'type' => "number",
      'width' => 200,
	    'value' => gvfa("longitude", $source), 
      'error' => gvfa('longitude', $error)
	  ],
    [
	    'label' => 'ip address',
      'name' => 'ip_address',
      
      'width' => 200,
	    'value' => gvfa("ip_address", $source), 
      'error' => gvfa('ip_address', $error)
	  ],
    [
	    'label' => 'sensor',
      'name' => 'sensor_id',
      'type' => "int",
      'width' => 200,
	    'value' => gvfa("sensor_id", $source), 
      'error' => gvfa('sensor_id', $error)
	  ],
    [
	    'label' => 'reserved 1 name',
      'name' => 'reserved1_name',
      'width' => 400,
	    'value' => gvfa("reserved1_name", $source), 
      'error' => gvfa('reserved1_name', $error)
	  ],
    [
	    'label' => 'reserved 2 name',
      'name' => 'reserved2_name',
      'width' => 400,
	    'value' => gvfa("reserved2_name", $source), 
      'error' => gvfa('reserved2_name', $error)
	  ],
    [
	    'label' => 'reserved 3 name',
      'name' => 'reserved3_name',
      'width' => 400,
	    'value' => gvfa("reserved3_name", $source), 
      'error' => gvfa('reserved3_name', $error)
	  ],
    [
	    'label' => 'reserved 4 name',
      'name' => 'reserved4_name',
      'width' => 400,
	    'value' => gvfa("reserved4_name", $source), 
      'error' => gvfa('reserved4_name', $error)
	  ],
    );
  $form = genericForm($formData, $submitLabel);
  return $form;
}

function editManagementRule($error,  $tenantId) {
  Global $conn;
  $table = "management_rule";
  $pk = gvfw($table . "_id");
  
  $submitLabel = "save management rule";
  if($pk  == "") {
    $submitLabel = "create management rule";
    $source = $_POST;
  } else {
    $sql = "SELECT * from " . $table . " WHERE " . $table . "_id=" . intval($pk) . " AND tenant_id=" . intval($tenantId);
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
      'type' => 'int',
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
      'no_syntax_highlighting' => true,
      'width' => 400,
      'height'=> 200,
	    'value' => gvfa("conditions", $source), 
      'error' => gvfa('conditions', $error)
	  ],
    [
	    'label' => 'created',
      'name' => 'created',
      'type' => 'datetime',
      'value' => gvfa("created", $source)
    ],
    );
  $form = genericForm($formData, $submitLabel);
  $form .= managementRuleTools();
  return $form;
}

function managementRules($tenantId, $deviceId){
  Global $conn;
  $table = "management_rule";
  $out = "<div class='listheader'>Management Rules</div>";
  $out .= "<div class='listtools'><div class='basicbutton'><a href='?action=startcreate&table=" . $table  . "'>Create</a></div> a management rule</div>\n";
  $headerData = array(
    [
	    'label' => 'id',
      'name' => $table . "_id"
	  ],
		[
	    'label' => 'name',
      'name' => 'name'
	  ] ,
		[
	    'label' => 'created',
      'name' => 'created'
	  ] 
    );
 
  $sql = "SELECT * FROM " . $table . " WHERE tenant_id =" . intval($tenantId) . " ORDER BY created DESC";
  $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a> ";
  $toolsTemplate .= " | " . deleteLink($table, $table. "_id" ); 
  $result = mysqli_query($conn, $sql);
 
  if($result) {
    $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
    //var_dump($rows);
    if($rows) {
      $out .= genericTable($rows, $headerData, $toolsTemplate, null, $table, $table . "_id");
    }
    
  }
  return $out;
}

function deviceFeatureLog($deviceFeatureId, $tenantId){
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
    ],
    [
      'label' => 'user',
      'name' => 'email' 
    ],
    [
      'label' => 'rule',
      'name' => 'rule_name' 
    ]
    );
  $deviceFeatureName = getDeviceFeature($deviceFeatureId, $tenantId)["name"];
  $sql = "SELECT recorded, beginning_state, end_state, mechanism, m.name AS rule_name , email, u.user_id  FROM device_feature_log f LEFT JOIN management_rule m ON m.management_rule_id=f.management_rule_id  AND m.tenant_id=f.tenant_id LEFT JOIN user u ON f.user_id = u.user_id WHERE f.tenant_id =" . intval($tenantId) . " AND device_feature_id=" . intval($deviceFeatureId) . " ORDER BY recorded DESC LIMIT 0,500";
  //die($sql);
  $result = mysqli_query($conn, $sql);
  $out = "<div class='listheader'>Device Feature Log: " . $deviceFeatureName . "</div>";
  if($result) {
    $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
    //var_dump($rows);
    if($rows) {
      $out .= genericTable($rows, $headerData, null, null);
    }
    
  }
  return $out;
}

function getDeviceFeature($deviceFeatureId, $tenantId){
  return getGeneric("device_feature", $deviceFeatureId, $tenantId);
}

function getGeneric($table, $pk, $tenantId){
  Global $conn;
  $sql = "SELECT * FROM " . $table . " WHERE " . $table . "_id='" . mysqli_real_escape_string($conn, $pk)  . "' AND tenant_id=" . intval($tenantId);
	$result = mysqli_query($conn, $sql);
	if($result) {
		$row = mysqli_fetch_array($result, MYSQLI_ASSOC);
		return $row;
	}
}

function getDevices($tenantId){
  Global $conn;
  $sql = "SELECT * FROM device WHERE tenant_id=" . intval($tenantId);
	$result = mysqli_query($conn, $sql);
	if($result) {
		$rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
		return $rows;
	}
}
function saveSolarData($tenant, $gridPower, $batteryPercent,  $batteryPower, $loadPower, 
  $solarString1, $solarString2, $batteryVoltage, 
  $mysteryValue3,
  $mysteryValue1,
  $mysteryValue2,
  $changer1,
  $changer2,
  $changer3,
  $changer4,
  $changer5,
  $changer6,
  $changer7
) {
  Global $conn;
  $date = new DateTime("now", new DateTimeZone('America/New_York'));
  $formatedDateTime =  $date->format('Y-m-d H:i:s');
  $nowTime = strtotime($formatedDateTime);

  $weatherDescriptionKey = "weather_description";
  $weatherDescription = readMemoryCache($weatherDescriptionKey, 10);
  if(!$weatherDescription) {
    $weatherData = getWeatherDataByCoordinates($tenant["latitude"], $tenant["longitude"], $tenant["open_weather_api_key"]);
    //var_dump($weatherData);
    $weatherDescription = $weatherData["weather"][0]["description"];
    writeMemoryCache($weatherDescriptionKey, $weatherDescription);
  }
  if($weatherDescription == "") {
    $weatherDescription = "none";
  }
  
  $loggingSql = "INSERT INTO inverter_log ( tenant_id, recorded, 
  solar_power, load_power, grid_power, battery_percentage, battery_power,
  battery_voltage, mystery_value3,
  mystery_value1,
  mystery_value2,
  changer1,
  changer2,
  changer3,
  changer4,
  changer5,
  changer6,
  changer7,
  weather
  ) VALUES (";
  $loggingSql .= $tenant["tenant_id"] . ",'" . $formatedDateTime . "'," .
   intval(intval($solarString1) + intval($solarString2)) . "," . 
   $loadPower. "," . 
   $gridPower . "," . 
   $batteryPercent . "," . 
   $batteryPower . "," .  
   $batteryVoltage . "," .  
   $mysteryValue3 . "," .
   $mysteryValue1 . "," .
   $mysteryValue2 . "," .
   $changer1 . "," .
   $changer2 . "," .
   $changer3 . "," .
   $changer4 . "," .
   $changer5 . "," .
   $changer6 . "," .
   $changer7 . ",'" .
   $weatherDescription .
   "')";
  $loggingResult = mysqli_query($conn, $loggingSql);

}


//reads data from the cloud about our particular solar installation
function getCurrentSolarData($tenant) {
  Global $conn;
  $baseUrl = "https://www.solarkcloud.com";
  $mostRecentInverterRecord = getMostRecentInverterRecord($tenant);
  $date = new DateTime("now", new DateTimeZone('America/New_York'));//obviously, you would use your timezone, not necessarily mine
  $formatedDateTime =  $date->format('Y-m-d H:i:s');
  $nowTime = strtotime($formatedDateTime);
  $minutesSinceLastRecord = 10000;
  if($mostRecentInverterRecord){
    $lastRecordTime = strtotime($mostRecentInverterRecord["recorded"]);
    $minutesSinceLastRecord = round(abs($nowTime - $lastRecordTime) / 60,2);
  }
  //i decoupled the API call to pv.inteless.com from the API call made by the ESP8266s so that they won't create a DoS attack on pv.inteless.com if i have a lot of devices.  we only hit it once every five minutes now
  //var_dump($mostRecentInverterRecord);
  //echo $minutesSinceLastRecord;
  
  if(false && $minutesSinceLastRecord > 5) { //we don't need to get data from SolArk any more, but this is how you would
    $plantId = $tenant["energy_api_plant_id"];
    $url = $baseUrl . '/oauth/token';
    $headers = [
            'Content-Type: application/json;charset=UTF-8', // Set Content-Type header to application/json
          
    ];
    $params = [
            'client_id' => 'csp-web',
            'grant_type' => 'password',
            'password' => $tenant["energy_api_password"],
            'username' => $tenant["energy_api_username"],
 
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
    $actionUrl = $baseUrl . '/api/v1/plant/energy/' . $plantId  . '/day?date=' . $currentDate . "&id=" . $plantId . "&lan=en";
    $actionUrl = $baseUrl .'/api/v1/plant/energy/' . $plantId  . '/flow?date=' . $currentDate . "&id=" . $plantId . "&lan=en"; 
    $actionUrl = $baseUrl . '/api/v1/plant/energy/' . $plantId  . '/flow';
    $userParams =   [
            //'access_token' => $access_token,
            'date' => $currentDate,
            'id' => $tenant["energy_api_plant_id"],
            'lan' => 'en'         
    ];

    // Make GET request to user endpoint
    $queryString = http_build_query($userParams);
    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL, $actionUrl . "?" . $queryString);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HTTPHEADER, array(
      "Authorization: Bearer " . $access_token,  //had been sending the token in the querystring, but that stopped working the morning of April 9, 2024
      "Accept: application/json",
    ));

    $dataResponse = curl_exec($ch);
    curl_close($ch);

    $dataBody = json_decode($dataResponse, true);

    $data = $dataBody["data"];
    //var_dump($data);
    //if ($data["pvTo"] == true) { //this indicates we have real data!
      $loggingSql = "INSERT INTO inverter_log ( tenant_id, recorded, solar_power, load_power, grid_power, battery_percentage, battery_power) VALUES (";
      $loggingSql .= $tenant["tenant_id"] . ",'" . $formatedDateTime . "'," . $data["pvPower"] . "," . $data["loadOrEpsPower"] . "," . $data["gridOrMeterPower"] . "," . $data["soc"] . "," . $data["battPower"]    . ")";
      $loggingResult = mysqli_query($conn, $loggingSql);
    //}
    //echo $loggingSql;
    return Array("tenant_id"=>$tenant["tenant_id"], "recorded" => $formatedDateTime, "solar_power" => $data["pvPower"], "load_power" => $data["loadOrEpsPower"] ,
    "grid_power" =>  $data["gridOrMeterPower"], "battery_percentage" => $data["soc"], "battery_power" => $data["battPower"]); 
  } else {
    return $mostRecentInverterRecord;

  }
}

function getMostRecentInverterRecord($tenant){
  Global $conn;
  $sql = "SELECT * FROM inverter_log WHERE inverter_log_id = (SELECT MAX(inverter_log_id) FROM inverter_log WHERE tenant_id=" . $tenant["tenant_id"] . ") LIMIT 0,1";
	$result = mysqli_query($conn, $sql);
	if($result) {
		$row = mysqli_fetch_array($result, MYSQLI_ASSOC);
		return $row;
	}
}

function currentSensorData($tenant){
  Global $conn;
  $out = "";
  $sql = "SELECT * FROM inverter_log WHERE inverter_log_id = (SELECT MAX(inverter_log_id) FROM inverter_log WHERE tenant_id=" . $tenant["tenant_id"] . ") ";
  $result = mysqli_query($conn, $sql);
  $out .= "<div class='listheader'>Inverter </div>";
  $energyInfo = array(
    [
	    'label' => 'solar power',
      'name' => 'solar_power'
	  ],
    [
      'label' => 'load power',
      'name' => 'load_power' 
    ],
    [
      'label' => 'grid power',
      'name' => 'grid_power' 
    ],
    [
      'label' => 'battery percentage',
      'name' => 'battery_percentage' 
    ],
    [
      'label' => 'battery power',
      'name' => 'battery_power' 
    ],
    [
      'label' => 'battery voltage',
      'name' => 'battery_voltage' 
    ],
    [
      'label' => 'recorded',
      'name' => 'recorded',
      'function' => 'timeAgo("<recorded/>")'
    ]
  );
  if($result) {
    $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
    //var_dump($rows);
    if($rows) {
      $out.= genericTable($rows, $energyInfo, NULL, NULL, "inverter_log", "inverter_log_id", $sql);
    }
  }
  $sql = "SELECT
    device_log_id,
    d.last_poll,
      d.location_name,
      wd.temperature * 9 / 5 + 32 AS temperature,
      wd.pressure,
      wd.humidity,
      wd.gas_metric,
      wd.recorded
    FROM
      device_log wd
    JOIN
      device d ON wd.location_id = d.device_id
    JOIN (
      SELECT
          location_id,
          MAX(recorded) AS max_recorded
      FROM
          device_log
      GROUP BY
          location_id
    ) latest ON wd.location_id = latest.location_id AND wd.recorded = latest.max_recorded
    WHERE d.tenant_id = " . $tenant["tenant_id"];
  $result = mysqli_query($conn, $sql);
  $out .= "<div class='listheader'>Weather </div>";

  $weatherInfo = array(
    [
	    'label' => 'location',
      'name' => 'location_name'
	  ],
    [
      'label' => 'temperature',
      'name' => 'temperature' 
    ],
    [
      'label' => 'pressure',
      'name' => 'pressure' 
    ],
    [
      'label' => 'humidity',
      'name' => 'humidity' 
    ],
    [
      'label' => 'gas metric',
      'name' => 'gas_metric' 
    ],
    [
      'label' => 'recorded',
      'name' => 'recorded',
       'function' => 'timeAgo("<recorded/>")'
    ],
    [
      'label' => 'last poll',
      'name' => 'last_poll',
       'function' => 'timeAgo("<last_poll/>")'
    ],
    );



  if($result) {
    $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
    //var_dump($rows);
    if($rows) {
      $out.= genericTable($rows, $weatherInfo, NULL, NULL, "device_log", "device_log_id", $sql);
    }
  }
  return $out;
}

function managementRuleTools() {
  Global $conn;
  $out = "";
  $tables = array("inverter_log", "device_log");
  //genericSelect($id, $name, $defaultValue, $data, $event = "", $handler= "")
  $out.="\n<br/><div class='unfoldingtool'>\n";
  $out.="<div class='unfoldingtoolline'><span>Create a lookup token in conditions:</span>";
  $out.= "<div class='unfoldingtoolline' ><span>" . genericSelect("tableNameForManagementRule", "tableNameForManagementRule", "", $tables,"onchange", "managementRuleTableChange()") . "</span></div>";
  $out .= "\n<div class='unfoldingtoolline' id='mr_column'></div>";
  $out .= "\n<div class='unfoldingtoolline' id='mr_location'></div>";
  $out .= "\n<div class='unfoldingtoolline' id='mr_tag'></div>";
  $out .= "\n<div class='unfoldingtoolline' id='mr_button' style='display:none'><button onclick='managementConditionsAddTag()'>Insert Token</button></div>";
  $out .= "\n</div>\n";
  return $out;
}

function getWeatherDataByCoordinates($latitude, $longitude, $apiKey) {
  $baseUrl = "https://api.openweathermap.org/data/2.5/weather";
  $query = http_build_query([
      'lat' => $latitude,
      'lon' => $longitude,
      'appid' => $apiKey,
      'units' => 'metric' // Use 'imperial' for Fahrenheit
  ]);
  $url = "$baseUrl?$query";

  // Initialize a cURL session
  $ch = curl_init();

  // Set the URL and options
  curl_setopt($ch, CURLOPT_URL, $url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

  // Execute the cURL session
  $response = curl_exec($ch);

  // Check for errors
  if ($response === false) {
      $error = curl_error($ch);
      curl_close($ch);
      throw new Exception("cURL Error: $error");
  }

  // Close the cURL session
  curl_close($ch);

  // Decode the JSON response
  $weatherData = json_decode($response, true);

  // Check for JSON decode errors
  if (json_last_error() !== JSON_ERROR_NONE) {
      throw new Exception("JSON Decode Error: " . json_last_error_msg());
  }

  return $weatherData;
}



function getWeatherForecast($latitude, $longitude, $apiKey) {
  $baseUrl = "https://api.openweathermap.org/data/2.5/onecall";
  $query = http_build_query([
      'lat' => $latitude,
      'lon' => $longitude,
      'appid' => $apiKey,
      'units' => 'metric' // Use 'imperial' for Fahrenheit
  ]);
  $url = "$baseUrl?$query";

  // Initialize a cURL session
  $ch = curl_init();

  // Set the URL and options
  curl_setopt($ch, CURLOPT_URL, $url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

  // Execute the cURL session
  $response = curl_exec($ch);

  // Check for errors
  if ($response === false) {
      $error = curl_error($ch);
      curl_close($ch);
      throw new Exception("cURL Error: $error");
  }

  // Close the cURL session
  curl_close($ch);

  // Decode the JSON response
  $weatherData = json_decode($response, true);

  // Check for JSON decode errors
  if (json_last_error() !== JSON_ERROR_NONE) {
      throw new Exception("JSON Decode Error: " . json_last_error_msg());
  }

  return $weatherData;
}

function tenantListSql($user){
  if($user["role"] == "super") {
    $tenantListSql = "SELECT tenant_id, name as text FROM tenant ORDER BY name";
  } else {
    $tenantListSql = "SELECT t.tenant_id, name as text FROM tenant t JOIN tenant_user tu ON t.tenant_id=tu.tenant_id WHERE tu.user_id=" . intval($user["user_id"]) . " ORDER BY name";
  }
  return $tenantListSql;
}
function tablesThatRequireUser(){
  return ["device_feature"];
}

function tablesThatRequireModified(){
  return ["device_feature"];
}

//all the tables that implement templating
function templateableTables() {
  return ["feature_type", "device_type", "device_type_feature", "management_rule", "report"];
}

//all the tables of this system
function schemaTables() {
  return ["command", "command_type", "device", "device_feature", "device_feature_log", "device_feature_management_rule", "device_type", "device_type_feature", "feature_type", "inverter_log", "ir_pulse_sequence", "ir_target_type", "management_rule", "reboot_log", "report report_log", "tenant", "tenant_user", "user", "device_log"];
 
}

function utilities($user, $viewMode = "list") {
  Global $database;
  Global $backupLocation;
  $utilitiesData = array(
    [
      'label' => 'Recent Visitor Log',
      'url' => '?table=utilities&action=recentvisitorlogs',
      'description' => "Beats having to use putty.",
      'action' => 'visitorLog(<device_id/>, <number/>)',
      'key' => 'recentvisitorlogs',
      'role' => "super",
      'skip_confirmation' => true,
      'form' => 
      [
        [
          'label' => 'Number of Records',
          'name' => 'number',
          'value' => gvfa("tenant_id", $_POST),
          'type' => 'select',
          'values' => [10, 20, 40, 60, 100, 200, 500, 1000, 2000, 5000, 10000]
        ],
        [
          'label' => 'Location',
          'name' => 'device_id',
          'value' => gvfa("device_id", $_POST),
          'type' => 'select',
          'values' => "SELECT name as text, device_id FROM device WHERE tenant_id=<tenant_id/>"
        ]
      ]
    ]
    ,
    [
      'label' => 'Recent Error Log',
      'url' => '?table=utilities&action=recenterrorlogs',
      'description' => "Also beats having to use putty.",
      'action' => 'errorLog(<number/>)',
      'key' => 'recenterrorlogs',
      'role' => "super",
      'skip_confirmation' => true,
      'form' => 
      [
        [
          'label' => 'Number of Records',
          'name' => 'number',
          'value' => gvfa("tenant_id", $_POST),
          'type' => 'select',
          'values' => [10, 20, 40, 60, 100, 200, 500, 1000, 2000, 5000, 10000]
        ] 
      ]
    ]
    ,
    [
      'label' => 'Get Invitation Link to Create User for Tenant',
      'url' => '?table=utilities&action=tenantlink',
      'description' => "Send this link to someone to get them to become a specific tenant's user.",
      'action' => 'generateCurrentUrl() . "?table=user&action=startcreate&encrypted_tenant_id=" . siteEncrypt(<tenant_id/>)',
      'key' => 'tenantlink',
      'role' => "admin",
      'skip_confirmation' => true,
      'form' => 
      [
        [
          'label' => 'Tenant',
          'name' => 'tenant_id',
          'value' => gvfa("tenant_id", $_POST),
          'type' => 'select',
          'values' => tenantListSql($user)
        ] 
      ]
    ]
    ,
    [
      'label' => 'Copy Tenant Table Data as Template Values',
      'url' => '?table=utilities&action=copytotenanttemplate',
      'description' => "Copies user data from select tables to the 'template' tenant. Used to save data from an especially well-curated tenant to templates for the initiation of new tenants.",
      'key' => 'copytotenanttemplate',
      'role' => "super",
      'action' => "copyTenantToTemplates(<tenant_id/>, '<table_name/>')",
      'skip_confirmation' => false,
      'form' => 
      [
        [
          'label' => 'Tenant to Get Data From',
          'name' => 'tenant_id',
          'value' => gvfa("tenant_id", $_POST),
          'type' => 'select',
          'values' => "SELECT tenant_id, name as text from tenant ORDER BY name"
        ],
        [
          'label' => 'Tables',
          'name' => 'table_name',
          'value' => gvfa("table_name", $_POST),
          'type' => 'multiselect',
          'values' => templateableTables()
        ]

      ]
    ]
    ,
    [
      'label' => 'Copy Template Table Data to Tenant',
      'url' => '?table=utilities&action=copyfromtenanttemplate',
      'description' => "Copies template data from select tables to a specific tenant.  Used as part of tenant setup or when rebuilding a tenant from scratch.",
      'key' => 'copyfromtenanttemplate',
      'role' => "admin",
      'action' => "copyTemplatesToTenant(<tenant_id/>, '<table_name/>')",
      'skip_confirmation' => false,
      'form' => 
      [
        [
          'label' => 'Tenant Data to Overwrite',
          'name' => 'tenant_id',
          'value' => gvfa("tenant_id", $_POST),
          'type' => 'select',
          'values' => tenantListSql($user)
        ],
        [
          'label' => 'Tables',
          'name' => 'table_name',
          'value' => gvfa("table_name", $_POST),
          'type' => 'multiselect',
          'values' => templateableTables()
        ]

      ]
    ]
    ,
    [
      'label' => 'Build Database Initialization Script',
      'url' => '?table=utilities&action=buildinitializationscript',
      'description' => "Using the present schema and template data, build an initial SQL install script.",
      'key' => 'buildinitializationscript',
      'role' => "super",
      'action' => "sqlInitializationScript()",
      'skip_confirmation' => false
    ]
    ,
    [
      'label' => 'Backup Database',
      'url' => '?table=utilities&action=backupdatabase',
      'description' => "Do a complete backup.",
      'key' => 'backupdatabase',
      'role' => "super",
      'action' => "backupDatabase()",
      'skip_confirmation' => false
    ]
    ,
    [
      'label' => 'Download Database',
      'url' => '?table=utilities&action=downloaddatabase',
      'description' => "Download the latest database backup.",
      'key' => 'downloaddatabase',
      'role' => "super",
      'path' => $backupLocation . "/" . $database . ".sql",
      'friendly_name' => $database . "_" . date('Y-m-d_His') . ".sql",
      'action' => "",
      'skip_confirmation' => false,
      'output_format' => 'download'
    ]
  );

 
 

  $filteredData = array_filter($utilitiesData, function ($subData) use ($user) {
    return canUserDoThing($user, gvfa("role", $subData));
    //return doesUserHaveRole($tenant, gvfa("role", $subData));
  });
  //echo $viewMode;
  //var_dump($filteredData );
  if($viewMode == "data"){
    return $filteredData;
  }
  if($viewMode == "list"){
 
    $out = presentList($filteredData);
  } else if ($viewMode == "form") {

    die();
  }
  
  return $out;
      
}

//tables is a comma-delimited string
function copyTenantToTemplates($tenantId, $tablesString){
  Global $conn;
  Global $database;
  $date = new DateTime("now", new DateTimeZone('America/New_York'));//obviously, you would use your timezone, not necessarily mine
  $formatedDateTime =  $date->format('Y-m-d H:i:s');
  $tables = explode(",", $tablesString);
  foreach($tables as $currentTableName) {
    $currentTableName = trim($currentTableName);
    $columnSql = "
    SELECT GROUP_CONCAT(COLUMN_NAME) AS columns
    FROM INFORMATION_SCHEMA.COLUMNS
      WHERE table_schema='" . $database . "' AND table_name='" . $currentTableName . "' AND COLUMN_KEY != 'PRI' AND COLUMN_NAME != 'created' AND COLUMN_NAME != 'tenant_id'";
    //echo $columnSql . "<P>";
    $result = $conn->query($columnSql);
    $row = $result->fetch_assoc();
    $columnString = "`" . str_replace(",", "`,`", $row["columns"]) . "`";
    $deleteSql = "DELETE FROM " . $currentTableName . " WHERE tenant_id=0";
    if(strpos($columnString, "templateable") !== false){
      $deleteSql .= " AND templateable = 1 ";
    }
    $sql = "INSERT INTO " . $currentTableName . "(" . $columnString  . ",created,tenant_id)";
    $sql .= " SELECT " . $columnString . ",'" . $formatedDateTime . "', 0 FROM " . $currentTableName . " WHERE tenant_id=" . intval($tenantId);
    if(strpos($columnString, "templateable") !== false){
      $sql .= " AND templateable = 1 ";
    }
    //echo $sql;
    $result = $conn->query($deleteSql);
    $result = $conn->query($sql);
    $error = mysqli_error($conn);
  }
  if(!$error){
    return "Template for tables: " . $tablesString  . " updated.";
  } else {
    return $error;
  }
}

//tables is a comma-delimited string
function copyTemplatesToTenant($tenantId, $tablesString){
  Global $conn;
  Global $database;
  $date = new DateTime("now", new DateTimeZone('America/New_York'));//obviously, you would use your timezone, not necessarily mine
  $formatedDateTime =  $date->format('Y-m-d H:i:s');
  $tables = explode(",", $tablesString);
  foreach($tables as $currentTableName) {
    $currentTableName = trim($currentTableName);
    $columnSql = "
    SELECT GROUP_CONCAT(COLUMN_NAME) AS columns
    FROM INFORMATION_SCHEMA.COLUMNS
      WHERE table_schema='" . $database . "' AND table_name='" . $currentTableName . "' AND COLUMN_KEY != 'PRI' AND COLUMN_NAME != 'created' AND COLUMN_NAME != 'tenant_id'";
 
    $result = $conn->query($columnSql);
    $row = $result->fetch_assoc();
    $columnString = "`" . str_replace(",", "`,`", $row["columns"]) . "`";
    $deleteSql = "DELETE FROM " . $currentTableName . " WHERE tenant_id=" . intval($tenantId);
    if(strpos($columnString, "templateable") !== false){
      $deleteSql .= " AND templateable = 1 ";
    }
    $sql = "INSERT INTO " . $currentTableName . "(" . $columnString  . ",created,tenant_id)";
    $sql .= " SELECT " . $columnString . ",'" . $formatedDateTime . "', " . intval($tenantId). " FROM " . $currentTableName . " WHERE tenant_id=0";
    if(strpos($columnString, "templateable") !== false){
      $sql .= " AND templateable = 1 ";
    }
    //echo $sql;
    $result = $conn->query($deleteSql);
    $result = $conn->query($sql);
    $error = mysqli_error($conn);
    if(!$error){
      return "Tenant tables: " . $tablesString  . " restored from template.";
    } else {
      return $error;
    }
  }
 
}


function visitorLog($deviceId, $number) {
  $lines=array();
  $logFile = "/var/log/apache2/access.log";
  if($deviceId){
    $strToExec = "grep \"locationId=" . $deviceId . "\" " . $logFile . " | tail -n " . $number;
  } else {
    $strToExec = "cat " . $logFile . " | tail -n " . $number;
  }
  
  $output = shell_exec($strToExec);
  return $output;

}


function errorLog($number) {
  $lines=array();
  $logFile = "/var/log/apache2/error.log";
  $strToExec = "cat " . $logFile . " | tail -n " . $number;
  $output = shell_exec($strToExec);
  return $output;

}


function sqlInitializationScript() {
  Global $username;
  Global $password;
  Global $database;
  Global $backupLocation;
  $backupPath = $backupLocation . "/";
  $strToExec = "./sql_backup.sh " . $username . " " . $password . " " . $database . " " . $backupPath .  " \"" . implode(' ', schemaTables())  . "\" \"" . implode(' ', templateableTables())  . "\"";
  //echo  $strToExec ;
  $output = shell_exec($strToExec);
  return $output;

}


function backupDatabase() {
  Global $username;
  Global $password;
  Global $database;
  Global $backupLocation;
  $backupLoc = $backupLocation . "/" . $database . ".sql";
  if (file_exists($backupLoc)) {
    unlink($backupLoc);
  }
  $strToExec = " ./full_sql_backup.sh  " .  $username . "  " . $password . " " . $database . " " . $backupLoc;
  $output = shell_exec($strToExec);
  $fileSize = filesize($backupLoc);
  return "The database <b>" . $database . "</b> was backed up.  It came to " . formatBytes($fileSize) . ".";
}




 
