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
 
normalizePostData();

$conn = mysqli_connect($servername, $username, $password, $database);

$table = strtolower(filterStringForSqlEntities(gvfw('table', "device"))); //make sure this table name doesn't contain injected SQL
if($table == "etc"){
  $table = "device_type_feature";
}

$action = strtolower(gvfa("action", $_POST, gvfw('action', "list")));
//echo $action;
$user = autoLogin();
$tenantId = gvfa("tenant_id", $user);
 
$deviceId = gvfw('device_id');
$userId = gvfa("user_id", $user);
$outputFormat = gvfw("output_format");
$forceUpdate = gvfw("_forceupdate");
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
$date = new DateTime("now", new DateTimeZone($timezone));//$timezone is set in config.php
$formattedDateTime = $date->format('Y-m-d H:i:s');


if(strpos($action, "password") !== false) {
  $email = gvfw("email");
  $token = gvfw("token");
  if($email  &&  $token == ""){
    if(sendPasswordResetEmail($email)) {
      $out = "A password reset email was sent.  Check your email.";
    } else {
      $out = "Reset email could not be sent. Complain to the admin if you can somehow.";
    }
  } else {
    
    if($token){
      $userPassword = gvfw("password");
      $userPassword2 = gvfw("password2");
      if($userPassword) {
        //update the password
        if($userPassword != $userPassword2){
          $errors = [];
          $errors["password"] = "Your passwords must be identical.";
        } else {
          updatePasswordOnUserWithToken($email, $userPassword, $token);
          header("Location: ?action=login");
          die();
        }
      } 
      if($errors || $userPassword =="") {
        $out = changePasswordForm($email, $token, $errors);
      }
    } else {
      $out = forgotPassword();
    }
  }
} else if ($action == "logout") {
  logOut();
  header("Location: ?action=login");
  die();
} else if ($action == 'settenant') {
  setTenant(gvfw("encrypted_tenant_id"));
} else if  ($action == "disimpersonate") {
  disImpersonate();
  header("Location: .");
  die();
} else if ($action == "manifest") {
  $startUrl = gvfw("url");
  $icon = gvfw("icon");
  $bgColor = gvfw("bgcolor");
  $themeColor = gvfw("themecolor");
  $name = gvfw("name");
  $shortName = gvfw("shortname");
  manifestJson($name, $shortName, $startUrl, $icon, $bgColor, $themeColor);
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
      $errors = genericEntitySave($user, $table, $forceUpdate);
    } else {
      $out.= "You lack permissions to make changes to a " . $table . ".";
    }
  }
} else {
 
}
 
