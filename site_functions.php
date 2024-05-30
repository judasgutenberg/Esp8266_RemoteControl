<?php 

function utilityForm($foundData) {
  $out = "";
  if(array_key_exists("form", $foundData)){
    $out .= "<div class='issuesheader'>" . $foundData["label"]. "</div>";
    $out .= "<div>" . $foundData["description"] . "</div>";
    $form = genericForm($foundData["form"], "Run", "Running " . $foundData["label"]);
    $confirmJs  = "onclick=\"return(confirm('Are you sure you want to run " . $foundData["label"] . "?'))\"";
    $form = str_replace("value='Run' type='submit'/>", "value='Run' type='submit'  " . $confirmJs  . "/>", $form);
    $out .= $form;
  }
  return $out;
}

function doesUserHaveRole($user, $role) {
  if($role == "") {
    return true;
  }
  if(array_key_exists("role", $user)){
    if(gvfa("role", $user) == $role || strtolower(gvfa("role", $user)) == "admin" ) {
      return true;
    }
  }
  return false;
}

function getUtilityInfo($user, $key){
  $data =  utilities($user, "data");
  //echo "$" . $key . "$";
  //var_dump($data);
  $searchValue = $key;
  $resultArray = array_filter($data, function ($subData) use ($searchValue) {
    return $subData["key"] == $searchValue;
  });
  //var_dump($resultArray);
  $foundData = reset($resultArray);
  return $foundData;
}

function bodyWrap($content, $user, $deviceId, $poser = null) {
  $out = "<!doctype html>";
  $out .= "<html>\n";
  $out .= "<head>\n";
  $siteName = "Remote Controller";
  $version = filemtime("./index.php");
  $out .= "<script src='tablesort.js?version=" . urlencode($version) . "'></script>\n";
  $out .= "<script src='tool.js?version=" . urlencode($version) . "'></script>\n";
  $out .= "<link rel='stylesheet' href='tool.css?version=" . urlencode($version) . "'>\n";
  $out .= "<title>" . $siteName . "</title>\n";
  $out .= "</head>\n";
  $out .= "<body>\n";
  $out .= topmostNav();
  $out.= "<div class='logo'>" . $siteName . "</div> \n";

  $out .= "<div class='waitingouter' id='waitingouter'><div class='waiting' id='waiting'><img width='200' height='200' src='./images/signs.gif'></div><div id='waitingmessage' class='waitingmessage'>Waiting...</div></div>\n";
  $out .= "<div class='outercontent'>\n";
  
  $poserString = "";
  if($user) {
    if($poser) {
      $poserString = " posing as <span class='poserindication'>" . $poser["email"] . "</span> (<a href='?action=disimpersonate'>unpose</a>)";

    }
    $out .= "<div class='loggedin'>You are logged in as <b>" . $user["email"] . "</b>" .  $poserString . " <div class='basicbutton'><a href=\"?action=logout\">logout</a></div></div>\n";
	}
	else
	{
    //$out .= "<div class='loggedin'>You are logged out.  </div>\n";
	} 
	$out .= "<div>\n";
 
  $out .= "<div class='devicedescription'>";
 
  $out .= "</div>";
	$out .= tabNav($user);
 
  $out .= "<div class='innercontent'>\n";
  $out .= $content;
  $out .= "</div>\n";
  $out .= "</div>\n";
  $out .= "</body>\n";
  $out .= "</html>\n";
  return $out;
}



function filterStringForSqlEntities($input) {
  // Replace characters that are not letters, numbers, dashes, or underscores with an empty string
  $filtered = preg_replace('/[^a-zA-Z0-9-_]/', '', $input);

  return $filtered;
}

function logIn() {
  Global $encryptionPassword;
  Global $cookiename;
  if(!isset($_COOKIE[$cookiename])) {
    return false;
  } else {
  
   $cookieValue = $_COOKIE[$cookiename];
   $email = openssl_decrypt($cookieValue, "AES-128-CTR", $encryptionPassword);
   if(strpos($email, "@") > 0){
      return getUser($email);
      
   } else {
      return  false;
   }
  }
}
 
function disImpersonate() {
	Global $poserCookieName;
	setcookie($poserCookieName, "");
	return false;
}

function logOut() {
	Global $cookiename;
	setcookie($cookiename, "");
	return false;
}
 
function loginForm() {
  $out = "";
  $out .= "<form method='post' name='loginform' id='loginform'>\n";
  $out .= "<strong>Login here:</strong>  email: <input name='email' type='text'>\n";
  $out .= "password: <input name='password' type='password'>\n";
  $out .= "<input name='action' value='login' type='submit'>\n";
  $out .= "<div> or  <div class='basicbutton'><a href=\"tool.php?table=user&action=startcreate\">Create Account</a></div>\n";
  $out .= "</form>\n";
  return $out;
}


function xForm($error = NULL, $wordId, $userId, $wordListId = "", $source = null) {
  Global $conn;
  if($wordId  != "") {
    $sql = "SELECT * FROM `word` WHERE user_id = " . intval($userId) . " AND word_id = " . intval($wordId);
    $result = mysqli_query($conn, $sql);
    $row = $result->fetch_assoc();
    $source = $row;
  } else if ($_POST){
    $source = $_POST;
  } else if(is_null($source) && array_key_exists("word_id", $source)) {
    $source = array("word_list_id"=> $wordListId);
    
  }
  if(!$wordId) {
    $wordId = $source["word_id"];
  }
  $submitLabel = "save word";
  if($wordId  == "") {
    $submitLabel = "create word";
  }
	$formData = array(
		[
	    'label' => 'word',
      'name' => 'word',
      'width' => 400,
	    'value' => gvfa("word", $source), 
      'error' => gvfa('word', $error)
	  ],
    [
	    'label' => 'type',
      'name' => 'type',
      'type' => 'select',
	    'value' => gvfa("type", $source), 
      'error' => gvfa('type', $error),
      'values' => [
        'white',
        'black' 
      ],
	  ],
    [
	    'label' => 'list',
      'name' => 'word_list_id',
      'type' => 'select',
	    'value' => gvfa("word_list_id", $source), 
      'error' => gvfa('word_list_id', $error),
      'values' => "SELECT word_list_id, name as 'text' FROM word_list WHERE user_id='" . $userId  . "' ORDER BY name ASC",
	  ]
    ,
	  [
	    'label' => '',
      'name' => 'word_id',
      'value' => gvfa("word_id", $source),
      'type' => 'hidden',
      'error' => gvfa('word_id', $error)
	  ]
    );
  return genericForm($formData, $submitLabel);
}

