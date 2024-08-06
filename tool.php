<?php 
//server-based remote controller using microcontrollers 
//gus mueller, March 18 2024
//////////////////////////////////////////////////////////////
//troubleURLs:  
 
 

ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);

include("config.php");
include("site_functions.php");
include("device_functions.php");
//var_dump($_POST);
 

$conn = mysqli_connect($servername, $username, $password, $database);

$table = strtolower(filterStringForSqlEntities(gvfw('table', "device"))); //make sure this table name doesn't contain injected SQL
$action = strtolower(gvfw('action', "list"));
$user = autoLogin();
$tenantId = gvfa("tenant_id", $user);
 
$deviceId = gvfa('device_id', $_GET);
$userId = gvfa("user_id", $user);

//used to impersonate another user

$poser = getImpersonator(false);
if ($poser) {
  $userId = $poser["user_id"];
}
$out = "";
$errors = NULL;
$tenantSelector = "";
 
 
$page = gvfw('page');
$datatype = gvfw('datatype'); 
//echo $table  . "*" . $action. "*" .  $datatype;
//$formatedDateTime =  $date->format('H:i');


if ($action == "logout") {
  logOut();
  header("Location: ?action=login");
  die();
} else if ($action == 'settenant') {
  setTenant(gvfw("encrypted_tenant_id"));
} else if  ($action == "disimpersonate") {
  disImpersonate();
  header("Location: .");
  die();
}


if($_POST || gvfw("table")) { //gvfw("table") 


	if ($action == "login") {
    $tenantId = gvfa("tenant_id", $_GET);
		$tenantSelector = loginUser(NULL, $tenantId);
  } else if (strtolower($action) == "create user"  && array_key_exists("password2", $_POST)) {
    $encryptedTenantId = gvfw("encrypted_tenant_id");
		$errors = createUser($encryptedTenantId); //only use this for self-creates
	}  else if (strtolower($table) == "run") {
    //oh, we're a utility, though we never get here
  } else if(beginsWith(strtolower($action), "save") || beginsWith(strtolower($action), "create")) {
    if($user["role"] != "viewer" && ($table != "user"  &&  $table != "report") || $user["role"] == "super") {
      $errors = genericEntitySave($tenantId, $table);
    } else {
      $out.= "You lack permissions to make changes to a " . $table . ".";
    }
  }
} else {
 
}
 
