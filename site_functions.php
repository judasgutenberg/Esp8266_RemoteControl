<?php 

function utilityForm($user, $foundData) {
  $out = "";
  $frontEndJs = gvfa("front_end_js", $foundData);
  if(array_key_exists("form", $foundData)){
    $out .= "<div class='issuesheader'>" . $foundData["label"]. "</div>";
    $out .= "<div>" . $foundData["description"] . "</div>";
    $mergedData =  $foundData["form"];
    

    $form = genericForm($mergedData, "Run", "Running " . $foundData["label"], $user);
    $confirmJs = "";
    if(gvfa("skip_confirmation", $foundData) === false) {
      $confirmJs  = "onclick=\"return(confirm('Are you sure you want to run " . $foundData["label"] . "?'))\"";
    }
    $runOverride = gvfa("run_override", $foundData);
    if($runOverride){
      $form = str_replace("value='Run' type='submit'/>", "value='Run' type='button'  onclick='return(" . $runOverride  . ")'/>", $form);
    } else {
      $form = str_replace("value='Run' type='submit'/>", "value='Run' type='submit'  " . $confirmJs  . "/>", $form);
    }
    
    $out .= $form;
  } else if($frontEndJs) {
      $out .= "\n<div id='utilityDiv'></div>\n";
      $out .= "\n<script>\n";
      $out .=  $frontEndJs . ";";
      $out .= "\n</script>\n";
  }
  return $out;
}

//useful if you can't get sendmail working on this server
function remoteEmail($recipient, $message, $subject) {
  global $remoteEmailPassword;
  global $remoteEmailUrl;
  if($remoteEmailUrl) {
    $postData = [
      "password" => $remoteEmailPassword, //used to make sure your email sender elsewhere isn't used by spammers
      "email" => $recipient,
      "subject" => $subject,
      "body" => $message
    ];
    $url = $remoteEmailUrl;
    $ch = curl_init();

    curl_setopt($ch, CURLOPT_URL, $url);
    curl_setopt($ch, CURLOPT_POST, true);
    curl_setopt($ch, CURLOPT_POSTFIELDS, http_build_query($postData)); // Convert data to query string
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true); // Return the response
    $response = curl_exec($ch);
    curl_close($ch);
  } else {
    //if we don't have remote email set up, just use regular PHP mail
    $response = mail($recipient,  $subject, $message);
  }
  return $response;
}


function forgotPassword() {
  $out = "<div><a href='?action=login'>Normal login</a></div>";
  $formData = array(
    [
	    'label' => 'What is your email?',
      'name' => 'email',
      'type' => 'text'
	  ],
    [
	    'label' => 'Your email',
      'name' => 'action',
      'type' => 'hidden',
      'value' => "forgotpassword"
	  ]
  );
  $form = genericForm($formData, "Reset Password", "Resetting Password");
 
  $out .= "\n<div id='utilityDiv'>Forgot your password?</div>\n";
  $out .= $form;
  $out .= "\n<div id='utilityDiv'></div>\n";
  return $out;
}

function sendPasswordResetEmail($email){
  Global $conn;
  $token = sprintf("%08x", random_int(0, 0xFFFFFFFFFFFF));
  $sql = "UPDATE user  SET reset_password_token ='" . mysqli_real_escape_string($conn, $token) . "' WHERE email = '" . mysqli_real_escape_string($conn, $email) . "'";
  $result = mysqli_query($conn, $sql);
  $emailBody = "Follow this link to reset your password:\n\r\n\r ";
  $emailBody .= getCurrentUrl() . "&token=" . $token . "&email=" . $email;
  return remoteEmail($email, $emailBody, "Reset Your Email on " . $_SERVER['SERVER_NAME']);
  //echo $emailBody;
}

function changePasswordForm($email, $token, $errors){
  $out = "";
  $formData = array(
    [
	    'label' => 'password',
      'name' => 'password',
      'type' => 'password',
      'error' => gvfa("password", $errors),
	  ],
    [
      'label' => 'password (again)',
      'name' => 'password2',
      'type' => 'password'
	  ],
    [
      'name' => 'token',
      'type' => 'hidden',
      'value' => $token
	  ],
    [
      'name' => 'email',
      'type' => 'hidden',
      'value' => $email
	  ]
  );
  $form = genericForm($formData, "Change Password", "Changing Password");
  $out = "\n<div id='utilityDiv'>Change your password</div>\n";
  $out .= $form;
  $out .= "\n<div id='utilityDiv'></div>\n";
  return $out;
}

function getCurrentUrl() {
  $protocol = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off') ? "https" : "http";
  $host     = $_SERVER['HTTP_HOST'];
  $uri      = $_SERVER['REQUEST_URI'];
  return "$protocol://$host$uri";
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
  //echo $key;
  $data =  utilities($user, "data");
  //var_dump($data);
  //echo "$" . $key . "$";
 
  $searchValue = $key;
  $resultArray = array_filter($data, function ($subData) use ($searchValue) {
      return $subData["key"] == $searchValue;
  });
  $foundData = reset($resultArray);
 
  return $foundData;
}

function bodyWrap($content, $user, $deviceId, $poser = null) {
  global $timezone;
  $out = "<!doctype html>";
  $out .= "<html>\n";
  $out .= "<head>\n";
  $out .= '<link rel="icon" type="image/x-icon" href="./favicon.ico" />';

  $siteName = "Remote Controller";
  $version = filemtime("./index.php");
  $out .= "<script>window.timezone ='" .  $timezone . "'</script>\n";
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
    $out .= "<div class='loggedin'>You are logged in as <b>" . $user["email"] . "</b>" .  $poserString . "  on " . $user["name"] . " <div class='basicbutton'><a href=\"?action=logout\">logout</a></div></div>\n";
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
  $out .= "</div>\n";
  $out .= "</body>\n";
  $out .= "</html>\n";
  return $out;
}

function filterCommasAndDigits($input) {
  // Use a regular expression to keep only commas and digits
  return preg_replace('/[^,\d]/', '', $input);
}

function filterStringForSqlEntities($input) {
  // Replace characters that are not letters, numbers, dashes, or underscores with an empty string
  $filtered = preg_replace('/[^a-zA-Z0-9-_]/', '', $input);

  return $filtered;
}

function autoLogin() {
  Global $cookiename;
  Global $tenantCookieName;
  //var_dump($_COOKIE);
  if(!isset($_COOKIE[$cookiename]) || !isset($_COOKIE[$tenantCookieName])) {
    //die("wuuup");
    return false;
  } else {
   $cookieValue = $_COOKIE[$cookiename];
   
   $email = siteDecrypt($cookieValue);
   $tenantId = siteDecrypt($_COOKIE[$tenantCookieName]);
 
   if(strpos($email, "@") > 0){
      //die("yeee!!");
      return getUser($email, $tenantId);
      
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
  Global $tenantCookieName;
  setcookie($cookiename, "");
	setcookie($tenantCookieName, "");
	return false;
}
 
function loginForm() {
  $out = "";
  $out .= "<form method='post' name='loginform' id='loginform'>\n";
  $out .= "<strong>Login here:</strong>  email: <input name='email' type='text'>\n";
  $out .= "password: <input name='password' type='password'>\n";
  $out .= "<input name='tenant_id' value='" . gvfa("tenant_id", $_GET). "' type='hidden'>\n";
  $out .= "<input name='action' value='login' type='submit'>\n";
  $out .= "<div> or  <div class='basicbutton'><a href=\"tool.php?table=user&action=startcreate\">Create Account</a></div> (<a href=\"tool.php?action=forgotpassword\">Forgot password?</a>) </div>\n";
  $out .= "</form>\n";
  return $out;
}

function newUserForm($error = NULL, $encryptedTenantId = NULL) {
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
  if($encryptedTenantId) {
    if(!$encryptedTenantId) {
      $encryptedTenantId = gvfw("encrypted_tenant_id");
    }
    $formData[] =   [
      'name' => 'encrypted_tenant_id',
      'type' => 'hidden',
      'value' =>  $encryptedTenantId,
      'error' => gvfa('encrypted_tenant_id', $error)
    ];
  }
  $out = genericForm($formData, "create user");
  $out.= "<div style='padding-left:180px'><a href='?action=login'>Go back to login</a></div>";
  return $out;
}

function presentList($data) {
  $out = "<ul class='utilitylist'>\n";
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
      if($fieldName != "tenant_id") {
        $record = ["label" => $fieldName, "name" => $fieldName, "type" => $type ];
        if($type == "bool" || $type == "number" ){
          $record["changeable"] = true;
        }
        $headerData[] = $record;
      }
      
    }
  }
  return $headerData;
}