function newUserForm($error = NULL) {
	$formData = array(
  [
    'label' => 'email',
    'name' => 'email',
    'value' => gvfa("email", $_POST), 
    'error' => gvfa('email', $error)
  ],
  [
    'title' => 'password',
    'name' => 'password',
    'type' => 'password',
    'value' => gvfa("password", $_POST), 
    'error' => gvfa('error', $error)
  ],
  [
    'label' => 'password (again)',
    'name' => 'password2',
    'type' => 'password',
    'value' => gvfa("password2", $_POST),
    'error' => gvfa('password2', $error)
	   ]
	);
  return genericForm($formData, "create user");
}

function presentList($data) {
  $out = "<ul>\n";
  foreach ($data as $item) {
    $out .=  "<li>\n";
    if(array_key_exists('url', $item)){
      $url = $item['url'];
    } else if(array_key_exists('key', $item)) {
      $url = "?table=utilities&action=" . $item['key'];
    }
    $out .=  '<a href="' . $url . '">' . htmlspecialchars($item['label']) . '</a>';
    $out .=  ' - ' . htmlspecialchars($item['description']);
    $out .=  "\n</li>\n";
  }
  $out .=  "</ul>\n";
  return $out;
}

 

function camelCaseToWords($input) { //thanks, chatgpt!
  // Use a regular expression to add spaces before each uppercase letter
  $result = preg_replace('/(?<!^)([A-Z])/', ' $1', $input);
  // Convert the result to lowercase
  $result = strtolower($result);
  return $result;
}

function generateSubFormFromJson($parentKey, $jsonString, $template) {
  $templateArray = (array)json_decode($template);
  //var_dump($templateArray);
  // Decode the JSON string
  $data = (array)json_decode($jsonString, true);
  if($templateArray && false){
    $keysToAdd = array_diff_key($data, $templateArray);
  
    $data = $data + $keysToAdd;
  }
  // Start building the form
  // Generate form elements recursively
  return generateFormElements($parentKey, $data);
}

function generateFormElements($parentKey, $data) {
  $formElements = '<div class="genericform">';
  foreach ($data as $key => $value) {
      //$formElements .= '<div>';
      // Create a label based on the key
      $formElements .= '<div class="genericformelementlabel" for="' . $key . '">' . camelCaseToWords($key) . '</div>';
      if (is_array($value)) {
          // Recursively generate form elements for arrays
          $formElements .= generateFormElements($key, $value);
      } else {
          // Create an input field for non-array values
          $formElements .= '<div class="genericformelementinput"><input style="width:200px"  type="text" name="' . $parentKey . "|" . $key . '" id="' . $key . '" value="' . htmlspecialchars($value) . '"></div>';
      }
      //$formElements .= '</div>';
  }
  $formElements .= '</div>';
  return $formElements;
}

//also returns pk by reference
function schemaArrayFromSchema($table, &$pk){
  Global $conn;
  $sql = "EXPLAIN " . $table;
  $result = mysqli_query($conn, $sql);
  $headerData = [];
  if($result) {
 
	  $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
    
    foreach($rows as $row) {
      $fieldName = $row["Field"];
      $type = "string";
      if($row["Key"] == "PRI"){
        $pk = $fieldName;
        $type = "hidden";
      }
      if($row["Type"] == "tinyint(4)" ){
        $type = "bool";
      }
      if(beginswith($row["Type"] , "int") || beginswith($row["Type"] , "decim")|| beginswith($row["Type"] , "float")  ){
        $type = "number";
      }
      if($fieldName != "user_id") {
        $record = ["label" => $fieldName, "name" => $fieldName, "type" => $type ];
        if($type == "bool" || $type == "number" ){
          $record["liveChangeable"] = true;
        }
        $headerData[] = $record;
      }
      
    }
  }
  return $headerData;
}

function genericEntityList($userId, $table) {
  Global $conn;
  $headerData = schemaArrayFromSchema($table, $pk);
  $additionalValueQueryString = "";
  foreach($_REQUEST as $key=>$value){ //slurp up values passed in
    if(endsWith($key, "_id")){
      $additionalValueQueryString .= "&" . $key . "=" . urlencode($value);
    }
  }
  $out = "<div class='listtools'><div class='basicbutton'><a href='?table=" . $table . "&action=startcreate" . $additionalValueQueryString . "'>Create</a></div> a new " . $table . "<//div>\n";
  $thisDataSql = "SELECT * FROM " . $table . " WHERE user_id=" . intval($userId);
  $deviceId = gvfw("device_id");
  if($deviceId && $table== "device_feature" ){
    $thisDataSql .= " AND device_id=" . intval($deviceId);
  }
  $thisDataResult = mysqli_query($conn, $thisDataSql);
  if($thisDataResult) {
    $thisDataRows = mysqli_fetch_all($thisDataResult, MYSQLI_ASSOC); 
    $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a>";
    $toolsTemplate .= " | " . deleteLink($table, $table. "_id" ); 
    //if($table == "device") {
      //$toolsTemplate .= " | <a href='?table=device_feature&device_id=<" . $table . "_id/>'>Device Features</a>";

    //}
   
    $out .= genericTable($thisDataRows, $headerData, $toolsTemplate, null, $table, $pk);
    return $out;
  }
}