if ($user) {

	$out .= "<div>\n";
  if($action == "checksqlsyntax") {
    $sql = gvfw('sql');
    $out = json_encode(checkMySqlSyntax($sql));
    die( $out);
  } else if($action == "checkjsonsyntax") {
      $sql = gvfw('json');
      $out = json_encode(checkJsonSyntax($sql));
      die($out);
  } else if($action == "getdevices") {
    $out = getDevices($tenantId);
    die(json_encode($out));
  } else if($action == "getcolumns") {
    $out = getColumns($table);
    die(json_encode($out));
  } else if($action == "genericformsave") { //this should be pretty secure now that i am hashing all the descriptive information to make sure it isn't tampered with
    //mostly works for numbers, colors, and checkboxes

    $name = filterStringForSqlEntities(gvfw('name'));
    $value = gvfw('value');
    $primaryKeyName = filterStringForSqlEntities(gvfw('primary_key_name'));
    $primaryKeyValue = gvfw('primary_key_value');
    $hashedEntities = gvfw('hashed_entities');
    $whatHashedEntitiesShouldBe =  crypt($name . $table .$primaryKeyName  . $primaryKeyValue , $encryptionPassword);
    if($hashedEntities != $whatHashedEntitiesShouldBe){
      echo $hashedEntities. " " . $whatHashedEntitiesShouldBe . "\n";
      die("Data appears to have been tampered with.");
    }
    if($value == "false"){
      $value = 0;
    } else if($value == "true"){
      $value = 1;
    }
    //a little safer only because it allows a user to screw up records connected to their tenantId but mabe revisit!!!
    $sql = "UPDATE ". filterStringForSqlEntities($table) . " SET " . filterStringForSqlEntities($name) . "='" .  mysqli_real_escape_string($conn, $value) . "' WHERE tenant_id=" . intval($tenantId) . " AND " . filterStringForSqlEntities($primaryKeyName) . "='" . intval($primaryKeyValue) . "'";
    if($user["role"] != "viewer" && ($table != "user"  &&  $table != "report") || $user["role"] == "super") { //can't have just anybody do this
      $result = mysqli_query($conn, $sql); 
    }
    var_dump($result);
    die($sql);
  
  } else if ($action == "runencryptedsql") { //this is secure because the sql is very hard to decrypt if you don't know the encryption key
 
    $headerData = json_decode(gvfw('headerData'), true);
    $currentSortColumn = gvfw('sortColumn');
    $sortAddendum = "";
    if($currentSortColumn  > -1){
      $sortDatum = $headerData[$currentSortColumn];
      $sortAddendum = " ORDER BY ". $sortDatum["name"];
      $direction = gvfw('direction');
      if($direction == "true"){
        $sortAddendum .= " DESC";

      }
    }
 
    $sql = decryptLongString(gvfw('value'), $encryptionPassword);
    $sql .= $sortAddendum  ;
    //echo $sql . "\n\n";
    //die();
    $result = mysqli_query($conn, $sql);
    if($result) {
      $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
      echo json_encode($rows);
      die();
    }
    die();
  }
  if ($table == "sensors") {//a pseudo-table, as sensors are either one-to-a-device or a device_feature
    $out .= currentSensorData($user);
  } else if ($table == "utilities") {

 
    $action = gvfa('action', $_GET); //$_POST will have this as "run"
    
    $data = json_decode(gvfw("_data")); //don't actually need this
    $foundData = getUtilityInfo($user, $action); 
 
    if($foundData) {
      $role = gvfa("role", $foundData);
 


      if ($_POST ||  gvfa("action", $foundData) && !gvfa("form", $foundData)) {
        //dealing with a utility that has a form
        $role = gvfa("role", $foundData);
        if (canUserDoThing($user, $role) && $action) { //don't actually need to do this here any more
          if(array_key_exists("sql", $foundData)) { //allows the guts of a report to work as a utility

            $sql = $foundData["sql"];
            $sql =  tokenReplace($sql, $_POST);
            $reportResult = mysqli_query($conn, $sql);
            $error = mysqli_error($conn);
            $affectedRows = mysqli_affected_rows($conn);
            $out .= "<pre>Affected rows: " . $affectedRows . "\n";
            if($error){
              $out .= "Error: " . $error . "\n";
            }
            
            $out .= "</pre>";
            if($reportResult !== false  && $reportResult !== true) {
              $rows = mysqli_fetch_all($reportResult, MYSQLI_ASSOC);
              $out .= genericTable($rows, null, null, null);
            }

          } else if(array_key_exists("action", $foundData)) {
              $redirect = true;
              $codeToRun = tokenReplace($foundData["action"], $_GET);
              $codeToRun = tokenReplace($codeToRun, $_POST) . ";";
 
              try {

                //$result = @eval($evalcode . "; return true;");
                
                //die(var_dump($result));
                //die($result);
                //if($result){
                  eval('$result  =' . $codeToRun . ";");
                //}
                $out .= "<pre>" . $result . "</pre>";
                //die();
                //die($codeToRun);
              }
              catch(ParseError $error) { //this shit does not work. does try/catch ever work in PHP?
                //var_dump($error);
                $errors["generic"] = "Data is missing for this utility.";
                $redirect = false;
              }
              if($redirect){
                //header('Location: '.$_SERVER['PHP_SELF'] . "?mode=" . $mode);
                //die();
              }
          }
        }
 
        
      } else if ($action) {
 
        $out .= utilityForm($user, $foundData);
      }
   }  
   
    
   if(!$action) {

    $out .= utilities($user, "list");
   } else if(!$foundData && false) {
    $out .= "<div class='generalerror'>Utility not yet developed.</div>";
   }
  } else if ($action == "delete") {

    $sql = "DELETE FROM " . $table . " WHERE " . $table . "_id='" . intval(gvfw( $table . "_id")) . "'";
    if($table != "user") {
      $sql .= " AND tenant_id='" . $tenantId . "'";
    }
    //die($sql);
    $hashedEntities = gvfw('hashed_entities');
    $whatHashedEntitiesShouldBe =  crypt($table .$table . "_id"  . intval(gvfw( $table . "_id")) , $encryptionPassword);
    if($hashedEntities != $whatHashedEntitiesShouldBe){
      die("Data appears to have been tampered with.");
    }
    $result = mysqli_query($conn, $sql);
    header('Location: '.$_SERVER['PHP_SELF'] . "?table=" . $table);
  } elseif($action == "json"){
    if($table!= "user" || $user["role"]  == "super") {
      $sql = "SELECT * FROM " .  $table  . " WHERE " . $table . "_id='" . intval(gvfw( $table . "_id")) . "'";
      if($table != "tenant") {
        $sql .= " AND tenant_id='" . $tenantId . "'";
      } 
      $result = mysqli_query($conn, $sql);
      $valueArray = mysqli_fetch_assoc($result);
      die(json_encode($valueArray, JSON_FORCE_OBJECT));
    }
  } elseif($action == "log") {
      if($table == "device_feature"){
        $out .= deviceFeatureLog(gvfw($table . '_id'), $tenantId);
      }
  //this is the section for conditionals related to specially-written editors and listers
	} else if($table == "report") {
    if ($action == "rerun" || $action == "fetch" || beginsWith(strtolower($action), "run")) {
      $out .= doReport($user, gvfw("report_id"), gvfw("report_log_id"));
    
    } else if ($action == "startcreate" || gvfw("report_id") != "") {
      $out .=  editReport($errors,  $tenantId);
    } else {
      $out .= reports($tenantId, $user);
    }
	} else if($table == "device") {
    if ($action == "startcreate" || gvfw("device_id") != "") {
      $out .=  editDevice($errors,  $tenantId);
    } else {
     $out .= devices($tenantId);
    }
	} else if($table == "device_feature") {
    if ($action == "startcreate" || gvfw("device_feature_id") != "") {
      $out .=  editDeviceFeature($errors,  $tenantId);
    } else {
      $out .= deviceFeatures($tenantId, $deviceId);
    }
	} else if($table == "management_rule") {
    if ($action == "startcreate" || gvfw("management_rule_id") != "") {
      $out .=  editManagementRule($errors,  $tenantId);
    } else {
      $out .= managementRules($tenantId, $deviceId);
    }
	} else if($table == "user") {
    if ($action == "startcreate" || gvfw("user_id") != "") {
      $out .=  editUser($errors);
    } else {
      if($user["role"]  == "super"){
        $out .= users();
      }
    }
	} else if($table == "tenant") {
    
    if ($action == "startcreate" || gvfw("tenant_id") != "") {
      $out .=  editTenant($errors, $user);
    } else {
      if($user["role"]  == "super" || $user["role"]  == "admin"){
        $out .= tenants($user);
      }
    }
  } else if ($action == "startcreate") {
    $out .= genericEntityForm($tenantId, $table, $errors);
  } else if($table!= "user" || $user["role"]  == "super") {
    if(gvfw($table . '_id')) {
      $out .= genericEntityForm($tenantId, $table, $errors);
    } else if($table) {
      $out .= genericEntityList($tenantId, $table);
    }
  }
	$out .= "</div>\n";
	$out .= "<div>\n";
	$out .= " "; 
	$out .= "</div>\n";

} else {
 
  if ($table == "user" || !is_null($errors)) {
    $encryptedTenantId = gvfw("encrypted_tenant_id");
    $out .= newUserForm($errors, $encryptedTenantId);
  } else if(gvfa("password", $_POST) != "") {
 
    if(!$tenantSelector){
      $out .= "<div class='genericformerror'>The credentials you entered have failed.</div>";
    }
   }
  if($action != "startcreate"){
    if(!$tenantSelector) {
      $out .= loginForm();
    } else {
      $out .= $tenantSelector;
    }
  }
}


echo bodyWrap($out, $user, $deviceId, $poser);