function generateCsvContent(array $dataRows) {
  // Open a memory stream to store CSV data
  $output = fopen('php://temp', 'r+');
  // Get headers from the first row and write to CSV
  if (!empty($dataRows)) {
      fputcsv($output, array_keys($dataRows[0]));
  }
  // Write each row of data to CSV
  foreach ($dataRows as $row) {
      fputcsv($output, $row);
  }
  // Rewind the memory stream and fetch the contents
  rewind($output);
  $csvContent = stream_get_contents($output);
  // Close the memory stream
  fclose($output);
  return $csvContent;
}

function genericEntityList($tenantId, $table, $outputFormat = "html") {
  Global $conn;
  $headerData = schemaArrayFromSchema($table, $pk);
  $additionalValueQueryString = "";
  foreach($_REQUEST as $key=>$value){ //slurp up values passed in
    if(endsWith($key, "_id")){
      $additionalValueQueryString .= "&" . $key . "=" . urlencode($value);
    }
  }
  $thisDataSql = "SELECT * FROM " . $table;
  if($table != "tenant"  && $table != "user") {
    $thisDataSql .= " WHERE tenant_id=" . intval($tenantId);
  }
  $deviceId = gvfw("device_id");
  if($deviceId && $table== "device_feature" ){
    $thisDataSql .= " AND device_id=" . intval($deviceId);
  }
  $thisDataResult = mysqli_query($conn, $thisDataSql);
  $out = "<div class='listtools'><div class='basicbutton'><a href='?table=" . $table . "&action=startcreate" . $additionalValueQueryString . "'>Create</a></div> a new " . $table . "<//div>\n";
  if($thisDataResult) {
    $thisDataRows = mysqli_fetch_all($thisDataResult, MYSQLI_ASSOC); 
    $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a>";
    $toolsTemplate .= " | " . deleteLink($table, $table. "_id" ); 
    if(strtolower($outputFormat) == "csv") {
      $content = generateCsvContent($thisDataRows);
      download($path, $friendlyName, $content = "");
      die();
    } else {
      $out .= genericTable($thisDataRows, $headerData, $toolsTemplate, null, $table, $pk);
    }
  }
  return $out;
}

