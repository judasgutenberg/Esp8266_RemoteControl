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
$user = logIn();
$deviceId = gvfa('device_id', $_GET);
$userId = gvfa("user_id", $user);
//used to impersonate another user

$poser = getImpersonator(false);
if ($poser) {
  $userId = $poser["user_id"];
}
$out = "";
$errors = NULL;
 
 
 
$page = gvfw('page');
$datatype = gvfw('datatype'); 
//echo $table  . "*" . $action. "*" .  $datatype;
//$formatedDateTime =  $date->format('H:i');
 
if ($action == "logout") {
  logOut();
  header("Location: ?action=login");
  die();
} else if  ($action == "disimpersonate") {
  disImpersonate();
  header("Location: .");
  die();
}

if($_POST || gvfw("table")) { //gvfw("table") 

 
	if ($action == "login") {
		loginUser();
  } else if (strtolower($action) == "create user") {
		$errors = createUser();
	}  else if (strtolower($table) == "run") {
    //oh, we're a utility, though we never get here
  } else if(beginsWith(strtolower($action), "save") || beginsWith(strtolower($action), "create")) {
    $errors = genericEntitySave($userId, $table);
  }
} else {
 
}
 
if ($user) {
	$out .= "<div>\n";
  if($action == "getdevices") {
    $out = getDevices($userId);
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
    //a little safer only because it allows a user to screw up records connected to their userId but mabe revisit!!!
    $sql = "UPDATE ". filterStringForSqlEntities($table) . " SET " . filterStringForSqlEntities($name) . "='" .  mysqli_real_escape_string($conn, $value) . "' WHERE user_id=" . intval($userId) . " AND " . filterStringForSqlEntities($primaryKeyName) . "='" . intval($primaryKeyValue) . "'";
    
    $result = mysqli_query($conn, $sql);
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
    $action = $_GET['action']; //$_POST will have this as "run"
    $data = json_decode(gvfw("_data")); //don't actually need this
    //var_dump($data);
    $foundData = getUtilityInfo($user, $action);
    
    if($foundData) {
      if ($_POST ||  gvfa("action", $foundData) && !gvfa("form", $foundData)) {
        //dealing with a utility that has a form
        $role = gvfa("role", $foundData);
        if (doesUserHaveRole($user, $role) && $action) { //don't actually need to do this here any more
          if(array_key_exists("action", $foundData)) {
              $redirect = true;
              $codeToRun = tokenReplace($foundData["action"], $_GET);
              $codeToRun = tokenReplace($codeToRun, $_POST) . ";";
              //die($codeToRun);
              try {
                $result = @eval($evalcode . "; return true;");
                //die(var_dump($result));
                //die($result);
                if($result){
                  eval($codeToRun);
                }
              }
              catch(ParseError $error) { //this shit does not work. does try/catch ever work in PHP?
                //var_dump($error);
                $errors["generic"] = "Data is missing for this utility.";
                $redirect = false;
              }
              if($redirect){
                header('Location: '.$_SERVER['PHP_SELF'] . "?table=" . $table);
                die();
              }
          }
        }
      } else if($action == "xxxxx") {
        
      } else if ($action) {
        //var_dump($foundData);
        $out .= utilityForm($foundData);
      }
   }  

 
   if(!$foundData ) {
    $out .= "<div class='generalerror'>Utility not yet developed.</div>";
   } else if(!$action) {
    $out .= utilities($user, "list");
   }
 
	} else if($table == "report") {
    if ($action == "fetch" || beginsWith(strtolower($action), "run")) {
      $out .= doReport($userId, gvfw("report_id"));
    } else if ($action == "startcreate" || gvfw("report_id") != "") {
      $out .=  editReport($errors,  $userId);
    } else {
      $out .= reports($userId);
    }
	} else if($table == "device") {
    $out .= devices($userId);
  } else if ($action == "startcreate") {
    if ($table == "test") {
      
    } else if($table == "device") {
      $out .=  deviceForm($errors,  $userId);
    } else  if($table == "management_rule") {
      $out .=  managementRuleForm($errors,  $userId);
    } else if ($table == "device_feature") {
      $out .= deviceFeatureForm($errors,  $userId);
    } else {
      $out .= genericEntityForm($userId, $table, $errors);
    }
 
  } else if ($action == "delete") {
    $sql = "DELETE FROM " . $table . " WHERE " . $table . "_id='" . intval(gvfw( $table . "_id")) . "' AND user_id='" . $userId . "'";
    //die($sql);
    $hashedEntities = gvfw('hashed_entities');
    $whatHashedEntitiesShouldBe =  crypt($table .$table . "_id"  . intval(gvfw( $table . "_id")) , $encryptionPassword);
    if($hashedEntities != $whatHashedEntitiesShouldBe){
      die("Data appears to have been tampered with.");
    }
    $result = mysqli_query($conn, $sql);
    header('Location: '.$_SERVER['PHP_SELF'] . "?table=" . $table);
  } else if($table!= "user" || $user["role"]  == "super") {
    if(gvfw($table . '_id')) {
      


      if($action == "log") {
        if($table == "device_feature"){
          $out .= deviceFeatureLog(gvfw($table . '_id'), $userId);
        }
      } else {
        if($action == "json"){
          $sql = "SELECT * FROM " .  $table  . " WHERE " . $table . "_id='" . intval(gvfw( $table . "_id")) . "' AND user_id='" . $userId . "'";
          $result = mysqli_query($conn, $sql);
          $valueArray = mysqli_fetch_assoc($result);
          die(json_encode($valueArray, JSON_FORCE_OBJECT));
        } else {
        if($table == "management_rule"){
            $out .= managementRuleForm($errors,  $userId);
          } else if($table == "device") {
            $out .=  deviceForm($errors,  $userId);
          } else if ($table == "device_feature") {
            $out .= deviceFeatureForm($errors,  $userId);
          } else {
  
            $out .= genericEntityForm($userId, $table, $errors);
          }
        }
      }

    } else {
      if($table == "device_feature") {
        $out .= deviceFeatures($userId, $deviceId);
      } else if($table == "device") {
          $out .= devices($userId, $deviceId);
      } else {
        $out .= genericEntityList($userId, $table);
      }
      
    }
    
  }
	$out .= "</div>\n";
	$out .= "<div>\n";
	$out .= " "; 
	$out .= "</div>\n";

} else {
 
  if ($table == "user" || !is_null($errors)) {
    $out .= newUserForm($errors);
  } else if(gvfa("password", $_POST) != "") {
    $out .= "<div class='genericformerror'>The credentials you entered have failed.</div>";
   }
  if($action != "startcreate"){
    $out .= loginForm();
  }
}


echo bodyWrap($out, $user, $deviceId, $poser);