function genericEntityForm($userId, $table, $errors){
  Global $conn;
  $data = schemaArrayFromSchema($table, $pk);
  $pkValue =  gvfa($pk, $_GET);
  $thisDataSql = "SELECT * FROM " . $table . " WHERE " . $pk . " = '" . $pkValue . "' AND user_id=" . intval($userId);

  $thisDataResult = mysqli_query($conn, $thisDataSql);
  if($thisDataResult) {
    $thisDataRows = mysqli_fetch_all($thisDataResult, MYSQLI_ASSOC);
    if($thisDataRows && count($thisDataRows) > 0) {
      $data = updateDataWithRows($data, $thisDataRows[0]);
    }
    

    return genericForm($data, "Save " . $table, "Saving...");
  }
}

 
function genericEntitySave($userId, $table) {
  Global $conn;
  //$data = schemaArrayFromSchema($table, $pk);
  $pk = $table . "_id";
  $data = $_POST;
  unset($data['action']);
  unset($data[$pk]);
  unset($data['created']);
  $data["user_id"] = $userId;
  $sql = insertUpdateSql($conn, $table, array($pk => gvfw($table . '_id')), $data);
  //echo $sql;
  //die();


  if (mysqli_multi_query($conn, $sql)) {
    do {
      // Store first result set
      if ($result = mysqli_store_result($conn)) {
        while ($row = mysqli_fetch_row($result)) {
          printf("%s\n", $row[0]);
        }
        mysqli_free_result($result);
      }
      // if there are more result-sets, the print a divider
      if (mysqli_more_results($conn)) {
        printf("-------------\n");
      }
       //Prepare next result set
    } while (mysqli_next_result($conn));
  }


  $id = mysqli_insert_id($conn);
  header("Location: ?table=" . $table);
}

function updateDataWithRows($data, $thisDataRows) {
  // Iterate over each row in $thisDataRows
  foreach ($thisDataRows as $key => $value) {
      // Iterate over each associative array in $data
      foreach ($data as &$item) {
          // Check if the "name" key in the current item matches the key in $thisDataRows
          if (isset($item['name']) && $item['name'] == $key) {
              // Set the "value" of the current item to the value in $thisDataRows
              $item['value'] = $value;
              // Break out of the inner loop since we found a match
              break;
          }
      }
  }
  return $data;
}

function genericForm($data, $submitLabel, $waitingMesasage = "Saving...") { //$data also includes any errors
  Global $conn;
	$out = "";
  $onSubmitManyToManyItems = [];
	$out .= "<div class='genericform'>\n";
  $columnCount = 0;
	foreach($data as &$datum) {
		$label = gvfa("label", $datum);
		$value = str_replace("\\\\", "\\", gvfa("value", $datum)); 
		$name = gvfa("name", $datum); 
		$type = strtolower(gvfa("type", $datum)); 
    $width = 200;
    
    if(endsWith($name, "_id") && $columnCount == 0  && ($type == "" || $type == "number")) { //make first column read-only if it's an _id
      $type = "read_only";
    }
 
    if(gvfa("width", $datum)){
      $width = gvfa("width", $datum);
    }
    $height = '';
    if(gvfa("height", $datum)){
      $height = gvfa("height", $datum);
    }
    $values =gvfa("values", $datum); 
		$error = gvfa("error", $datum); 
		if($label == "") {
			$label = $name;
		}
		if($type == "") {
			$type = "text";
		}
    $idString = "";
		if($type == "file") {
			$idString = "id='file'";
      $waitingMesasage = "Uploading...";
		}
		if($type == "hidden") {
      $out .= "<input name='" . $name . "' value=\"" .  ($value) . "\" type='" . $type . "'/>";
		} else {
      $out .= "<div class='genericformelementlabel'>" . $label . ": </div>";
      $out .= "<div class='genericformelementinput'>";
      $out .= "<div class='genericformerror'>" . $error . "</div>";
      $template = gvfa("template", $datum);
      if($type == 'json') {
        if($value) {
          $out .= generateSubFormFromJson($name, $value, $template);
        } else {
          
          $out .= generateSubFormFromJson($name, $template, $template);

        }
    
      } else if($type == 'select') {

        $out .= "<select  name='" . $name . "' />";
        if(is_string($values)) {
          $out .= "<option value='0'>none</option>";
          //echo $values;
          $result = mysqli_query($conn, $values); //REALLY NEED TO SANITIZE $values since it contains RAW SQL!!!
          $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
          foreach($rows as $row){
            $selected = "";
            if(!$value){
              $value = gvfw($name);
            }
            if($row[$name] == $value) {
              $selected = " selected='selected' ";

            }
            $out .= "<option " . $selected . " value='". $row[$name] . "'>" . $row["text"]  . "</option/>\n";

          }
          

        } else if(is_array($values)){
          $out .= "<option value=''>none</option>";
          foreach($values as &$optionValue) {
            $selected = "";
            if($value == $optionValue){
              $selected = "selected";

            }
            $out .= "<option ". $selected . ">" . $optionValue . "</option/>\n";
          }
        }
        $out .= "</select>";
      } else if ($type == "many-to-many") {
        //echo $values;
        $result = mysqli_query($conn, $values); //REALLY NEED TO SANITIZE $values since it contains RAW SQL!!!
        $rows = null;
        $itemTool = gvfa("item_tool", $datum);
        $itemToolString = "";
        if($itemTool){
          $itemToolString = " onmouseup='" . $itemTool . "(this)' ";
        }
        if($result) {
          $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
        }
        $out .= "<div class='destinationitems'>\n";
        $out .= "attached:<br/>";
        if($height == ""){
          $height = 5;
        }
        $out .= "<select multiple='multiple' name='" . $name . "[]' id='dest_" . $name . "' size='" . intval($height) . "'/>";
        if($rows) {
          foreach($rows as $row){
            $selected = "";
            if(!$value){
              $value = gvfw($name);
            }
            if($row[$name] == $value) {
              $selected = " selected='selected' ";

            }

            if($row["has"] ){
              $out .= "<option " . $itemToolString . $selected . " value='". $row[$name] . "'>" . $row["text"]  . "</option/>\n";
            }

          }
        }
        $out .= "</select>";
        $onSubmitManyToManyItems[] = $name;
        $out .= "</div>\n"; 
        $out .= "<div class='manytomanytools'>\n";
        $out .= "<button onclick='return copyManyToMany(\"source_" . $name ."\", \"dest_" . $name ."\")'>&lt;</button>";
        $out .= "<button onclick='return copyManyToMany(\"dest_" . $name ."\", \"source_" . $name ."\")'>&gt;</button>";
        $out .= "</div>\n"; 
        $out .= "<div class='sourceitems'>\n";
        $out .= "available:<br/>";
        $out .= "<select name='source_" . $name . "' id='source_" . $name . "' size='" . intval($height) . "'/>";
        if($rows) {
          foreach($rows as $row){
            $selected = "";
            if(!$value){
              $value = gvfw($name);
            }
            if($row[$name] == $value) {
              $selected = " selected='selected' ";

            }
            if(!$row["has"] ){
              $out .= "<option " . $itemToolString . $selected . " value='". $row[$name] . "'>" . $row["text"]  . "</option/>\n";
            }

          }
        }
        $out .= "</select>";
        $out .= "</div>\n"; 
        $out .= "<div class='toolpanel' id='panel_" . $name . "'>\n"; 
        $out .= "</div>\n"; 

      } else if ($type == "bool" || $type == "checkbox"){
        $checked = "";
          if($value) {

            $checked = "checked";
          }
        $out .= "<input value='1' name='" . $name . "'  " . $checked . " type='checkbox'/>\n";
      } else if ($type == "read_only"){
        $out .= $value . "\n";
      } else {
        if($height){
          $out .= "<textarea " .  $idString . " style='width:" . $width . "px;height:" . $height . "px' name='" . $name . "'  />" .  $value  . "</textarea>\n";
        } else {

          $out .= "<input style='width:" . $width . "px'  " . $idString. " name='" . $name . "' value=\"" .  $value . "\" type='" . $type . "'/>\n";
        }
        
      }
      
      $out .= "</div>\n";
    }
    $columnCount++;
	}
	$out .= "<div class='genericformelementlabel'><input name='action' id='action' value='" .  $submitLabel. "' type='submit'/></div>\n";
  $out .= "<input  name='_data' value=\"" . htmlspecialchars(json_encode($data)) . "\" type='hidden'/>";
  
	$out .= "</div>\n";
	$out .= "</form>\n";
  $out .= "\n<script>let onSubmitManyToManyItems=['" . implode("','", $onSubmitManyToManyItems) . "'];</script>\n";
  $out = "<form name='genericForm' onsubmit='formSubmitTasks();startWaiting(\"" . $waitingMesasage . "\")' method='post' name='genericform' id='genericform' enctype='multipart/form-data'>\n" . $out;
	return $out;
}