function genericEntityForm($tenantId, $table, $errors){
  Global $conn;
  $data = schemaArrayFromSchema($table, $pk);
  $pkValue =  gvfa($pk, $_GET);
  $thisDataSql = "SELECT * FROM " . $table . " WHERE " . $pk . " = '" . $pkValue . "'";
   
  if($table != "user"  && $table != "tenant") {
    $thisDataSql .= " AND tenant_id=" . intval($tenantId);
  }
  $thisDataResult = mysqli_query($conn, $thisDataSql);
  if($thisDataResult) {
    $thisDataRows = mysqli_fetch_all($thisDataResult, MYSQLI_ASSOC);
    if($thisDataRows && count($thisDataRows) > 0) {
      $data = updateDataWithRows($data, $thisDataRows[0]);
    }
  }
  return genericForm($data, "Save " . $table, "Saving...");
}

 
function genericEntitySave($user, $table) {
  Global $conn;
  Global $encryptionPassword;
  $tenantId = $user["tenant_id"];
  $tablesThatRequireUser = tablesThatRequireUser();
  //$data = schemaArrayFromSchema($table, $pk);
  $pk = $table . "_id";
  $data = $_POST;
  if(array_key_exists("password", $data) &&  (array_key_exists("_new_password", $data) &&  gvfa("_new_password", $data) == true)){
    $data["password"] =  crypt($data["password"], $encryptionPassword);
  }
  if(in_array($table, $tablesThatRequireUser)){
    $data["user_id"] = $user["user_id"];
  }
  unset($data['action']);
  unset($data[$pk]);
  unset($data['created']);
  if($table != "user"  && $table != "tenant") {
    $data["tenant_id"] = $tenantId;
  } else {
    //unset($data["tenant_id"]);
  }
 
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
  $url = "?table=" . $table;
  $deviceId = gvfw("device_id");
  if($deviceId && $table != "device"){
    $url .=  "&device_id=" . $deviceId;
  }
  header("Location: " . $url);
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

function genericForm($data, $submitLabel, $waitingMesasage = "Saving...", $user = null, $onload = "") { //$data also includes any errors
  Global $conn;
  $textareaIds = [];
	$out = "";
  $noWaiting = false;
  $onSubmitManyToManyItems = [];
  $out .= "<script>\n";
  $out .= "let formSpec = " . json_encode($data) . ";";
  $out .= "</script>\n";
	$out .= "<div class='genericform'>\n";
  $columnCount = 0;
  if(!$data){
    return;
  }
	foreach($data as &$datum) {
		$label = gvfa("label", $datum);
    $frontendValidation = gvfa("frontend_validation", $datum);
    $validationString = "";
    if($frontendValidation){
      $validationString = ' onblur=\" . $frontendValidation . "\"';
    }
		$value = str_replace("\\\\", "\\", gvfa("value", $datum)); 
    //var_dump($datum);
		$name = gvfa("name", $datum); 
    $changeFunction = gvfa("change-function", $datum); 
		$type = strtolower(gvfa("type", $datum)); 
    $accentColor = gvfa("accent_color", $datum, "#66eeee");
    //echo $name .  " " . $accentColor . "<BR>";
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
    $noSyntaxHighlighting = false;
    $noSyntaxHighlighting =gvfa("no_syntax_highlighting", $datum); 
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
      } else if($type == 'multiselect') {
        foreach($values as $specificValue){
          $checkPart = "";
          if(is_array($value)) {
            foreach($value as $selectedValue){
              if($selectedValue == $specificValue){
                $checkPart  = " checked='checked'";
              }
            }

          }
          $out .= "<input style='accent-color:" .  $accentColor . "' type='checkbox' name='" . $name . "[]' value='" . $specificValue . " " . $checkPart . "'/> " . $specificValue . "<br/>";
        }
      } else if($type == 'select') {
        //echo $values;
        $onChangePart = "";
        if($changeFunction) {
          $onChangePart = " onchange=\"" . $changeFunction . "\" ";
        }
        $out .= "<select " . $onChangePart. " name='" . $name . "' />";
        if(is_string($values)) {
          $out .= "<option value='0'>none</option>";
          //var_dump($user);
          if($user) {
            $values = tokenReplace($values, $user); //I'd has something embarrassingly hardcoded here until I had $user available
          }
 
          $result = mysqli_query($conn, $values); //REALLY NEED TO SANITIZE $values since it contains RAW SQL!!!
          
   
          if($result){
            $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
  
            if($rows){
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
          }
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
        $out .= "<select style='accent-color:" .  $accentColor . "'  multiple='multiple' name='" . $name . "[]' id='dest_" . $name . "' size='" . intval($height) . "'/>";
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
        $out .= "<select style='accent-color:" . $accentColor . "' name='source_" . $name . "' id='source_" . $name . "' size='" . intval($height) . "'/>";
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
        $out .= "<input style='accent-color:" .  $accentColor . "' value='1' name='" . $name . "'  " . $checked . " type='checkbox'/>\n";
      } else if ($type == "read_only"){
        $out .= $value . "\n";
      } else {
        if($height){
          $textAreaId = "id-" . $name;
          $idString = "id='" . $textAreaId  . "'";
          if(!$noSyntaxHighlighting) {
            array_push($textareaIds, $textAreaId);
          }
          $codeLanguage = gvfa("code_language", $datum, "html");
          $out .= "<textarea " . $validationString . " " .  $idString . " style='width:" . $width . "px;height:" . $height . "px;accent-color:" . $accentColor . "' name='" . $name . "'  />" .  $value  . "</textarea>\n";
        } else {
          $specialNumberAttribs = "";
          if ($type == "number") {
            $specialNumberAttribs = " step='0.0000000000001' ";
          }
          if ($type == "int") {
            $type == "number";
            $specialNumberAttribs = " step='1' ";
          }
          $inputJavascript = "";
          if($type == "plaintext_password") {
            $inputJavascript = "onchange=\"document.getElementById('_new_password').checked=true\"";
          }
          $out .= "<input " . $inputJavascript  . " " . $validationString . " style='width:" . $width . "px;accent-color:" . $accentColor . "' " . $idString. " " . $specialNumberAttribs . "  name='" . $name . "' value=\"" .  $value . "\" type='" . $type . "'/>\n";
          if($type == "plaintext_password") {
            $out .= "<input id='_new_password' name='_new_password' value='1' type='checkbox'>\n password not yet encrypted";
          }
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
  
  $onSubmit = "onsubmit='formSubmitTasks();startWaiting(\"" . $waitingMesasage . "\")'";
 
  $out = "<form name='genericForm' " . $onSubmit . " method='post' name='genericform' id='genericform' enctype='multipart/form-data'>\n" . $out;
  if(count($textareaIds) > 0){
    
    $out .= "<script src=\"./tinymce/tinymce.min.js\" referrerpolicy=\"origin\"></script>\n";
    /*
    $out .= "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.7.0/styles/default.min.css\">\n";
    $out .= "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.7.0/highlight.min.js\"></script>\n";
       */
      /*
    $out .= "<link href=\"https://cdnjs.cloudflare.com/ajax/libs/prism/1.27.0/themes/prism.min.css\" rel=\"stylesheet\" />\n";
    $out .= "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/prism/1.27.0/prism.min.js\"></script>\n";
    $out .= "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/prism/1.27.0/plugins/line-numbers/prism-line-numbers.min.js\"></script>\n";
    $out .= "<link href=\"https://cdnjs.cloudflare.com/ajax/libs/prism/1.27.0/plugins/line-numbers/prism-line-numbers.min.css\" rel=\"stylesheet\" />\n";
    $out .= "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/prism/1.27.0/components/prism-sql.min.js\"></script>\n";
    */
    $out .= "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/codemirror/5.65.5/codemirror.min.css\">\n";
    $out .= "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/codemirror/5.65.5/codemirror.min.js\"></script>\n";
    $out .= "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/codemirror/5.65.5/mode/sql/sql.min.js\"></script>\n";
    $out .= "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/codemirror/5.65.5/mode/javascript/javascript.min.js\"></script>\n";
    $out .= "\n<script>\n";
    $out.= "let textAreaCount = 0;\n";
    $out.= "let textareaIds = ['" . implode("','", $textareaIds) . "'];\n";
    $out .= "\ntextareaIds.forEach(id => {\n";
    $out .= "let textArea = document.getElementById(id);\n";
    $out .= "let textAreaName = textArea.name;\n";
    $out .= "let formItemInfoRecord = findObjectByName(formSpec, textAreaName);\n";
    $out .= "let codeLanguage = formItemInfoRecord[\"code_language\"];\n";
    $out .= "formattedCode = textArea.value;\n";
    $out .= "mode = 'text/x-html';";
    $out .= "if (codeLanguage == 'sql'){;\n";
    $out .= " formattedCode = formatSQL(textArea.value);\n";
    $out .= " mode = 'text/x-' + codeLanguage;\n";
    $out .= "}\n;";
    $out .= "if (codeLanguage == 'json'){;\n";
    $out .= " formattedCode = formatJSON(textArea.value);\n";
    $out .= " mode = 'application/' + codeLanguage;\n";
    $out .= "}\n;";
    $out .= "textArea.value = formattedCode;\n";
    $out .= "let editor = CodeMirror.fromTextArea(textArea, {\n";
    $out .=  "lineNumbers: true,\n";
    $out .= " mode:     mode,\n";
    $out .= " theme: \"default\",\n";
    $out .= "tabSize: 2,\n";
    $out .= "indentWithTabs: true,\n";
    $out .= "smartIndent: true,\n";
    $out .= "autoCloseBrackets: true,\n";
    $out .= "lineWrapping: true\n";
    $out .= "});\n";
    $out .= "editor.setSize(formItemInfoRecord['width'] + 'px', formItemInfoRecord['height'] + 'px');\n";

    $out .= "editor.on('blur', (cm) => {
          console.log('Editor lost focus');
          // Handle your blur event here
          
          console.log(formItemInfoRecord);
          editor.save(); 
          eval(formItemInfoRecord[\"frontend_validation\"]);
          //console.log('CodeMirror content:', value);
      });\n";



    $out.= "textAreaCount++;\n";
    $out .= "});\n";


    //$out.= "setTimeout(()=>{\n";
      /*
    $out .= "\ntextareaIds.forEach(id => {\n";
      $out .= "\n  tinymce.init({\n";
        $out .= "\nselector: `#$" . "{id}`,\n";
        $out .= "\nbranding: false,\n";
        $out .= "\nforce_br_newlines : true,\n";
        $out .= "\nlicense_key: 'gpl' ,\n";
        $out .= "\npromotion: false,\n";

        $out .= "\nplugins: 'codesample code',\n";
        $out .= "\ntoolbar: 'codesample code',\n";
 
        $out .= "codesample_languages: [
          {text: 'HTML/XML', value: 'markup'},
          {text: 'JavaScript', value: 'javascript'},
          {text: 'CSS', value: 'css'},
          {text: 'PHP', value: 'php'},
          {text: 'Ruby', value: 'ruby'},
          {text: 'Python', value: 'python'},
          {text: 'Java', value: 'java'},
          {text: 'C', value: 'c'},
          {text: 'C#', value: 'csharp'},
          {text: 'C++', value: 'cpp'},
          {text: 'SQL', value: 'sql'}
      ],";
      */
            /*
        $out .= "\nsetup: function(editor) {\n";
          $out .= "\neditor.on('change', function() {\n";
            $out .= "\neditor.save();\n";
            $out .= "\n  document.querySelectorAll('pre code').forEach((block) => {\n";
              $out .= "\n   hljs.highlightElement(block);\n";
              $out .= "\n  });\n";
              $out .= "\n});\n";
              $out .= "\n }\n";
              $out .= "\n});\n";
              $out .= "\n });\n";
        
    */
    /*
        $out .= "\nsetup: function(editor) {\n";
          $out .= "\n  editor.on('init', function() {\n";
            $out .= "\n  editor.on('NodeChange', function(e) {\n";
              $out .= "\nconsole.log('cccc');\n";
              $out .= "\n  // Highlight the code block using Prism.js when the editor content changes\n";
              $out .= "\n  Prism.highlightAllUnder(editor.getBody());\n";
              $out .= "\n });\n";
              $out .= "\n});\n";
              $out .= "\n}\n";
              $out .= "\n});\n";
              $out .= "\n });\n";

              */
        //$out.= "\n}, 8000)\n";
    $out .= "\n</script>\n";
  }
  if($onload){
    $out .= "\n<script>" . $onload . "</script>\n";
  }
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

//needs to be made so users and tenants can be many to many
//also returns tenant information
function getUser($email, $tenantId = null) {
  Global $conn;
  $user = null;
  $sql = "SELECT * FROM `user` WHERE email = '" . mysqli_real_escape_string($conn, $email) . "'";
  $result = mysqli_query($conn, $sql);
  if($result){
    $user = $result->fetch_assoc();
    $role = $user["role"];
    $sql = "SELECT * FROM `tenant_user` tu JOIN tenant t ON tu.tenant_id=t.tenant_id WHERE user_id = '" . $user["user_id"] . "'";
    if($tenantId){
      $sql .= " AND t.tenant_id = " . intval($tenantId);
    }
    //echo $sql;
    $result = mysqli_query($conn, $sql);
    $user["tenants"] = null;
    if($result){
      $tenants = mysqli_fetch_all($result, MYSQLI_ASSOC);
      //var_dump($tenants);
      if(count($tenants) > 0) {
        $tenant = $tenants[0];
        $tenant["role"] = $role;//a temporary hack that will probably be good for awhile -- overwrite the mapping table role with the user role;
        $user["tenants"] = $tenants;
        $user  = array_merge($user, $tenant);
      }
    }
  }
  //var_dump($user);
  return $user;
}

function impersonateUser($impersonatedUserId) {
  Global $poserCookieName;
  setcookie($poserCookieName, siteEncrypt($impersonatedUserId), time() + (30 * 365 * 24 * 60 * 60));
  header("location: .");
}

function getImpersonator($justId = true){
  Global $poserCookieName;
  $poserId = siteDecrypt(gvfa($poserCookieName, $_COOKIE));
  //die($poserCookieName . " " . $poserId );
  if($justId){
    return $poserId;
  }
  return getUserById($poserId);
}

function setTenant($encryptedTenantId){
  Global $tenantCookieName;
  setcookie($tenantCookieName, $encryptedTenantId, time() + (30 * 365 * 24 * 60 * 60));
  header('Location: '.$_SERVER['PHP_SELF']);
}

function availableRoles(){
  return ["", "viewer", "operator", "subadmin", "admin", "super"];
}

function loginUser($source = NULL, $tenant_id = NULL) {
  Global $conn;
  Global $encryptionPassword;
  Global $cookiename;
  Global $tenantCookieName;
  if($source == NULL) {
  	$source = $_REQUEST;
  }
  $email = gvfa("email", $source);
  $passwordIn = gvfa("password", $source);
  $sql = "SELECT `email`, `password`, t.tenant_id, name as tenant_name, about FROM `user` u JOIN tenant_user tu ON u.user_id=tu.user_id JOIN tenant t ON tu.tenant_id = t.tenant_id WHERE email = '" . mysqli_real_escape_string($conn, $email) . "' AND (u.expired IS NULL OR u.expired>NOW()) AND (t.expired IS NULL OR t.expired>NOW()) ";

  if($tenant_id){
    $sql .= " AND t.tenant_id = " . $tenant_id;
  }
  //die($sql);
  $result = mysqli_query($conn, $sql);
  if(!$result){
    header("location: .");
    die();

  }
  //try{
    $row  = NULL;
    $iv = "0x12345678123456";
    $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
    //var_dump( $rows);
    if(count($rows) > 1) {
      $out ="<div class='tenantpicker'>\n";
      $out .="<div class='listtitle'>Pick a Tenant</div>\n";
      $row = $rows[0];
      $email = $row["email"];
      $passwordHashed = $row["password"];
      if (password_verify($passwordIn, $passwordHashed)) {
        //die("we are setting a cookie");
        setcookie($cookiename, siteEncrypt($email), time() + (30 * 365 * 24 * 60 * 60));
        foreach($rows as &$row) {
          $out .="<div><a href='?action=settenant&encrypted_tenant_id=" . urlencode(siteEncrypt($row["tenant_id"])). "'>" .$row["tenant_name"] . "</a> <div class='description'>" . $row["about"]  . "</div></div>";

        }
        //echo $out;
        return $out;
      }
      $out .="</div>\n";
    } else if($rows && count($rows) > 0) {
      $row = $rows[0];
    }
    if($row  && $row["email"] && $row["password"]) {
      //$tenant_id = $row["tenant_id"];
      $email = $row["email"];
      $passwordHashed = $row["password"];
      //die($passwordHashed . "*" . $passwordIn);
      //for debugging:
      //echo crypt($passwordIn, $encryptionPassword);
      //die(crypt("public", $encryptionPassword) . "*" . $passwordIn . "*" . crypt($passwordIn, $encryptionPassword) . "*" . $passwordHashed . "*" .password_verify($passwordIn, $passwordHashed) . "*");
      if (password_verify($passwordIn, $passwordHashed)) {
          setcookie($cookiename, siteEncrypt($email), time() + (30 * 365 * 24 * 60 * 60));
          setcookie($tenantCookieName, siteEncrypt($tenant_id), time() + (30 * 365 * 24 * 60 * 60));
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

function siteEncrypt($text){
  Global $encryptionPassword;
  $ivLength  = openssl_cipher_iv_length('AES-128-CTR');
  $iv = openssl_random_pseudo_bytes($ivLength);
  $out = base64_encode($iv . openssl_encrypt($text , "AES-128-CTR", $encryptionPassword, 0, $iv));
  return $out;
}

function siteDecrypt($encrytedText){
  Global $encryptionPassword;
  $ivLength = openssl_cipher_iv_length('AES-128-CTR');
  $data = base64_decode($encrytedText);
  
  $iv = substr($data, 0, $ivLength);
  $encrypted = substr($data, $ivLength);
  //echo $encrytedText . "|" . $iv . "|" . $encrypted ;
  // Decrypt the data
  return openssl_decrypt($encrypted, 'AES-128-CTR', $encryptionPassword, 0, $iv);
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
  $out = "\n<div class='nav'>\n";
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
    $out .= "\n<a href='./" . $url . "'><div class='" . $class . "'>" . $label . "</div></a>\n";
    if($rpad) {
      $out .= "<span style='padding-right:" . $rpad . "px;width:10px;height:12px'></span>\n";
    }
  }
  $out .= "</div>\n";
  return $out;

}

function tabNav($user) {
  $etcTables = array("device_type", "feature_type", "device_type_feature", "management_rule", "ir_pulse_sequence","command","command_type","tenant", "user");
	$tabData = array(
 
  [
    'label' => 'Devices',
    'table' => 'device' 
  ],
  [
    'label' => 'Device Features',
    'table' => 'device_feature' 
  ],
  [
    'label' => 'Sensor Data',
    'table' => 'sensors' 
  ],
  [
    'label' => 'Reports',
    'table' => 'report' 
  ]
  
	);
  if($user && ($user["role"] == "super" || $user["role"] == "admin")){
    $tabData[] =   [
      'label' => 'Utilities',
      'table' => 'utilities' 
    ];
  }
  $tabData[] =
    [
      'label' => 'Etc',
      'table' => 'etc' 
    ];
	$out = "<div class='nav'>";
  $currentMode = gvfa('table', $_REQUEST);
  $deviceId = gvfa('device_id', $_REQUEST);
 
  foreach($tabData as &$tab) {

    if($currentMode == "") {
      $currentMode  = "device";
    } else if ($currentMode == "etc"){
      //$currentMode  = "device_type_feature";
    }
    $label = gvfa("label", $tab);
    $table = gvfa("table", $tab); 
    $class = "navtab";
    if($currentMode == $table || array_search($currentMode, $etcTables) !== false && $table == "etc") {
      $class = "navtabthere";
    }
    $url = "?table=" . $table;
    if($table!= "device"){
      $url .= "&device_id=" . $deviceId;
    }
    
    $out .= "\n<a href='" . $url . "'><div class='" . $class . "'>" . $label . "</div></a>\n";
  }
  $out .= "</div>\n";
  
  if($currentMode == "etc" || array_search($currentMode, $etcTables) !== false) {
    $out .="<br/>";
    $out .= etcTabNav($user);
  }
  return $out;
}


function etcTabNav($user) {
  $tabData = array(
 
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
    ] 
    ,
    [
      'label' => 'Management Rules',
      'table' => 'management_rule' 
    ]
    );
    if($user && ($user["role"] == "super" || $user["role"] == "admin")){
      $tabData[] =   [
        'label' => 'IR Sequences',
        'table' => 'ir_pulse_sequence' 
      ];
      $tabData[] =   [
        'label' => 'Command',
        'table' => 'command' 
      ];
      $tabData[] =   [
        'label' => 'Command Type',
        'table' => 'command_type' 
      ];
      $tabData[] =   [
        'label' => 'Tenants',
        'table' => 'tenant' 
      ];
    }
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
      if($currentMode == $table || $table=="device_type_feature" && $currentMode == "etc") {
        $class = "navtabthere";
      }
      $url = "?table=" . $table;
      if($table!= "word_list"){
        //$url .= "&word_list_id=" . $wordListId ; //too messy
      }
      if($table!= "device"){
        $url .= "&device_id=" . $deviceId;
      }
      
      $out .= "\n<a href='" . $url . "'><div class='" . $class . "'>" . $label . "</div></a>\n";
    }
    $out .= "</div>\n";
    return $out;

}

//a work in progress:
function genericTableViaJs($rows, $headerData = NULL, $toolsTemplate = NULL, $searchData = null, $tableName = "", $primaryKeyName = "", $autoRefreshSql = null, $tableTools = '') { //aka genericList
  Global $encryptionPassword;
  if($headerData == NULL  && $rows  && $rows[0]) {
    $headerData = [];
    foreach(array_keys($rows[0]) as &$key) {
      array_push($headerData, array("label"=>$key, "name"=>$key));
    }
  }
  if(!$headerData){
    $headerData = [];
  }
  $out = "";

  if($searchData) {
    $out .= "<script>\n";
    $out .= "let rows = " . json_encode($rows) . ";";
    $out .= "</script>\n";
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
  $tableId = "list-" . $tableName;
  $out .= "<div class='list' id='" . $tableId . "'>\n";

  $out .="<div class='listheader'>\n";
  $cellNumber = 0;
 
  foreach($headerData as &$headerCell) {
  	$out.= "<span class='headerlink' onclick='sortTable(event, " . $cellNumber . ")'>" . $headerCell['label'] . "</span>\n";
    $cellNumber++;
  }
  $out.= "<span class='headertool'>" .  $tableTools . "</span>\n";
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
      $accentColor = gvfa("accent_color", $headerItem, "#66eeee");
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
      if (gvfa("changeable", $headerItem)) {
        if($row[$name] == 1){
          $checkedString = " checked ";
          
        }

        if(($type == "color"  || $type == "text"  || $type == "number" || $type == "string") &&  $primaryKeyName != $name){
          $hashedEntities =  crypt($name . $tableName .$primaryKeyName  . $row[$primaryKeyName] , $encryptionPassword);
          $out .= "<input style='width:55px;accent-color:" . $accentColor. "' onchange='genericListActionBackend(\"" . $name . "\",  this.value ,\"" . $tableName  . "\",\"" . $primaryKeyName  . "\",\"" . $row[$primaryKeyName] . "\",\""  . $hashedEntities . "\")' value='" . $value . "'  name='" . $name . "' type='" . $type . "' />\n";
        } else if(($type == "checkbox" || $type == "bool")  &&  $primaryKeyName != $name) {
          $hashedEntities =  crypt($name . $tableName .$primaryKeyName  . $row[$primaryKeyName] , $encryptionPassword);
          $out .= "<input style='width:55px;accent-color:" . $accentColor. "' onchange='genericListActionBackend(\"" . $name . "\",this.checked,\"" . $tableName  . "\",\"" . $primaryKeyName  . "\",\"" . $row[$primaryKeyName] . "\",\""  . $hashedEntities . "\")' name='" . $name . "' type='checkbox' value='1' " . $checkedString . "/>\n";
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
    $out .= "<script>autoUpdate('" . $encryptedSql . "','" . addslashes(json_encode($headerData)) . "','" . $tableId . "');</script>";

  }
  return $out;
}

 

function genericTable($rows, $headerData = NULL, $toolsTemplate = NULL, $searchData = null, $tableName = "", $primaryKeyName = "", $autoRefreshSql = null, $tableTools = '') { //aka genericList
  Global $encryptionPassword;
  if($headerData == NULL  && $rows  && $rows[0]) {
    $headerData = [];
    foreach(array_keys($rows[0]) as &$key) {
      array_push($headerData, array("label"=>$key, "name"=>$key));
    }
  }
  if(!$headerData){
    $headerData = [];
  }
  $tableId = "list-" . $tableName;
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
  $out .= "\n<div class='list' id='" . $tableId . "'>\n";

  $out .="<div class='listheader'>\n";
  $cellNumber = 0;
 
  foreach($headerData as &$headerCell) {
  	$out.= "<span class='headerlink' onclick='sortTable(event, " . $cellNumber . ")'>" . $headerCell['label'] . "</span>\n";
    $cellNumber++;
  }
  $out.= "<span class='headertool'>" .  $tableTools . "</span>\n";
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
      $accentColor = gvfa("accent_color", $headerItem, "#66eeee");
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
          eval('$value = ' . $function . ";");
        }
        catch(Exception  $err){
          //echo $err;

        }
        //echo $value . "<P>";
      }

      //echo $value . "=<P>";
      if (gvfa("changeable", $headerItem)) {
        if($row[$name] == 1){
          $checkedString = " checked ";
          
        }

        if(($type == "color"  || $type == "text"  || $type == "number" || $type == "string") &&  $primaryKeyName != $name){
          $hashedEntities =  crypt($name . $tableName .$primaryKeyName  . $row[$primaryKeyName] , $encryptionPassword);
          $out .= "<input style='width:55px;accent-color:" . $accentColor. "' onchange='genericListActionBackend(\"" . $name . "\",  this.value ,\"" . $tableName  . "\",\"" . $primaryKeyName  . "\",\"" . $row[$primaryKeyName] . "\",\""  . $hashedEntities . "\")' value='" . $value . "'  name='" . $name . "' type='" . $type . "' />\n";
        } else if(($type == "checkbox" || $type == "bool")  &&  $primaryKeyName != $name) {
          $hashedEntities =  crypt($name . $tableName .$primaryKeyName  . $row[$primaryKeyName] , $encryptionPassword);
          $out .= "<input style='width:55px;accent-color:" . $accentColor. "' onchange='genericListActionBackend(\"" . $name . "\",this.checked,\"" . $tableName  . "\",\"" . $primaryKeyName  . "\",\"" . $row[$primaryKeyName] . "\",\""  . $hashedEntities . "\")' name='" . $name . "' type='checkbox' value='1' " . $checkedString . "/>\n";
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
    $out .= "<script>autoUpdate('" . $encryptedSql . "','" . addslashes(json_encode($headerData)) . "','" . $tableId . "');</script>";

  }
  return $out;
}

function tokenReplace($template, $data,  $tableName = "", $strDelimiterBegin = "<", $strDelimiterEnd = "/>"){
  Global $encryptionPassword;
  //var_dump($data);
  foreach($data as $key => $value) {
    if(!is_array($value)) {
      $template = str_replace($strDelimiterBegin . $key . $strDelimiterEnd, $value, $template);
    } else {
      if(array_is_list($value) && count($value) > 0 && is_array($value[0]) == false) {
        $values = implode(",", $value);
        $template = str_replace($strDelimiterBegin . $key . $strDelimiterEnd, $values, $template);
      }
    }
  }
  if($tableName!= "") {
    $hashedEntities = crypt($tableName . $tableName . "_id" . $data[$tableName . "_id"], $encryptionPassword);
    $template = str_replace($strDelimiterBegin . "hashed_entities" . $strDelimiterEnd, $hashedEntities, $template);
  }
  return $template;
}

if (!function_exists('array_is_list')) {
  function array_is_list(array $arr)
  {
      if ($arr === []) {
          return true;
      }
      return array_keys($arr) === range(0, count($arr) - 1);
  }
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

function formatBytes($bytes, $precision = 2) {
  $units = ['bytes', 'kilobytes', 'megabytes', 'gigabytes', 'terabytes'];
  if ($bytes == 0) {
      return '0 bytes';
  }
  $base = log($bytes, 1024);
  $unitIndex = (int) floor($base);
  $size = round(pow(1024, $base - $unitIndex), $precision);
  return $size . ' ' . $units[$unitIndex];
}

/////////////////////////////////////////////////////////////////////////////////////
//if a user gets an email with an encryptedTenantId, they can create a user belonging to that tenant
function createUser($encryptedTenantId = NULL){
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
  	$encryptedPassword =  crypt($password, $encryptionPassword); //siteEncrypt($password); //we need one-way encryption for this, not siteEncrypt!
    $userList = userList();
    $tenantSql = "";
    $role = "normal";
    if (!$encryptedTenantId){
      $role = "admin"; //admin can alter the tenant and run reports.  super can do ANYTHING. normal can only do the basics
    }
    $sql = "INSERT INTO user(email, password, role, created) VALUES ('" . $email . "','" .  mysqli_real_escape_string($conn, $encryptedPassword) . "','" . $role . "','" . $formatedDateTime . "')"; 
    if(count(userList()) == 0) {
      //if there are no users, create the first one as admin. we also need a Tenant and we need to add the user to that Tenant
      $sql = "INSERT INTO user(email, password, created, role) VALUES ('" . $email . "','" .  mysqli_real_escape_string($conn, $encryptedPassword) . "','" .$formatedDateTime . "','super')"; 
      $tenantSql = "INSERT INTO tenant(name, created) VALUES  ('First Tenant', '" . $formatedDateTime . "')";
    } else if ($encryptedTenantId){
      $tenantId = siteDecrypt($encryptedTenantId);
    } else {
      //also make a tenant for this user if there's just a new user
      $tenantName = "New";
      if(strpos($email, "@") > 0){
        $tenantName = explode("@", $email)[1];
      }
      $tenantSql = "INSERT INTO tenant(name, created) VALUES  ('" . mysqli_real_escape_string($conn, $tenantName) . " Tenant', '" . $formatedDateTime . "')";
    }
    //die($sql);
	  //echo $sql;
    //die();
    $result = mysqli_query($conn, $sql);
    $userId = mysqli_insert_id($conn);
    if($tenantSql || $encryptedTenantId){
      if($tenantSql ){
        $result = mysqli_query($conn, $tenantSql);
        $tenantId = mysqli_insert_id($conn);
      }
      $tenantSql = "INSERT INTO tenant_user(user_id, tenant_id, created) VALUES  (" . $userId . "," . $tenantId  . ",'" . $formatedDateTime . "')";
      $result = mysqli_query($conn, $tenantSql);
    }
    //updateTablesFromTemplate($id);
    //die("*" . $id);
    loginUser($_POST);
    header("Location: ?");
  } else {
  	return $errors;
  
  }
  return false;
}

function updatePasswordOnUserWithToken($email, $userPassword, $token){
  global $conn;
  global $encryptionPassword;
  $encryptedPassword = crypt($userPassword, $encryptionPassword);
  $sql = "UPDATE user SET password = '" . mysqli_real_escape_string($conn, $encryptedPassword) . "', reset_password_token = NULL WHERE reset_password_token='" . mysqli_real_escape_string($conn, $token) . "' AND email = '" . mysqli_real_escape_string($conn, $email) ."'";
  mysqli_query($conn, $sql);
}

function userList(){
  Global $conn;
  $userSql = "SELECT * FROM user";
  $thisDataResult = mysqli_query($conn, $userSql);
  if($thisDataResult) {
    $rows = mysqli_fetch_all($thisDataResult, MYSQLI_ASSOC);
  } else {
    $rows = [];
  }
  return $rows;
}

function download($path, $friendlyName, $content = ""){
    header("Cache-Control: no-cache private");
    header("Content-Description: File Transfer");
    header('Content-disposition: attachment; filename='.$friendlyName);
    header("Content-Type: application/whatevs");
    header("Content-Transfer-Encoding: binary");
    if($path){
      $file = file_get_contents($path);
      header('Content-Length: '. strlen($file));
      echo $file;
    } else {
      header('Content-Length: '. strlen($content));
      echo $content;
    }
 
    exit;
 
}

function genericSelect($id, $name, $defaultValue, $data, $event = "", $handler= "") {
	$out = "";
	$out .= "<select name='" . $name. "' id='" . $id . "' " . $event . "=\"" . $handler . "\">\n";
  $out .= "<option/>";
	foreach($data as $datum) {
		$value = gvfa("value", $datum);
		$text = gvfa("text", $datum);
    if($value == ""  && $text == ""){ //if it's just a list of items, then each is both $value and $text
      $value = $datum;
      $text = $datum;
    }
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
 
function timeDifferenceInMinutes($datetimeOld, $datetimeNew) {
  $dateOld = new DateTime($datetimeOld);
  $dateNew= new DateTime($datetimeNew);
  $interval = $dateOld->diff($dateNew);
  $minutes = ($interval->days * 24 * 60) + ($interval->h * 60) + $interval->i;
  return $minutes;
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
  //var_dump($_POST);
  if($_dataRaw){
    $_data = json_decode($_dataRaw, true);
    if($tableName != "user") {
      $_data[] = array("name"=>"tenant_id", "type"=>"hidden");
    }
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
    //echo $column ."<BR>";
    $type =  strtolower(gvfa("type", $datum, ""));
    $value = gvfa($column, $data, "");
    if(!beginsWith($column, "_")  && !array_key_exists($column, $primaryKey)) {
      //echo  $column . "=" . $value . ", " . $type . "<BR>";
      $skip = false;
      if($type == "many-to-many") {
        if ($isInsert) {
          $skip  = true;
        }
      } else if($type == "time" && $value == "") {
        $skip  = true;

      } else if($column == "last_known_device_modified" || $column == "created" || $column == "modified"  || $type=="datetime") {
        if($type == "datetime" && $value=="" && $column != "created" && $column != "modified" ){
          $sanitized = 'NULL';
        } else {
          $sanitized = "'" . $formatedDateTime . "'";
        }
      } else if ($column == "expired"){
        $skip = true;
      } else if(($type == "bool"  || $type == "checkbox") && !$value){
        $sanitized = '0';
      } else if ((beginsWith($type, "number")  ||  $type == "int") && $value === "") {
        $sanitized = 'NULL';
      } else if (beginsWith($type, "string") && $value === "") {
        //echo $type . "=" . $column . "<BR>";
        $sanitized = 'NULL';
      } else {
        $sanitized = "'" . mysqli_real_escape_string($conn, $value) . "'";
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
        if($value == ''  && $column == "modified"){
          $value = $formatedDateTime;
        }
        //echo  $column . "=" . $value . ", " . $type . "<BR>";
        if($type == "many-to-many") {
          $mappingTable = gvfa("mapping_table", $datum);
          $countingColumn = gvfa("counting_column", $datum);
          $laterSql .= "\nDELETE FROM " . $mappingTable . " WHERE <whereclause/> ";
          if($tableName  != "user" &&  $tableName  != "tenant"){
            $laterSql .= "AND tenant_id='".  $data["tenant_id"] .  "'"; 
          }
          $laterSql .= ";";
          $count = 1;
          $extraM2MColumns = "";
          $extraM2MValues = "";
          //var_dump($value);
          if($value){;
            foreach($value as $valueItem){
              if($countingColumn) {
                $extraM2MColumns = ", " .$countingColumn;
                $extraM2MValues = ", " . $count;
              }
              $insertSql = "\n INSERT INTO " . $mappingTable . "(<tenant_id/> " . implode(",", array_keys($primaryKey)) . "," . $column . ",created" . $extraM2MColumns  . ") VALUES(<tenant_id_value/> '" . implode("','", array_values($primaryKey)) . "','" . $valueItem . "','" .  $formatedDateTime . "'" . $extraM2MValues . ");\n";
              if($tableName  != "user" && $tableName  != "tenant"){
                $insertSql = str_replace("<tenant_id/>", "tenant_id,", $insertSql);
                $insertSql = str_replace("<tenant_id_value/>", $data["tenant_id"] . ",", $insertSql);
              } else {
                $insertSql = str_replace("<tenant_id/>", "", $insertSql);
                $insertSql = str_replace("<tenant_id_value/>", "", $insertSql);
              }
              $laterSql .= $insertSql;
              $count++;
            }
          }
        } else if($column == "expired"  && $value == ""){
        } else if($type == "time" && $value == "") {
        } else if ($column != "created" && !beginsWith($column, "_")  && array_key_exists($column, $primaryKey) == false) {
          //echo $column .  " " . $value . "<BR>";
          if($type == "datetime" && $value=="" && $column != "created" && $column != "modified") {
            $sanitized = "NULL";
          } else if(($type == "bool"  || $type == "checkbox") && !$value){
            $sanitized = '0';
          } else if ((beginsWith($type, "number") || beginsWith($type, "int")) &&  $value === "") {
            $sanitized = 'NULL';
          } else {
            //echo $column .  "*" . $value . "<BR>";
            $sanitized = "'" . mysqli_real_escape_string($conn, $value) . "'";
          }

          $updateFields[] = "`$column` =  $sanitized";
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
      $values = implode(", ", $sanitizedData);
      
      $sql .= "INSERT INTO `$tableName` ($columns) VALUES ($values);";
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

function generateCurrentUrl() {
  // Determine the protocol
  $protocol = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off' || $_SERVER['SERVER_PORT'] == 443) ? "https://" : "http://";
  $host = $_SERVER['HTTP_HOST'];
  $scriptName = $_SERVER['SCRIPT_NAME'];
  $fullUrl = $protocol . $host . $scriptName;
  return $fullUrl;
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

function sanitizeForJson($input) {
  // Define a regex pattern to match illegal characters
  // This pattern matches any character outside the legal JSON character set
  // \x20-\x7E covers the visible ASCII characters excluding control characters
  // \u0800-\uFFFF covers additional Unicode characters
  $illegalCharsPattern = '/[^\x20-\x7E\x80-\xFF\u0100-\uFFFF]/u';

  // Replace illegal characters with spaces
  $sanitizedInput = preg_replace($illegalCharsPattern, ' ', $input);

  return $sanitizedInput;
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
  $tempFile = "gottaHaveSomething1";
  $tempDir = sys_get_temp_dir();
  if (is_writable($tempDir)) {
    $tempFile = @tempnam($tempDir, 'php');
  }
  if($tempFile == "") {
    $tempFile = "gottaHaveSomething2";
  }
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


function getAssociatedRecords($commandTypeId, $tenantId){
  Global $conn;
  //first figure out if we have an associated table: 
  $sql = "SELECT associated_table, value_column, name_column FROM command_type WHERE command_type_id=" . intval($commandTypeId) . " AND tenant_id=" . intval($tenantId);
  //echo $sql;
  $result = mysqli_query($conn, $sql);
  if($result) {
    $row = $result->fetch_assoc();
    if($row){
      $table = $row["associated_table"];
			$valueColumn = $row["value_column"];
      $nameColumn = $row["name_column"];
      if(!$nameColumn){
        $nameColumn = "name";
      }
			//my framework kinda depends on single-column pks having the name of the table with "_id" tacked on the end. if you're doing something different, you might have to store the name of your pk
			if($valueColumn && $table) {
				$sql = "SELECT ". $nameColumn . ", " . $table . "_id AS id," . $valueColumn . " FROM " . $table . " WHERE tenant_id=" . $tenantId . " ORDER BY name";
				//echo $sql;
				$subResult = mysqli_query($conn, $sql);
				if($subResult) {
					$rows = mysqli_fetch_all($subResult, MYSQLI_ASSOC);
          return $rows;
        }
      }
    }
  }
}

function deleteLink($table, $pkName, $extraData = "") {
  if($extraData  && !beginsWith($extraData, "&")){
    $extraData = "&" . $extraData;
  }
  $out = "<a onclick='return confirm(\"Are you sure you want to delete this " . $table . "?\")' href='?table=" . $table . "&action=delete&" . $pkName . "=<" . $pkName . "/>&hashed_entities=<hashed_entities/>" . $extraData ."'>Delete</a>";
  return $out;
}

//this permissions model applies to reports with roles, but it could apply to other things
//like device management and the automation editor, among other things
function canUserDoThing($user, $thingRole){
  $userRole = $user["role"];
  if($userRole == "super"){
    return true;
  }
  if($userRole == $thingRole || !$thingRole){
    return true;
  }
  if($thingRole != "super"  && $userRole == "admin"){
    return true;
  }
  if(($thingRole != "super" && $thingRole != "admin")  && $userRole == "subadmin"){
    return true;
  }
  return false;
}

function previousReportRuns($user, $reportId) {
  Global $conn;
  $sql = "SELECT report_log_id, run, records_returned, runtime, SUBSTRING(`sql`, 1, 40) as `sql`  FROM report_log WHERE report_id=" . intval($reportId) . " AND tenant_id=" . intval($user["tenant_id"]) . " AND user_id=" . intval($user["user_id"]) . " ORDER BY run DESC";
  $result = mysqli_query($conn, $sql);
  $out = "";
  if($result) {
    $reportRuns = mysqli_fetch_all($result, MYSQLI_ASSOC);
    $toolsTemplate = "<a href='?action=rerun&table=report&report_log_id=<report_log_id/>'>Re-Run</a> ";
    $out = genericTable($reportRuns, null, $toolsTemplate, null);
  }
  return $out;
}

function doReport($user, $reportId, $reportLogId = null, $outputFormat = ""){
  Global $conn;
  $tenantId = $user["tenant_id"];
  $historicDataObject = null;

  if($reportLogId != null) {
    $sql = "SELECT data, report_id FROM report_log WHERE report_log_id = " . intval($reportLogId) . " AND tenant_id=" . intval($tenantId) . " AND user_id=" . intval($user["user_id"]);
   
    $historyResult = mysqli_query($conn, $sql);
    if($historyResult) {
      $historyData = mysqli_fetch_array($historyResult);
      if($historyData){
        $data = $historyData["data"];
        $reportId = $historyData["report_id"];
        //var_dump($data);
        //trouble with carriage feeds:
 
        $data = sanitizeForJson($data);
 
        $historicDataObject = json_decode($data, true);
 
      }
    }
  }
  //$displayDropdownConfig = json_decode('[{"text":"web","value":"web"},{"text":"CSV","value":"CSV"}]');
  $editButton = "<a href='?table=report&report_id=" . $reportId . "' class='basicbutton'>edit</a>";
  $reRunButton = "<a href='?action=fetch&table=report&report_id=" . $reportId . "' class='basicbutton'>run</a>";
  $displayButtons = "<a class='basicbutton' href='?table=report&report_log_id=" . $reportLogId . "&report_id=" . $reportId . "&action=fetch&output_format=csv'>download CSV</a>";
  //$displayDropdown = genericSelect("output_format", "output_format", $outputFormat, $displayDropdownConfig);
  $sql = "SELECT * FROM report WHERE report_id=" . intval($reportId) . " AND tenant_id=" . intval($tenantId);
  //die($sql);
  $result = mysqli_query($conn, $sql);
  $unfoundName = "unnamed";
  $data = "";
  $out = "";
  $errors = [];
  $ran = false;
  $date = new DateTime("now", new DateTimeZone('America/New_York'));//obviously, you would use your timezone, not necessarily mine

  $formatedDateTime =  $date->format('Y-m-d H:i:s');

  if($result) {
    
    $reportData = mysqli_fetch_array($result);
    if(!canUserDoThing($user, $reportData["role"])) {
      return "You lack permissions to run this report.";
    }
    $sql = $reportData["sql"];
    $form = $reportData["form"];

    $decodedForm = "";
    $decodedFormToUse = "";
    $output = null;
    $outputs = [];
    if($form){
      $decodedForm = json_decode($form, true);
      if(!$decodedForm && $form!=""){
        $errors[] ="There was malformed JSON in the Form.</div>";

      } else if ($decodedForm){
        if(array_key_exists("output", $decodedForm)) {
          
          $output = $decodedForm["output"];
          if(is_array($output)){
            if(array_is_list($output)){
              $outputCount = 1;
              $outputs = [];
              foreach($output as $specificOutput){
                $outputName = gvfa("name", $specificOutput, $unfoundName . " " . $outputCount);
                $outputCount++;
                $outputs[] = $outputName;
              }
            } else {
              $outputName = gvfa("name", $output, "graph");
              $outputs = [$outputName];
            }
          }
          //$decodedFormToUse = "";
          $decodedFormToUse = [];
        } else {
          $decodedFormToUse = $decodedForm;
        }
        if(array_key_exists("form", $decodedForm)){
          $decodedFormToUse = $decodedForm["form"];
        }
        if(array_key_exists("outputDefault", $decodedForm)){
          if($outputFormat == ""){
            $outputFormat = $decodedForm["outputDefault"];
          }
        }
      }
    } else {
      $decodedFormToUse = [];
    }
    if($outputFormat == ""){
      $outputFormat = "html";
    }
    if(count($errors) == 0 && $decodedFormToUse != "" && gvfw("action") == "fetch" || gvfw("action") == "rerun") {
      $out .= "<div class='listtitle'>Prepare to Run Report  '" . $reportData["name"] . "' " . $editButton . "</div>";
 
      if($historicDataObject  && $decodedFormToUse){

        $decodedFormToUse = copyValuesFromSourceToDest($decodedFormToUse, $historicDataObject);
      }
 
      $outputDropdownValues = ['web', 'CSV'];
      if($outputs){
        $outputDropdownValues = array_merge($outputDropdownValues, $outputs);
      }

      $decodedFormToUse[] =      [
        'label' => 'output format',
        'name' => 'output_format',
        'value' => $outputFormat,
        'type' => 'select',
        'values' => $outputDropdownValues
      ];


      $out .= genericForm($decodedFormToUse, "Run", "Running Report...", $user);
      $out .= "<div class='listtitle'>Past Runs:</div>";
      $out .= previousReportRuns($user, $reportId);
    } else if (count($errors) > 0) {
      $out .= "<div class='genericformerror'>";
      foreach($errors as $error){
        $out .= $error . "<br/>";
      }
      $out .= "</div>";
    } else {
      $ran = true;
      $out = "<div class='listtitle'>Running Report  '" . $reportData["name"] . "' " . $editButton . " " . $reRunButton  . " " . "</div>";
      //gotta merge form values in here:
      $sql =  tokenReplace($sql, $_POST);
      //die($sql);
      //delete from report_log where report_log_id=149;
      //echo $sql;
      $start = microtime(true);
      $reportResult = mysqli_query($conn, $sql);
      $error = mysqli_error($conn);
      $affectedRows = mysqli_affected_rows($conn);
      $count = 0;
      $rows = false;
      //echo $reportResult . "-" . $error  . "=" . $affectedRows;
      if($reportResult || $error || $affectedRows) {
        if($decodedFormToUse != "") {
          $decodedFormToUse = mergeValues($decodedFormToUse, $_POST);
        }
        if($error) {
          $rows = [["error" => $error]];
          $count = 0;
        } else if($affectedRows &&  $reportResult===true) {
          $rows = [["Affected records" => $affectedRows]];
          $count = count($rows);
        } else if($reportResult !== false  && $reportResult !== true){
          $rows = mysqli_fetch_all($reportResult, MYSQLI_ASSOC);
          $count = count($rows);
        }
        $timeElapsedSecs = microtime(true) - $start;
        $reportLogSql = "INSERT INTO report_log (tenant_id, user_id, report_id, run, records_returned, runtime, `data`, `sql`) VALUES (" . intval($tenantId) . "," . intval($user["user_id"]) . "," . intval($reportId) . ",'" . $formatedDateTime . "'," . $count  . "," .  intval($timeElapsedSecs * 1000) . ",'" . mysqli_real_escape_string($conn, json_encode($decodedFormToUse)) . "','" . mysqli_real_escape_string($conn, $sql) . "');";
        //echo $reportLogSql;
        $reportLogResult = mysqli_query($conn, $reportLogSql);
        //var_dump($rows);
        if($rows) {
          if(strtolower($outputFormat) == "csv") {
            $content = generateCsvContent($rows);
            download("", str_replace(" ", "_", $reportData["name"]) . ".csv", $content);
            $url = "?table=report&report_id=" . $reportId . "&report_log_id=" . $reportLogId . "&action=fetch";
            header("Location: " . $url);
          } else {
            //$tableTools 
            //function genericTable($rows, $headerData = NULL, $toolsTemplate = NULL, $searchData = null, $tableName = "", $primaryKeyName = "", $autoRefreshSql = null, $tableTools) { //aka genericList
            //echo "'" . $outputFormat . "'<BR>";
            $outputIfThereIsOne = getOutputIfThereIsOne($outputFormat, $output, $unfoundName);
            //var_dump($outputIfThereIsOne);
            if($outputIfThereIsOne == null) {
              $data .= genericTable($rows, null, null, null, "", "", null, "");
            } else {
              $outputIfThereIsOne =  tokenReplace($outputIfThereIsOne, $_POST); //there could be tokens in the output config
              $canvasId = "statsCanvas";
              $data .= "\n<script src = \"./tinycolor.js\"></script>\n";
              $data .= "\n<script src = \"https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.7.3/Chart.min.js\"></script>\n";
              $data .= "\n<script>let reportData = " . json_encode($rows) . ";\nlet reportOutput = " . json_encode($outputIfThereIsOne) . "\n;document.addEventListener('DOMContentLoaded', async () => {displayReport(" . $reportId . ",'" . $canvasId . "');});\n</script>\n";
              $data .= "\n<canvas id=\"" . $canvasId . "\" style='display:block;'></canvas>\n";
              $data .= "\n<div id='visualizationCaption' style='padding:10px'></div>";
            }
          }
        }
      }
    }
  }
  if($data == ""  && $ran){
    $data = "No results";
  }
  $out .= $data;
  return $out;
}

function timeAgo($sqlDateTime, $compareTo = null) {
  global $timezone;
  // Set the timezone
  $timezone = $timezone ?? 'UTC';
  $timezoneObject = new DateTimeZone($timezone);

  // Convert input dates to DateTime objects with the specified timezone
  $past = new DateTime($sqlDateTime, $timezoneObject);
  $compareTo = $compareTo ? new DateTime($compareTo, $timezoneObject) : new DateTime('now', $timezoneObject);

  // Calculate the time difference in seconds
  $diffInSeconds = max(0, $compareTo->getTimestamp() - $past->getTimestamp());
  
  $seconds = $diffInSeconds % 60;
  $minutes = floor($diffInSeconds / 60) % 60;
  $hours = floor($diffInSeconds / 3600) % 24;
  $days = floor($diffInSeconds / 86400);

  if ($days > 0) {
      return $days === 1 ? '1 day ago' : "$days days ago";
  }
  if ($hours > 0) {
      return $hours === 1 ? '1 hour ago' : "$hours hours ago";
  }
  if ($minutes > 0) {
      return $minutes === 1 ? '1 minute ago' : "$minutes minutes ago";
  }
  return $seconds === 1 ? '1 second ago' : "$seconds seconds ago";
}


function getOutputIfThereIsOne($outputFormat, $output, $unfoundName = "custom") {
  $outputCount = 1;
  if(is_array($output)){
    if(array_is_list($output)){
      foreach($output as $specificOutput){
        $outputName = gvfa("name", $specificOutput, $unfoundName . " " . $outputCount);
        $outputCount++;
        if($outputFormat == $outputName){
          return $specificOutput;
        }
      }
    } else {
      $outputName = gvfa("name", $output, $unfoundName);
      if($outputFormat == $outputName){
        return $output;
      }
    }
  }
  return null;
}

if (!function_exists('array_is_list')) {
  function array_is_list(array $arr)
  {
      if ($arr === []) {
          return true;
      }
      return array_keys($arr) === range(0, count($arr) - 1);
  }
}

function replaceCharacterWithinQuotes($str, $char, $repl) {
  if ( strpos( $str, $char ) === false ) return $str ;

  $placeholder = chr(7) ;
  $inSingleQuote = false ;
  $inDoubleQuotes = false ;
  $inBackQuotes = false ;
  for ($p = 0 ; $p < strlen($str) ; $p++ ) {
      switch ( $str[$p] ) {
          case "'": if ( ! $inDoubleQuotes && ! $inBackquotes ) $inSingleQuote = ! $inSingleQuote ; break ;
          case '"': if ( ! $inSingleQuote && ! $inBackquotes ) $inDoubleQuotes = ! $inDoubleQuotes ; break ;
          case '`': if ( ! $inSingleQuote && ! $inDoubleQuotes ) $inBackquotes  = ! $inBackquotes ; break ;
          case '\\': $p++ ; break ;
          case $char: if ( $inSingleQuote || $inDoubleQuotes || $inBackQuotes) $str[$p] = $placeholder ; break ;
      }
  }
  return str_replace($placeholder, $repl, $str) ;
}

function getJsonErrorMessage($errorCode) {
  switch ($errorCode) {
      case JSON_ERROR_NONE:
          return;
      case JSON_ERROR_DEPTH:
          return 'Maximum stack depth exceeded';
      case JSON_ERROR_STATE_MISMATCH:
          return 'Underflow or the modes mismatch';
      case JSON_ERROR_CTRL_CHAR:
          return 'Unexpected control character found';
      case JSON_ERROR_SYNTAX:
          return 'Syntax error, malformed JSON';
      case JSON_ERROR_UTF8:
          return 'Malformed UTF-8 characters, possibly incorrectly encoded';
      case JSON_ERROR_RECURSION:
          return 'One or more recursive references in the value to be encoded';
      case JSON_ERROR_INF_OR_NAN:
          return 'One or more NAN or INF values in the value to be encoded';
      case JSON_ERROR_UNSUPPORTED_TYPE:
          return 'A value of a type that cannot be encoded was given';
      case JSON_ERROR_INVALID_PROPERTY_NAME:
          return 'A property name that cannot be encoded was given';
      case JSON_ERROR_UTF16:
          return 'Malformed UTF-16 characters, possibly incorrectly encoded';
      default:
          return 'Unknown error';
  }
}

function checkMySqlSyntax($query) {
  global $conn;
  $errors = [];

  if (trim($query)) {
      // Replace characters within string literals that may mess up the process
      $query = replaceCharacterWithinQuotes($query, '#', '%');
      $query = replaceCharacterWithinQuotes($query, ';', ':');

      // Prepare the query to make a valid EXPLAIN query
      // Remove comments # comment ; or # comment newline
      // Remove SET @var=val;
      // Remove empty statements
      // Remove last ;
      // Put EXPLAIN in front of every MySQL statement (separated by ;)
      $query = "EXPLAIN " .
          preg_replace(
              [
                  "/#[^\n\r;]*([\n\r;]|$)/",
                  "/[Ss][Ee][Tt]\s+\@[A-Za-z0-9_]+\s*:?=\s*[^;]+(;|$)/",
                  "/;\s*;/",
                  "/;\s*$/",
                  "/;/"
              ],
              [
                  "",
                  "",
                  ";",
                  "",
                  "; EXPLAIN "
              ],
              $query
          );

      foreach (explode(';', $query) as $q) {
          if (trim($q)) {
              try {
                  $result = $conn->query($q);
                  if (!$result) {
                    $error =  mysqli_error($conn);
                    //echo "one" . $error;
                    $errors[] = $error;
                  }
              } catch (Exception $e) {
                echo "two";
                  $errors[] = $e->getMessage();
              }
          }
      }
  }
  $errors = array_filter($errors);
  //var_dump($errors);
  //$errors = array_filter($errors);
  return ["errors"=>$errors];  
}

function checkJsonSyntax($json) {
  // Decode the JSON string
  json_decode($json);
  // Check for JSON parsing errors
  $lastError = json_last_error();
  if($lastError == 0) {
    $error = "";
  } else {
    $error = getJsonErrorMessage(json_last_error());
  }
  return ["errors"=>$error];  
}


function readMemoryCache($key, $persistTimeInMinutes = 10) {
  $time = apcu_fetch($key . "_time");
  $value = apcu_fetch($key . "_value");
  if(round(abs(time() - $time) / 60,2) <= $persistTimeInMinutes) {
    return $value;
  } else {
    return null;
  }
}

function writeMemoryCache($key, $value) {
  apcu_store($key . "_time", time());
  apcu_store($key . "_value", $value);
}


function findRecordByKey($records, $keyName, $value) {
  foreach ($records as $record) {
      if ($record[$keyName] === $value) {
          return $record;
      }
  }
  return null;
}

function defaultFailDown($first, $second="", $third=""){
  if($first){
    return $first;
  }
  if($second){
    return $second;
  }
  if($third){
    return $third;
  }
}

function checkPhpFunctionCallIsBogus($str){
  $str = trim($str);
  $str = str_replace([" ", "\t", "\n", "\r", "\0", "\x0B"], "", $str);
  $pattern = '/\(\s*,|,\s*\)/';
  return preg_match($pattern, $str) === 1;
}



function removeDelimiters($str, $replacement = "_") {
  $delimiters = "|*!";
  // Replace all delimiters with an empty string
  $str = str_replace(str_split($delimiters), $replacement, $str);
  return $str;
}

 