if ($user) {

	$out .= "<div>\n";
  if($action == "commandpoll"){
    $possibleTemporaryCommandFileName = "instant_response_" . $deviceId . ".txt";
    //if(file_exists($possibleTemporaryCommandFileName)){
    $sql = "SELECT MAX(result_recorded) as result_recorded FROM command_log WHERE tenant_id=" . intval($tenantId);
    if($deviceId > 0) {
      $sql .= " AND device_id=" . intval($deviceId);
    }
    $result = mysqli_query($conn, $sql);
    $row = mysqli_fetch_array($result);
    if($row["result_recorded"] > gvfw("result_recorded")) {
      //$temporaryComandText = file_get_contents($possibleTemporaryCommandFileName);
      //$commandText = getNumberAfterLastNewline($temporaryComandTeFoxt, true);
      //$commandLogId = getNumberAfterLastNewline($temporaryComandText, false);
      //$sql = "UPDATE command_log SET result_text='" . mysqli_real_escape_string($conn, $temporaryComandText) . "' WHERE tenant_id=". intval($tenantId) . "  AND device_id=" . intval($deviceId);
      //$sql .= " AND command_log_id=" . $commandLogId;
      
      //$result = replaceTokensAndQuery($sql, $user);
      //die($sql);
      echo '{"status": "new"}';
      //unlink($possibleTemporaryCommandFileName);
    } else {
      echo '{"status": "none"}';
    }
    
    die();
  }
  if($action == "checksqlsyntax") {
    $sql = gvfw('sql');
    $out = json_encode(checkMySqlSyntax($sql));
    die($out);
  } else if($action == "cancelinstantcommand") {
    $commandLogId = gvfw('command_log_id');
    $out = cancelInstantCommand($commandLogId, $formattedDateTime);
    die(json_encode($out));
  } else if($action == "valueexistselsewhere") {
    $value = gvfw('value');
    $pkName = gvfw('pk_name');
    $pkValue = gvfw('pk_value');
    $columnName = gvfw('column_name');
    $nameColumnName = gvfw('name_column_name');
    if(!$nameColumnName){
      $nameColumnName = "name";
    }
    //valueExistsElsewhere($table, $value, "digest_bit_position", $table . "_id", $pk, $tenantId)
    $record = valueExistsElsewhere($table, $value, $columnName, $table . "_id", $pkValue, $tenantId);
    die(json_encode($record));
    if($record) {
      //var_dump($record);
      $error[$columnName] = "Value already used in " . $record[$nameColumnName] . ".";//revisit!  name not always correct!
    }
  } else if($action == "checkjsonsyntax") {
      $sql = gvfw('json');
      $out = json_encode(checkJsonSyntax($sql));
      die($out);
  } else if($action == "sendmessage") {
    $content = gvfw("content");
    $targetDeviceId = gvfw("target_device_id");
    if(!$targetDeviceId){
      $targetDeviceId = "NULL";
    } else {
      $targetDeviceId = intval($targetDeviceId);
    }
    $sql = "INSERT INTO message(recorded, user_id, tenant_id, target_device_id, content) VALUES ('" . $formattedDateTime . "'," . $user["user_id"] . "," . $user["tenant_id"] . "," . $targetDeviceId . ",'" . mysqli_real_escape_string($conn, $content) . "')";
    $result = mysqli_query($conn, $sql);
    die("{}");
  } else if($action == "getdevices") {
    $out = getDevices($tenantId);
    die(json_encode($out));
  } else if($action == "getcolumns") {
    $out = getColumns($table);
    die(json_encode($out));
  } else if($action == "getrecordsfromassociatedtable") {
    $commandTypeId = gvfw('command_type_id');
    $out = getAssociatedRecords($commandTypeId, $user["tenant_id"]);
    die(json_encode($out));  
  } else if($action == "genericformsave") { //this should be pretty secure now that i am hashing all the descriptive information to make sure it isn't tampered with
    //mostly works for numbers, colors, and checkboxes
    
    $name = filterStringForSqlEntities(gvfa('name', $_POST));
    $value = gvfa('value', $_POST);
    $primaryKeyName = filterStringForSqlEntities(gvfa('primary_key_name', $_POST), false);
    $primaryKeyValue = gvfa('primary_key_value', $_POST);
    $hashedEntities = gvfa('hashed_entities', $_POST);
    $table = gvfa('table', $_POST);
    $tableSpec = schemaArrayFromSchema($table, $knownPk);
    $componentsOfHash = $table . $primaryKeyName . emptyIfNull($primaryKeyValue);
    //echo "***" . $componentsOfHash . "\n<br>";
    $extraForInsert = gvfa('extra_for_insert', $_POST);
    $whatHashedEntitiesShouldBe =  hash_hmac('sha256', $componentsOfHash, $encryptionPassword);
    if($hashedEntities != $whatHashedEntitiesShouldBe){
      echo $hashedEntities. " " . $whatHashedEntitiesShouldBe . "\n";
      die("Data appears to have been tampered with (2).");
    }
    //echo $value . "\n";
    if($value === false){
      $value = "0";
    } else if($value === true){
      $value = "1";
    }
    $pkSpecString = filterStringForSqlEntities($primaryKeyName, false) . "='" . intval($primaryKeyValue) . "'"; //this only works for singular pks
    if(count($knownPk)>1) { //ugh, this sure is ugly. but it allows me to edit data in a mapping table unrelated to the actual mapping
      $pkVals = explode("-", $primaryKeyValue);
      $pkKeys = explode("-", $primaryKeyName);
      $pkSpecString  = "";
      $pkIndex = 0;
      foreach($pkKeys as $pkKey) {
        $pkVal = $pkVals[$pkIndex];
        $pkSpecString.= filterStringForSqlEntities($pkKey, false) . "='" . intval($pkVal) . "' AND ";
        $pkIndex++;
      }
      $pkSpecString = substr($pkSpecString, 0, -4);
    }
    //echo $value . "\n";
    //a little safer only because it allows a user to screw up records connected to their tenantId but mabe revisit!!!
    $userClause = "";
    if(in_array($table, tablesThatRequireUser())){
      $userClause = ", user_id=" . $user["user_id"];
    }
    if(in_array($table, tablesThatRequireModified())){
      $userClause = ", modified='" . $formattedDateTime . "'";
    }
    if($extraForInsert && !$primaryKeyValue) {
      $initialExtraArray = explode(",", $extraForInsert);
      $extraColumns = "";
      $extraValues = "";
      foreach($initialExtraArray as $extraItem) {
        $extraParts = explode(":", $extraItem);
        $extraColumns .= filterStringForSqlEntities($extraParts[0]) . ",";
        $particularValue = $extraParts[1];
        $particularValue = str_replace("<now/>", $formattedDateTime, $particularValue);
        $extraValues .= "'" . mysqli_real_escape_string($conn, $particularValue) . "',";
      }
      $extraColumns = substr($extraColumns, 0, -1);
      $extraValues = substr($extraValues, 0, -1);
      
      $extraArray = explode(":", $extraForInsert);
      $sql = "INSERT INTO ". filterStringForSqlEntities($table, true) . "(" . $extraColumns ."," .  filterStringForSqlEntities($name) . ",tenant_id) VALUES(" . $extraValues  .",'" . mysqli_real_escape_string($conn, $value) . "','" . intval($tenantId) ."')";
    } else {
      $sql = "UPDATE ". filterStringForSqlEntities($table, true) . " SET " . filterStringForSqlEntities($name, true) . "='" .  mysqli_real_escape_string($conn, $value) . "' " . $userClause . " WHERE tenant_id=" . intval($tenantId) . " AND " . $pkSpecString;
    }
    if($user["role"] != "viewer" && ($table != "user"  &&  $table != "report") || $user["role"] == "super") { //can't have just anybody do this
      //echo "$$$$";
      $result = mysqli_query($conn, $sql); 
    } else {
      echo "You lack permissions.<br/>";
    }
    //echo $sql;
    if(!$result)
    {
      echo $sql;
    } else {
      //var_dump($result);
    }
    die();
  
  } else if ($action == "runencryptedsql") { //this is secure because the sql is very hard to decrypt if you don't know the encryption key
 
    $headerData = json_decode(gvfw('headerData'), true);
    
    $currentSortColumn = gvfw('sortColumn');
    $sortClause = "";
    if($currentSortColumn  > -1){
      $sortDatum = $headerData[$currentSortColumn];
      $sortClause = $sortDatum["name"];
      $direction = gvfw('direction');
      if($direction == "true"){
        $sortClause .= " DESC";
      }
    }
    $sql = decryptLongString(gvfw('value'), $encryptionPassword);
    $sql = setOrderByClause($sql, $sortClause);
    //echo $sql . "\n\n";
    //die();
    $result = replaceTokensAndQuery($sql, $user);
    if($result) {
      $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
      foreach ($rows as &$row) {
          $componentsOfHash = gvfw("table") . gvfw("pk") . $row[gvfw("pk")];
          $row['_components'] = $componentsOfHash;
          $row['_hashed_entities'] = hash_hmac('sha256', $componentsOfHash, $encryptionPassword);
      }
      unset($row); 
      echo json_encode($rows);
      die();
    } else {
      die($sql);
    }
    die();
  }
  if ($table == "sensors") {//a pseudo-table, as sensors are either one-to-a-device or a device_feature
    $out .= currentSensorData($user);
  } else if ($table == "utilities") {
    $action = gvfa('action', $_GET); //$_POST will have this as "run"
    
    $data = json_decode(gvfw("_data")); //don't actually need this
    $foundData = getUtilityInfo($user, $action); 
 
    if ($foundData) {
      $role = gvfa("role", $foundData);
      $path = gvfa("path", $foundData);
      $friendlyName = gvfa("friendly_name", $foundData);
      $outputFormat = gvfa("output_format", $foundData);
      if($_POST && (gvfa("action", $foundData) || $outputFormat)) { // ||  (gvfa("action", $foundData) && gvfa("form", $foundData))
        $out .= "<div class='issuesheader'>" .  gvfa("label", $foundData)  . "</div>";
        //dealing with a utility that has a form
        $role = gvfa("role", $foundData);
        if (canUserDoThing($user, $role) && ($action || $outputFormat)) { //don't actually need to do this here any more
          if (array_key_exists("sql", $foundData)) { //allows the guts of a report to work as a utility
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
          } else if(array_key_exists("action", $foundData) || $outputFormat) {
              $redirect = true;
              $codeToRun = "";
              if($foundData["action"]) {
                $codeToRun = tokenReplace($foundData["action"], $_GET);
                $codeToRun = tokenReplace($codeToRun, $_POST) . ";";
                $codeToRun = tokenReplace($codeToRun, $user) . ";";
              }
              //echo $codeToRun ;
              if(strpos($codeToRun, ">") !== false || checkPhpFunctionCallIsBogus($codeToRun)){
                $errors["generic"] = "You must enter all the necessary data in the utility form.";
                $out .= "Error: " . $errors["generic"]  . "\n";
              } else {
                try {
                  if($codeToRun) {
        
                    eval('$result  =' . $codeToRun . ";");
                  }
                  if($outputFormat == "download") {
                    //die($path . "*" . $friendlyName);
                    //die(file_exists($path) . "&");
                    download($path, $friendlyName);
                    die();
                  } else {
                    $out .= "<pre>" . $result . "</pre>";
                  } 
                }
                catch(ParseError $error) { //this shit does not work. does try/catch ever work in PHP?
                  //var_dump($error);
                  $errors["generic"] = "Data is missing for this utility.";
                  $redirect = false;
                }
              }
              if($redirect){
                //header('Location: '.$_SERVER['PHP_SELF'] . "?mode=" . $mode);
                //die();
              }
          }
        }
      } else if ($action) {
        if (!gvfa("form", $foundData) && !gvfa("front_end_js", $foundData)){
          $out .= "<div class='issuesheader'>" .  gvfa("label", $foundData)  . "</div>";
          $out .= "<form method='post'><input name='run' type='submit' value='run'/></form>"; //make a quick and dirty form
        } else {
          $out .= utilityForm($user, $foundData);
        }
      }
   }  

   if(!$action) {
    $out .= utilities($user, "list");
   } else if(!$foundData && false) {
    $out .= "<div class='generalerror'>Utility not yet developed.</div>";
   }
  } else if ($action == "delete") { //this version hardened against CSCRF
    $pkName = $_POST["primary_key_name"];
    $pkValue = $_POST["primary_key_value"];
    $table = $_POST["table"];
    $sql = "DELETE FROM " . $table . " WHERE " . $pkName  . "='" . intval($pkValue) . "'";
    if($table != "user") {
      $sql .= " AND tenant_id='" . $tenantId . "'";
    }
    
    $hashedEntities = gvfw('hashed_entities');
    $whatHashedEntitiesShouldBe =  hash_hmac('sha256', $table . $pkName . $pkValue, $encryptionPassword);
    //die( $table . $pkName . $pkValue . "*" . $hashedEntities . "*" . $whatHashedEntitiesShouldBe);
    if($hashedEntities != $whatHashedEntitiesShouldBe){
      die("Data appears to have been tampered with (1).");
    }
    //echo "SUCCESS";
    //die($sql);
    $result = mysqli_query($conn, $sql);
    //header('Location: '.$_SERVER['PHP_SELF'] . "?table=" . $table . "&device_id=" . $deviceId);
  } elseif($action == "json"){
    if ($table!= "user" || $user["role"]  == "super") {
      $name = filterStringForSqlEntities(gvfw('name')); 
      if($name == "") {
        $name = "*";
      }
      $pkPhrase = "";
      $tableSpec = schemaArrayFromSchema($table, $pk);
      $userForSpec = $user;
      if($table == "tenant") {
        $userForSpec  = null;
      }
      $pkSpec = populateDictionaryFromSource($pk, $_GET, $userForSpec);
      $pkPhrase = pkSpecToWhereClause($pkSpec);
      $primaryKeyName = implode("-", $pk); //how we handle composite pks in a way that allows the old single-pk-only system to work
      $pkValue = implode("-", array_values($pkSpec)); 
      //var_dump($pkValue);
      //$primaryKeyName = $table . "_id"; //for now assuming non-composite PK
      //$pkValue =  gvfw($primaryKeyName);
      //if($pkValue) {
      //  $pkPhrase =  $table . "_id='" . intval(gvfw($table . "_id")) . "'";
      //}
      $sql = "SELECT " . $name . " FROM " .  $table  . " WHERE " . $pkPhrase;
      if($pkPhrase != "" && !($table == "tenant" || $table == "user")) {
        $sql .= " AND ";
      }
      if($table != "tenant" && $table != "user"){
        $sql .= " tenant_id='" . $tenantId . "'";
      } 
      //echo $sql;
      $result = mysqli_query($conn, $sql);
      if($result) {
        $valueArray = mysqli_fetch_all($result, MYSQLI_ASSOC);
        //also include the hashedentities for that sort of thing
        foreach ($valueArray as &$row) {
            $componentsOfHash = $table . $primaryKeyName . $pkValue;
            //echo $componentsOfHash;
            $row['_hashed_entities'] = hash_hmac('sha256', $componentsOfHash, $encryptionPassword);
        }
        unset($row);
        if($pkValue && $valueArray) {
          $valueArray = $valueArray[0];
        }
      }
      die(json_encode($valueArray, JSON_FORCE_OBJECT));
    }
  } elseif($action == "log") {
      if($table == "device_feature"){
        $out .= deviceFeatureLog(gvfw($table . '_id'), $user);
      }
  //this is the section for conditionals related to specially-written editors and listers
	} else if($table == "report") {
    if ($action == "rerun" || $action == "fetch" || beginsWith(strtolower($action), "run")) {
      $out .= doReport($user, gvfw("report_id"), gvfw("report_log_id"), $outputFormat);
    
    } else if ($action == "startcreate" || gvfw("report_id") != "") {
      $out .=  editReport($errors,  $user);
    } else {
      $out .= reports($user);
    }
	} else if($table == "device") {
    if ($action == "startcreate" || gvfw("device_id") != "") {
      $out .=  editDevice($errors,  $user);
    } else {
     $out .= devices($user);
    }
	} else if($table == "configuration_value") {
    if ($action == "startcreate") {
      $out .= genericEntityForm($tenantId, $table, $errors);
    } else {
      $out .= configurationValues($user);
    }
	} else if($table == "code_template") {
    if ($action == "startcreate" || gvfw("code_template_id") != "") {
      $out .=  editCodeTemplate($errors,  $user);
    } else {
     $out .= codeTemplates($user);
    }
	} else if($table == "device_column_map") {
    if ($action == "startcreate" || gvfw("device_column_map_id") != "") {
      $out .=  editDeviceColumnMap($errors, $deviceId, $user);
    } else {
     $out .= deviceColumnMaps($deviceId, $user);
    }
	} else if($table == "device_feature") {
    if ($action == "startcreate" || gvfw("device_feature_id") != "") {
      $out .=  editDeviceFeature($errors,  $user);
    } else {
      $out .= deviceFeatures($user, $deviceId);
    }
	} else if($table == "management_rule") {
    if ($action == "startcreate" || gvfw("management_rule_id") != "") {
      $out .=  editManagementRule($errors,  $user);
    } else {
      $out .= managementRules($user, $deviceId);
    }
	} else if($table == "command") {
    if ($action == "startcreate" || gvfw("command_id") != "") {
      $out .=  editCommand($errors,  $user);
    } else {
      $out .= commands($tenantId, $deviceId);
    }
  } else if($table == "command_type") {
    if ($action == "startcreate" || gvfw("command_type_id") != "") {
      $out .=  editCommandType($errors,  $user);
    } else {
      $out .= commandTypes($user, $deviceId);
    }
  } else if($table == "weather_condition") {
    if ($action == "startcreate" || gvfw("weather_condition_id") != "") {
      $out .=  editWeatherCondition($errors,  $user);
    } else {
      $out .= weatherConditions($user);
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
      $out .= genericEntityList($tenantId, $table, $outputFormat);
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
  if($action != "startcreate"  && strpos($action, "password") === false){
    if(!$tenantSelector) {
      $out .= loginForm();
    } else {
      $out .= $tenantSelector;
    }
  }
}

echo bodyWrap($out, $user, $deviceId, $poser);

 