function assembledJsonData($template, $parentName, $sourceData){
  //var_dump($template);
  //echo gettype($sourceData);
  //echo $sourceData["config|excludeNumbers"] . "^";
  foreach($template  as $key => $value){

    if (is_array($value)) {
      $out[$key] = assembledJsonData($template[$key], $key, $sourceData);
    } else {
      $out[$key] = $sourceData[$parentName . "|" .  $key];
    }
  }
  return $out;
}

function reassembleFormJson($formData, $sourceData){
  //echo gettype($sourceData);
  foreach($formData as &$datum) {
    $datum = (array)$datum;
    $label = gvfa("label", $datum);
		$value = str_replace("\\\\", "\\", gvfa("value", $datum)); 
		$name = gvfa("name", $datum); 
		$type = strtolower(gvfa("type", $datum)); 
    $values =gvfa("values", $datum); 
		$error = gvfa("error", $datum); 
    if($type == "json"){
      if($datum["template"]) {
        $out = json_encode(assembledJsonData(json_decode($datum["template"]), $name,  $sourceData));

        return $out;
        //foreach($formData["template"]  as $key => $value){

        
      }
    }
  }
  die();
}



function getUserById($id) {
  Global $conn;
  $sql = "SELECT * FROM `user` WHERE user_id = " . intval($id);
  $result = mysqli_query($conn, $sql);
  $row = $result->fetch_assoc();
  return $row;
}

function getUser($email) {
  Global $conn;
  $sql = "SELECT * FROM `user` WHERE email = '" . mysqli_real_escape_string($conn, $email) . "'";
  $result = mysqli_query($conn, $sql);
  $row = $result->fetch_assoc();
  return $row;
}

function impersonateUser($impersonatedUserId) {
  Global $encryptionPassword;
  Global $poserCookieName;
  setcookie($poserCookieName, openssl_encrypt($impersonatedUserId, "AES-128-CTR", $encryptionPassword), time() + (30 * 365 * 24 * 60 * 60));
  header("location: .");
}

function getImpersonator($justId = true){
  Global $encryptionPassword;
  Global $poserCookieName;
  $poserId = openssl_decrypt(gvfa($poserCookieName, $_COOKIE), "AES-128-CTR", $encryptionPassword);
  //die($poserCookieName . " " . $poserId );
  if($justId){
    return $poserId;
  }
  return getUserById($poserId);
}

function loginUser($source = NULL) {
  Global $conn;
  Global $encryptionPassword;
  Global $cookiename;
  if($source == NULL) {
  	$source = $_REQUEST;
  }
  $email = gvfa("email", $source);
  $passwordIn = gvfa("password", $source);
  $sql = "SELECT `email`, `password` FROM `user` WHERE email = '" . mysqli_real_escape_string($conn, $email) . "' ";
  //die($sql);

  $result = mysqli_query($conn, $sql);
  if(!$result){
    header("location: .");
    die();

  }
  //try{
    $row = $result->fetch_assoc();
    if($row  && $row["email"] && $row["password"]) {
      $email = $row["email"];
      $passwordHashed = $row["password"];
      //for debugging:
      //echo crypt($passwordIn, $encryptionPassword);
      if (password_verify($passwordIn, $passwordHashed)) {
        //echo "DDDADA";
          setcookie($cookiename, openssl_encrypt($email, "AES-128-CTR", $encryptionPassword), time() + (30 * 365 * 24 * 60 * 60));
          header('Location: '.$_SERVER['PHP_SELF']);
          //echo "LOGGED IN!!!" . $email ;
          die;
      }
    }
    return false;

  //}  catch(Exception $e) {
    //header("location: .");
  //}
}


function topmostNav() {
	$tabData = array(

    [
      'label' => 'Weather',
      'url'=> "index.php",
      'lpad'=> 22
    ],
    [
      'label' => 'Energy',
      'url' => "energy.php"
    ],
    [
      'label' => 'Device Control',
      'url' => "tool.php"
    ]);
  $out = "<div class='nav'>";
  $currentMode = gvfa('table', $_REQUEST);
  $deviceId = gvfa('device_id', $_REQUEST);
  foreach($tabData as &$tab) {
    $label = gvfa("label", $tab);
    $url = gvfa("url", $tab); 
    $lpad = gvfa("lpad", $tab); 
    $rpad = gvfa("rpad", $tab); 
    $class = "navtab";
    $pathArray = explode("/", $_SERVER['PHP_SELF']);
    $currentFile = $pathArray[count($pathArray) - 1];
    //echo $url . " " . $currentFile  . "<br/>";
    if($currentFile == $url) {
      $class = "navtabthere";
    }
    if($lpad) {
      $out .= "<span style='padding-right:" . $lpad . "px;width:10px;height:12px'></span>\n";
    }
    $out .= "<a href='./" . $url . "'><div class='" . $class . "'>" . $label . "</div></a>";
    if($rpad) {
      $out .= "<span style='padding-right:" . $rpad . "px;width:10px;height:12px'></span>\n";
    }
  }
  $out .= "</div>\n";
  return $out;

}

function tabNav($user) {
	$tabData = array(
 
  [
    'label' => 'Devices',
    'table' => 'device' 
  ] ,
  [
    'label' => 'Device Types',
    'table' => 'device_type' 
  ] ,
  [
    'label' => 'Feature Types',
    'table' => 'feature_type' 
  ] ,
  [
    'label' => 'Device Feature Types',
    'table' => 'device_type_feature' 
  ] ,
  [
    'label' => 'Device Features',
    'table' => 'device_feature' 
  ] 
  ,
  [
    'label' => 'Management Rules',
    'table' => 'management_rule' 
  ]   ,
  [
    'label' => 'Sensor Data',
    'table' => 'sensors' 
  ] 
	);
  
  if($user && $user["role"] == "super") {
    $tabData[] =   [
      'label' => 'Users',
      'table' => 'user' 
    ];
  }
	$out = "<div class='nav'>";
  $currentMode = gvfa('table', $_REQUEST);
  $deviceId = gvfa('device_id', $_REQUEST);
 
  foreach($tabData as &$tab) {

    if($currentMode == "") {
      $currentMode  = "device";
    }
    $label = gvfa("label", $tab);
    $table = gvfa("table", $tab); 
    $class = "navtab";
    if($currentMode == $table) {
      $class = "navtabthere";
    }
    $url = "?table=" . $table;
    if($table!= "word_list"){
      //$url .= "&word_list_id=" . $wordListId ; //too messy
    }
    if($table!= "device"){
      $url .= "&device_id=" . $deviceId;
    }
    
    $out .= "<a href='" . $url . "'><div class='" . $class . "'>" . $label . "</div></a>";
  }
  $out .= "</div>\n";
  return $out;
}

 

function genericTable($rows, $headerData = NULL, $toolsTemplate = NULL, $searchData = null, $tableName = "", $primaryKeyName = "", $autoRefreshSql = null) { //aka genericList
  Global $encryptionPassword;
  if($headerData == NULL  && $rows  && $rows[0]) {
    $headerData = [];
    foreach(array_keys($rows[0]) as &$key) {
      array_push($headerData, array("label"=>$key, "name"=>$key));
    }
  }
 ;
  $out = "";
  if($searchData) {
    $out .=  "<form>\n";
    $out .=  "<input type='hidden' id='action' name='action' value='" . $searchData["action"] . "' />\n";
    $out .=  "<input type='hidden' name='table' value='" . $searchData["table"] . "' />\n";
    //"extraData" => array("word_list_id"=>$wordListId)
    if($searchData["extraData"]){
      foreach($searchData["extraData"] as $key=>$value){
        $out .=  "<input type='hidden' name='" . htmlspecialchars($key) . "' value='" . htmlspecialchars($value). "' />\n";
      }
      
    }
    $out .= " Search: <input value='" . htmlspecialchars(gvfw($searchData["searchTerm"])) . "' id='" . $searchData["searchTerm"] . "' name='" . $searchData["searchTerm"] . "' onkeyup=\"" . str_replace("<id/>", "document.getElementById('" . $searchData["searchTerm"] . "').value", $searchData["onkeyup"] ). "\"></form>"; 

  }
  $out .= "<div class='list' id='list'>\n";

  $out .="<div class='listheader'>\n";
  $cellNumber = 0;
  foreach($headerData as &$headerCell) {
  	$out.= "<span class='headerlink' onclick='sortTable(" . $cellNumber . ")'>" . $headerCell['label'] . "</span>\n";
    $cellNumber++;
  }
  if($toolsTemplate) {
  
    $out .= "<span></span>\n";
  }
  $out .= "</div>\n";
  //$out .= "<div class='listbody' id='listbody'>\n";
  for($rowCount = 0; $rowCount< count($rows); $rowCount++) {
    $row = $rows[$rowCount]; 
    $out .= "<div class='listrow'>\n";
    foreach($headerData as &$headerItem) {
      //var_dump($headerItem);
      $name = gvfa("name", $headerItem);
      $label = gvfa("label", $headerItem);
      $function = gvfa("function", $headerItem);
      if (array_key_exists("type", $headerItem)){
        $type = $headerItem["type"];
      } else {
        $type = "text";
      }
      if (array_key_exists("template", $headerItem)){  //useful for having links outside the toolsTemplate section. we can ignore toolsTemplate in some situations
        $template = $headerItem["template"];
      } else {
        $template = "";
      }
      $out .= "<span>";
      $checkedString = " ";
      $value = $row[$name];
      if($function){ //allows us to have columns with values that are calculated from PHP
      
        $function =  tokenReplace($function, $row, $tableName) . ";"; 
        //echo $function . "<P>";
        try{
          eval('$value = ' . $function);
        }
        catch(Exception  $err){
          //echo $err;

        }
      }
      if (gvfa("liveChangeable", $headerItem)) {
        if($row[$name] == 1){
          $checkedString = " checked ";
          
        }

        if(($type == "color"  || $type == "text"  || $type == "number" || $type == "string") &&  $primaryKeyName != $name){
          $hashedEntities =  crypt($name . $tableName .$primaryKeyName  . $row[$primaryKeyName] , $encryptionPassword);
          $out .= "<input onchange='genericListActionBackend(\"" . $name . "\",  this.value ,\"" . $tableName  . "\",\"" . $primaryKeyName  . "\",\"" . $row[$primaryKeyName] . "\",\""  . $hashedEntities . "\")' value='" . $value . "'  name='" . $name . "' type='" . $type . "' />\n";
        } else if(($type == "checkbox" || $type == "bool")  &&  $primaryKeyName != $name) {
          $hashedEntities =  crypt($name . $tableName .$primaryKeyName  . $row[$primaryKeyName] , $encryptionPassword);
          $out .= "<input onchange='genericListActionBackend(\"" . $name . "\",this.checked,\"" . $tableName  . "\",\"" . $primaryKeyName  . "\",\"" . $row[$primaryKeyName] . "\",\""  . $hashedEntities . "\")' name='" . $name . "' type='checkbox' value='1' " . $checkedString . "/>\n";
        } else {
          $out .= $row[$name];
        }
      } else {
        if($template != "") {
          $out .=  "<a href=\"" . tokenReplace($template, $row, $tableName) . "\">" . htmlspecialchars($value) . "</a>";
        } else {
          $out .=  htmlspecialchars($value);
        }
        
      }
      $out .= "</span>\n";
    }
    if($toolsTemplate) {
      
      $out .= "<span>" . tokenReplace($toolsTemplate,  $row, $tableName) . "</span>\n";
    }
    $out .= "</div>\n";
  }
  //$out .= "</div>\n";
  $out .= "</div>\n";
  if($autoRefreshSql) {
    $encryptedSql = encryptLongString($autoRefreshSql, $encryptionPassword);
    $out .= "<script>autoUpdate('" . $encryptedSql . "','" . addslashes(json_encode($headerData)) . "','list');</script>";

  }
  return $out;
}

function tokenReplace($template, $data,  $tableName = "", $strDelimiterBegin = "<", $strDelimiterEnd = "/>"){
  Global $encryptionPassword;
  foreach($data as $key => $value) {
    $template = str_replace($strDelimiterBegin . $key . $strDelimiterEnd, $value, $template);
  }
  if($tableName!= "") {
    $hashedEntities = crypt($tableName . $tableName . "_id" . $data[$tableName . "_id"], $encryptionPassword);
    $template = str_replace($strDelimiterBegin . "hashed_entities" . $strDelimiterEnd, $hashedEntities, $template);
  }
  return $template;
}

function gvfw($name, $fail = false){ //get value from wherever
  $out = gvfa($name, $_REQUEST, $fail);
  if($out == "") {
    $out = gvfa($name, $_POST, $fail);
  }
  return $out;
}

function gvfa($name, $source, $fail = false){ //get value from associative
  if(isset($source[$name])) {
    return $source[$name];
  }
  return $fail;
}

function beginsWith($strIn, $what) {
//Does $strIn begin with $what?
	if (substr($strIn,0, strlen($what))==$what){
		return true;
	}
	return false;
}

function endsWith($strIn, $what) {
//Does $strIn end with $what?
	if (substr($strIn, strlen($strIn)- strlen($what) , strlen($what))==$what) {
		return true;
	}
	return false;
}

function createUser(){
  Global $conn;
  Global $encryptionPassword;
  $errors = NULL;
  $date = new DateTime("now", new DateTimeZone('America/New_York'));//obviously, you would use your timezone, not necessarily mine
  $formatedDateTime =  $date->format('Y-m-d H:i:s'); 
  $password = gvfa("password", $_POST);
  $password2 = gvfa("password2", $_POST);
  $email = gvfa("email", $_POST);
  if($password != $password2 || $password == "") {
  	$errors["password2"] = "Passwords must be identical and have a value";
  }
  if (!filter_var($email, FILTER_VALIDATE_EMAIL)) {
	$errors["email"] = "Invalid email format";
  }

  if(is_null($errors)) {
  	$encryptedPassword =  crypt($password, $encryptionPassword);
    if(count(userList()) == 0) {
      //if there are no users, create the first one as admin
      $sql = "INSERT INTO user(email, password, created, role) VALUES ('" . $email . "','" .  mysqli_real_escape_string($conn, $encryptedPassword) . "','" .$formatedDateTime . "','admin')"; 
    } else {
  	  $sql = "INSERT INTO user(email, password, created) VALUES ('" . $email . "','" .  mysqli_real_escape_string($conn, $encryptedPassword) . "','" .$formatedDateTime . "')"; 
    }
	  //echo $sql;
    //die();
    $result = mysqli_query($conn, $sql);
    $id = mysqli_insert_id($conn);
    //updateTablesFromTemplate($id);
    //die("*" . $id);
    loginUser($_POST);
    header("Location: ?");
  } else {
  	return $errors;
  
  }
  return false;
}

function userList(){
  Global $conn;
  $userSql = "SELECT * FROM user";
  $thisDataResult = mysqli_query($conn, $userSql);
  if($thisDataResult) {
    $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
  } else {
    $rows = [];
  }
  return $rows;
}
 
function download($path, $friendlyName){
    $file = file_get_contents($path);
    header("Cache-Control: no-cache private");
    header("Content-Description: File Transfer");
    header('Content-disposition: attachment; filename='.$friendlyName);
    header("Content-Type: application/whatevs");
    header("Content-Transfer-Encoding: binary");
    header('Content-Length: '. strlen($file));
    echo $file;
    exit;
}

function genericSelect($id, $name, $defaultValue, $data, $event = "", $handler= "") {
	$out = "";
	$out .= "<select name='" . $name. "' id='" . $id . "' " . $event . "=\"" . $handler . "\">\n";
  $out .= "<option/>";
	foreach($data as $datum) {
		$value = gvfa("value", $datum);
		$text = gvfa("text", $datum);
    if(!$text) { //if the array is just a list of scalar values:
      $value = $datum;
      $text = $datum;
    }
		$selected = "";
		if($defaultValue == $value) {
			$selected = " selected='true'";
		}
		$out.= "<option " . $selected . " value=\"" . $value . "\">";
		$out.= $text;
		$out.= "</option>";
	}
	$out.= "</select>";
	return $out;
}
 

function stringToAscii($input) {
  $asciiCodes = [];
  $length = strlen($input);

  for ($i = 0; $i < $length; $i++) {
      echo $input[$i]  . " " . ord($input[$i]) . "<br>";
  }

  return $asciiCodes;
}

function eliminateExtraLinefeeds($input) {
  // Convert all line endings to Unix style (LF)inputinsertUpdateSql
  $input = str_replace("\r", "\n", $input);
  //$input = str_replace(chr(10), "X", $input);
  //$input = str_replace(["\r\n", "\r"], "\n", $input);
  //$input = str_replace(["\n\r", "\r"], "\n", $input);
  // Replace consecutive linefeeds with a maximum of two in a row
  $input = preg_replace("/\n{3,}/", "\n\n", $input);
  return $input;
}

function insertUpdateSql($conn, $tableName, $primaryKey, $data) {
  //this function is a serious mess and makes assumptions about your naming convention, but it can do amazing things, including add records to mapping tables to create many-to-many relationships
  //var_dump($_POST);
  $sql = "";
  $laterSql = "";
  $_dataRaw = gvfa("_data", $data); 
  $whereClauseString = "<whereclause/>";
  if($_dataRaw){
    $_data = json_decode($_dataRaw, true);
    $_data[] = array("name"=>"user_id", "type"=>"hidden");
  }
  // Check if a primary key is provided
  $sanitizedKeys = [];
  $dataToScan = $_data;
  $date = new DateTime("now", new DateTimeZone('America/New_York'));//obviously, you would use your timezone, not necessarily mine
  $formatedDateTime =  $date->format('Y-m-d H:i:s'); 
  if(!$_data){
    $dataToScan = $data;
    die();//don't worry about this case
  }
  $isInsert = true;
  if (!empty($primaryKey) && implode(",", array_values($primaryKey)) != "") {
    $isInsert = false;
  }
  foreach ($dataToScan as $datum) {
    $column = $datum["name"];
    $type =  strtolower(gvfa("type", $datum, ""));
    $value = gvfa($column, $data, "");
    if($column  != "_data"  && !array_key_exists($column, $primaryKey)) {
      //echo  $column . "=" . $value . ", " . $type . "<BR>";
      $skip = false;
      if($type == "many-to-many") {
        if ($isInsert) {
          $skip  = true;
        }
      } else if($type == "time" && $value == "") {
        $skip  = true;


      } else if($column == "last_known_device_modified" || $column == "created" || $column == "modified") {


        $sanitized = $formatedDateTime;
      } else if ($column == "expired"){
        $skip = true;
      } else if(($type == "bool"  || $type == "checkbox") && !$value){
        $sanitized = '0';
      } else if (beginsWith($type, "number") && !$value) {
        $sanitized = '0';
      } else {
        $sanitized = mysqli_real_escape_string($conn, $value);
      }
      if(!$skip ){
        $sanitizedData[] = $sanitized;
        //echo $column . "<BR>";
        //echo  $column . "=" . $sanitized . ", " . $type . "<BR>";
        $sanitizedKeys[] = $column;
      }
    }
  }
 
  if (!$isInsert) {
      // Update the existing record
      $sanitizedData = [];
      $updateFields = [];
      //$sanitizedKeys = [];
      foreach ($dataToScan as $datum) {
        $column = $datum["name"];
        $type =  strtolower(gvfa("type", $datum, ""));
        $value = gvfa($column, $data, "");

        //echo  $column . "=" . $value . ", " . $type . "<BR>";
        if($type == "many-to-many") {
          $mappingTable = gvfa("mapping_table", $datum);
          $countingColumn = gvfa("counting_column", $datum);
          $laterSql .= "\nDELETE FROM " . $mappingTable . " WHERE user_id='".  $data["user_id"] .  "' AND <whereclause/>;";
          $count = 1;
          $extraM2MColumns = "";
          $extraM2MValues = "";
          foreach($value as $valueItem){
            if($countingColumn) {
              $extraM2MColumns = ", " .$countingColumn;
              $extraM2MValues = ", " . $count;
            }
            $laterSql .= "\nINSERT INTO " . $mappingTable . "(user_id, " . implode(",", array_keys($primaryKey)) . "," . $column . ",created" . $extraM2MColumns  . ") VALUES('" . $data["user_id"] . "','" . implode("','", array_values($primaryKey)) . "','" . $valueItem . "','" .  $formatedDateTime . "'" . $extraM2MValues . ");\n";
            $count++;
          }
        } else if($column == "expired"  && $value == ""){
        } else if($type == "time" && $value == "") {
 


        } else if ($column != "created" && $column != "_data" && array_key_exists($column, $primaryKey) == false) {
          if(($type == "bool"  || $type == "checkbox") && !$value){
            $sanitized = '0';
          } else if (beginsWith($type, "number") && !$value) {
            $sanitized = '0';
          } else {
            $sanitized = mysqli_real_escape_string($conn, $value);
          }

          $updateFields[] = "`$column` =  '$sanitized'";
        }
      }

      $updateFieldsString = implode(', ', $updateFields);

      $whereClause = [];
      foreach ($primaryKey as $key => $value) {
          $whereClause[] = "`$key` = '$value'";
      }

      $whereClauseString = implode(' AND ', $whereClause);
      
      $sql .= "UPDATE `$tableName` SET $updateFieldsString WHERE $whereClauseString;";
  } else {
      // Insert a new record
      $columns = "`" . implode('`, `', $sanitizedKeys) . "`";
      $values = implode("', '", $sanitizedData);
      
      $sql .= "INSERT INTO `$tableName` ($columns) VALUES ('$values');";
  }
  $laterSql = str_replace("<whereclause/>", $whereClauseString, $laterSql);
  //die($sql . $laterSql );
  return $sql . $laterSql ;
}

function blendColors($color1, $color2) {
  // Convert hex colors to RGB
  $rgb1 = sscanf($color1, "#%02x%02x%02x");
  $rgb2 = sscanf($color2, "#%02x%02x%02x");

  // Blend the colors
  $result = "#";
  for ($i = 0; $i < 3; $i++) {
      $blended = ($rgb1[$i] + $rgb2[$i]) / 2;
      $result .= str_pad(dechex($blended), 2, '0', STR_PAD_LEFT);
  }
  return $result;
}

 
function encryptLongString($plaintext, $password) {
    // Generate a random initialization vector
    $iv = openssl_random_pseudo_bytes(openssl_cipher_iv_length('aes-256-cbc'));
    
    // Encrypt the plaintext using AES-256-CBC algorithm
    $ciphertext = openssl_encrypt($plaintext, 'aes-256-cbc', $password, 0, $iv);
 
    $iv = str_pad($iv, 16, "\0");
    // Encode the ciphertext and IV as base64 strings
    $ivBase64 = base64_encode($iv);
    $ciphertextBase64 = base64_encode($ciphertext);
    
    // Concatenate IV and ciphertext with a separator
    return $ivBase64 . ':' . $ciphertextBase64;
}

function decryptLongString($encryptedData, $password) {
    // Split the IV and ciphertext from the encrypted data
    list($ivBase64, $ciphertextBase64) = explode(':', $encryptedData, 2);
    
    // Decode the IV and ciphertext from base64 strings
    $iv = base64_decode($ivBase64);
    $ciphertext = base64_decode($ciphertextBase64);
    $iv = str_pad($iv, 16, "\0");
    //echo($iv . "|" . strlen($iv));
    // Decrypt the ciphertext using AES-256-CBC algorithm
    $plaintext = openssl_decrypt($ciphertext, 'aes-256-cbc', $password, 0, $iv);
    
    // Return the decrypted plaintext
    return $plaintext;
}

function isValidPHP($code) {
  // Wrap the code with PHP tags if they are not already present
  if (strpos($code, '<?php') === false) {
      $code = '<?php ' . $code;
  }
  
  // Use token_get_all to tokenize the PHP code
  $tokens = token_get_all($code);
  
  // Remove the first token if it's T_OPEN_TAG, to allow parsing in a block
  if ($tokens[0][0] === T_OPEN_TAG) {
      array_shift($tokens);
  }
  
  // Rebuild the code without the opening PHP tag
  $codeWithoutTags = '';
  foreach ($tokens as $token) {
      if (is_array($token)) {
          $codeWithoutTags .= $token[1];
      } else {
          $codeWithoutTags .= $token;
      }
  }

  // Use linting to check if the code is valid
  $tempFile = tempnam(sys_get_temp_dir(), 'php');
  file_put_contents($tempFile, '<?php ' . $codeWithoutTags);
  $result = shell_exec('php -l ' . escapeshellarg($tempFile));
  unlink($tempFile);

  return strpos($result, 'No syntax errors detected') !== false;
}

function getColumns($tableName) {
  Global $conn;
  $sql = "SHOW COLUMNS FROM " . filterStringForSqlEntities($tableName) . ";"; //watch out for sql injection!
  $result = mysqli_query($conn, $sql);
 
  if($result) {
    $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
    //var_dump($rows);
    $columnNames = array_column($rows, 'Field');
    return $columnNames;
  }
}

function deleteLink($table, $pkName) {
  $out = "<a onclick='return confirm(\"Are you sure you want to delete this " . $table . "?\")' href='?table=" . $table . "&action=delete&" . $pkName . "=<" . $pkName . "/>&hashed_entities=<hashed_entities/>'>Delete</a>";
  return $out;
}
