<?php 
//server-based remote controller using microcontrollers 
//gus mueller, March 18 2024
//////////////////////////////////////////////////////////////
//troubleURLs:  
 
 

ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);

include("config.php");
 

//var_dump($_POST);
$words = array();
$wordlists = array();

$conn = mysqli_connect($servername, $username, $password, $database);

$table = strtolower(gvfw('table', "document"));
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
 
 
$scanId = gvfw('scan_id');
$testId = gvfw('test_id');
$wordListId = gvfw('word_list_id');
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
  if($action == "genericformsave") { //Definitely fix the security here!!!!
    //this only works for checkboxes for now
    $tableName = gvfw('table_name');
    $name = gvfw('name');
    $value = gvfw('value');
    $primaryKeyName = gvfw('primary_key_name');
    $primaryKeyValue = gvfw('primary_key_value');
    if($value == "false"){
      $value = 0;
    } else if($value == "true"){
      $value = 1;
    }
    //a little safer only because it only allows a user to fuck up records connected to their userId
    $sql = "UPDATE ". filterStringForSqlEntities($tableName) . " SET " . filterStringForSqlEntities($name) . "='" .  mysqli_real_escape_string($conn, $value) . "' WHERE user_id=" . intval($userId) . " AND " . filterStringForSqlEntities($primaryKeyName) . "='" . intval($primaryKeyValue) . "'";
    
    $result = mysqli_query($conn, $sql);
    var_dump($result);
    die($sql);
  
  }
  if ($table == "utilities") {
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
 
 


	} else if($table == "devices") {
 

    $out .= devices($userId);

      
 
 
  
  } else if ($action == "startcreate") {
  
    if ($table == "test") {

      
      $out .= wordForm($errors, NULL, $userId, $wordListId);
    } else if ($table == "user" || !is_null($errors)) {
      $out .= newUserForm($errors);
    } else {

      $out .= genericEntityForm($userId, $table, $errors);
    }
 
 
      

 
  } else {
    if(gvfw($table . '_id')) {
      $out .= genericEntityForm($userId, $table, $errors);

    } else {
      $out .= genericEntityList($userId, $table);
    }
    
  }
	$out .= "</div>\n";
	$out .= "<div>\n";
	$out .= " "; 
	$out .= "</div>\n";

} else {
  if(gvfa("password", $_POST) != "") {
    $out .= "<div class='genericformerror'>The credentials you entered have failed.</div>";
   }
  $out .= loginForm();
}


echo bodyWrap($out, $user, $deviceId, $poser);

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
  $out = "";
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
  $out .= "<div class='waitingouter' id='waitingouter'><div class='waiting' id='waiting'><img width='200' height='200' src='./images/signs.gif'></div><div id='waitingmessage' class='waitingmessage'>Waiting...</div></div>\n";
  $out .= "<div class='outercontent'>\n";
  $out.= "<div class='logo'>" . $siteName . "</logo></div>\n";
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
  $out .= "<div class='documentdescription'>";
  /*
  if($documentId){
    $document = getDocumentFromDocumentId($documentId);
    $out .= "Current xxxx: <em>" . $document["name"] . "</em> (" . $document["file_name"] . ")";
  } else {
    $out .= "No xxxx selected\n";

  }
  */
  $out .= "</div>";
	$out .= tabNav();
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
  $out .= "<div> or  <div class='basicbutton'><a href=\"?table=user&action=startcreate\">Create Account</a></div>\n";
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

function xxTestForm($error = NULL, $testId = "", $userId = "") {
  if($testId  != "") {
    Global $conn;
    $sql = "SELECT * FROM `test` WHERE user_id = " . intval($userId) . " AND test_id = " . intval($testId);
    //echo $sql;
    $result = mysqli_query($conn, $sql);
    $row = $result->fetch_assoc();
    $source = $row;
  } else {
    $source = $_POST;
  }
  $submitLabel = "save test";
  if($testId  == "") {
    $submitLabel = "create test";
  }
	$formData = array(
		[
	    'label' => 'test name',
      'name' => 'name',
	    'value' => gvfa("name", $source), 
      'error' => gvfa('name', $error)
	  ],
    [
	    'label' => 'enabled',
      'name' => 'enabled',
      'type' => 'bool',
	    'value' => gvfa("enabled", $source), 
      'error' => gvfa('name', $error)
	  ],
		[
	    'label' => 'test type',
      'name' => 'test_type',
      'type' => 'select',
      'values' => [
        'chatGpt',
        'blacklist',
        'regex indices',
        'whitelist'
      ],
	    'value' => gvfa("test_type", $source), 
      'error' => gvfa('auttest_typehor', $error)
	  ],


    [
	    'label' => 'word list',
      'name' => 'word_list_id',
      'type' => 'select',
	    'value' => gvfa("word_list_id", $source), 
      'error' => gvfa('word_list_id', $error),
      'values' => "SELECT word_list_id, name as 'text' FROM word_list WHERE user_id='" . $userId  . "' ORDER BY name ASC",
	  ],

	  [
	    'label' => 'test content',
      'width' => 400,
      'height' => 100,
      'name' => 'test_content',
      'value' => gvfa("test_content", $source), 
      'error' => gvfa('test_content', $error)
	  ],
    [
	    'label' => 'table name',
      'name' => 'table_name',
      'type' => 'hidden',
      'value' => gvfa("table_name", $source), 
      'error' => gvfa('table_name', $error)
	  ],
    [
	    'label' => 'type value',
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
	    'label' => 'color',
      'name' => 'markup_color',
      'type' => 'color',
      'value' => gvfa("markup_color", $source), 
      'error' => gvfa('markup_color', $error)
	  ],
    [
	    'label' => 'size',
      'name' => 'markup_size',
      'value' => gvfa("markup_size", $source), 
      'error' => gvfa('markup_size', $error)
	  ],
    [
	    'label' => 'weight',
      'name' => 'markup_weight',
      'value' => gvfa("markup_weight", $source), 
      'error' => gvfa('markup_weight', $error)
	  ],
    [
	    'label' => 'more config',
      'name' => 'config',
      'type' => 'json',
      'value' => gvfa("config", $source), 
      'template' => '{"excludeNumbers": 0, "caseSensitive":0, "searchForPhrasesAndRoots":0, "requireLeadingSpaces":0, "filterOutResultsContainingCarriageReturns": 0}',
      'error' => gvfa('config', $error)
	  ],
	  [
	    'label' => '',
      'name' => 'test_id',
      'value' => gvfa("test_id", $source),
      'type' => 'hidden',
      'error' => gvfa('test_id', $error)
	  ]
	);
  //var_dump($formData);
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

function utilities($user, $viewMode = "list") {
  $utilitiesData = array(

 
 
    [
      'label' => 'Impersonate Another User',
      'description' => "Act as a different user.",
      'key' => 'impersonate',
      'role' => 'admin',
      'form' => 
          [
            [
              'label' => 'User',
              'name' => 'user_id',
              'value' => gvfa("user_id", $_POST),
              'type' => 'select',
              'values' => "SELECT user_id, email as 'text' FROM user ORDER BY email ASC"
            ]
          ],
        'action' => 'impersonateUser(<user_id/>)'
    ],
    [
      'label' => 'Reset User App Data',
      'description' => "Reset user data back to factory.",
      'key' => 'userreset',
      'role' => 'admin',
      'form' => 
          [
            [
              'label' => 'User',
              'name' => 'user_id',
              'value' => gvfa("user_id", $_POST),
              'type' => 'select',
              'values' => "SELECT user_id, email as 'text' FROM user ORDER BY email ASC"
            ]
          ],
        'action' => 'updateTablesFromTemplate(<user_id/>)'
    ],
    [
      'label' => 'Reset Factory Data',
      'description' => "Set factory data to that of a specific user.",
      'key' => 'factoryreset',
      'role' => 'admin',
      'form' => 
          [
            [
              'label' => 'User',
              'name' => 'user_id',
              'value' => gvfa("user_id", $_POST),
              'type' => 'select',
              'values' => "SELECT user_id, email as 'text' FROM user ORDER BY email ASC"
            ]
          ],
        'action' => 'updateTemplateTablesFromAUser(<user_id/>)'
    ],
    [
      'label' => 'Fix Word List Ids',
      'description' => "They might be wrong from the factory.",
      'key' => 'fixwordlistids',
      'role' => 'admin',
      'form' => 
          [
            [
              'label' => 'User',
              'name' => 'user_id',
              'value' => gvfa("user_id", $_POST),
              'type' => 'select',
              'values' => "SELECT user_id, email as 'text' FROM user ORDER BY email ASC"
            ]
          ],
        'action' => 'fixWordListIds(<user_id/>)'
    ]

  );


  $filteredData = array_filter($utilitiesData, function ($subData) use ($user) {
    return doesUserHaveRole($user, gvfa("role", $subData));
  });

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

  $thisDataSql = "SELECT * FROM " . $table . " WHERE user_id=" . intval($userId);
  $deviceId = gvfw("device_id");
  if($deviceId){
    $thisDataSql .= " AND device_id=" . intval($deviceId);
  }
  $thisDataResult = mysqli_query($conn, $thisDataSql);
  if($thisDataResult) {
    $thisDataRows = mysqli_fetch_all($thisDataResult, MYSQLI_ASSOC); 
    $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a>";
    if($table == "device") {
      $toolsTemplate .= " | <a href='?table=device_feature&device_id=<" . $table . "_id/>'>Device Features</a>";

    }
    return genericTable($thisDataRows, $headerData, $toolsTemplate, null, $table, $pk);
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

    $data = updateDataWithRows($data, $thisDataRows[0]);

    return genericForm($data, "Save " . $table, "Saving...");
  }
}

 
function genericEntitySave($userId, $table) {
  Global $conn;
  $data = schemaArrayFromSchema($table, $pk);
  $data = $_POST;
  unset($data['action']);
  unset($data[$pk]);
  unset($data['created']);
  $sql = insertUpdateSql($conn, $table, array($pk => $_GET[$table . '_id']), $data);
  //echo $sql;
  //die();
  $result = mysqli_query($conn, $sql);
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
	$out .= "<div class='genericform'>\n";
	foreach($data as &$datum) {
		$label = gvfa("label", $datum);
		$value = str_replace("\\\\", "\\", gvfa("value", $datum)); 
		$name = gvfa("name", $datum); 
		$type = strtolower(gvfa("type", $datum)); 
    $width = 200;
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

        $out .= "<select name='" . $name . "' />";
        if(is_string($values)) {
          $out .= "<option value='0'>none</option>";
          //echo $values;
          $result = mysqli_query($conn, $values); //REALLY NEED TO SANITIZE $values since it contains RAW SQL!!!
          $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
          foreach($rows as $row){
            $selected = "";
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
      } else if ($type == "bool"){
        $checked = "";
          if($value) {

            $checked = "checked";
          }
        $out .= "<input value='1' name='" . $name . "'  " . $checked . " type='checkbox'/>\n";
      } else {
        if($height){
          $out .= "<textarea " .  $idString . " style='width:" . $width . "px;height:" . $height . "px' name='" . $name . "'  />" .  $value  . "</textarea>\n";
        } else {

          $out .= "<input style='width:" . $width . "px'  " . $idString. " name='" . $name . "' value=\"" .  $value . "\" type='" . $type . "'/>\n";
        }
        
      }
      
      $out .= "</div>\n";
    }
	}
	$out .= "<div class='genericformelementlabel'><input name='action' id='action' value='" .  $submitLabel. "' type='submit'/></div>\n";
  $out .= "<input  name='_data' value=\"" . htmlspecialchars(json_encode($data)) . "\" type='hidden'/>";
  
	$out .= "</div>\n";
	$out .= "</form>\n";

  $out = "<form onsubmit='startWaiting(\"" . $waitingMesasage . "\")' method='post' name='genericform' id='genericform' enctype='multipart/form-data'>\n" . $out;
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

function tabNav() {
	$tabData = array(
  [
    'label' => 'Locations',
    'table' => 'location' 
  ] ,
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
    'label' => 'Device Type Features',
    'table' => 'device_type_feature' 
  ] ,
  [
    'label' => 'Device Features',
    'table' => 'device_feature' 
  ] 
	);
	$out = "<div class='nav'>";
  $currentMode = gvfa('table', $_REQUEST);
  $deviceId = gvfa('device_id', $_REQUEST);
 
  foreach($tabData as &$tab) {

    if($currentMode == "") {
      $currentMode  = "document";
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
    
    $out .= "<div class='" . $class . "'><a href='" . $url . "'>" . $label . "</a></div>";
  }
  $out .= "</div>\n";
  return $out;
}



function devices($userId) {
  Global $conn;
  $table = "device";
  $sql = "SELECT * FROM `" . $table . "` WHERE user_id = '" . intval($userId) . "' ORDER BY created DESC ";
  //echo $sql;
  $result = mysqli_query($conn, $sql);
  $out = "";
  $out .= "<div class='listtitle'>Your " . ucfirst($table) . "s</div>\n";
  $out .= "<div class='listtools'><div class='basicbutton'><a href='?table=" . $table . "&mode=startcreate'>Create</a></div> a new " . $table . "<//div>\n";
  //$out .= "<hr style='width:100px;margin:0'/>\n";
  $headerData = array(
    [
      'label' => 'name',
      'name' => 'name' 
    ],
    [
      'label' => 'created',
      'name' => 'created' 
    ] 

    );
  $toolsTemplate = "<a href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>'>Edit Info</a> ";

  
  $toolsTemplate .= "<a onclick='return confirm(\"Are you sure you want to delete this " . $table . "?\")' href='?table=" . $table . "&" . $table . "_id=<" . $table . "_id/>&action=delete'>Delete</a>";
  if($result) {
	  $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
	  
	  if($rows) {
      $out .= genericTable($rows, $headerData, $toolsTemplate, null, $table, $table . "_id");
    }
  }
  return $out;
}


function xLists($userId) {
  Global $conn;
  $sql = "SELECT * FROM `word_list`  WHERE user_id = " . intval($userId) . " ORDER BY name ASC limit 0,22";
  //echo $sql;
  $result = mysqli_query($conn, $sql);
  $out = "";
  $out .= "<div class='listtitle'>Your Word Lists</div>\n";
  $out .= "<div class='listtools'><div class='basicbutton'><a href='?table=word_list&mode=startcreate'>Create</a></div> a new word list<//div>\n";
  //$out .= "<hr style='width:100px;margin:0'/>\n";
  $headerData = array(
    [
      'label' => 'name',
      'name' => 'name' 
    ]
    );

  $toolsTemplate = "<a href='?table=word_list&word_list_id=<word_list_id/>'>Edit Info</a> | <a href='?table=word&word_list_id=<word_list_id/>'>see words</a>";
  if($result) {
	  $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
	  
	  if($rows) {
      $out .= genericTable($rows, $headerData, $toolsTemplate, $searchData, null, "word_list", "word_list_id");
    }
  }
  return $out;
}

 
 

function genericTable($rows, $headerData = NULL, $toolsTemplate = NULL, $searchData = null, $tableName = "", $primaryKeyName = "") { //aka genericList
  if($headerData == NULL  && $rows  && $rows[0]) {
    foreach(array_keys(rows[0]) as &$key) {
      array_push($headerData, array("label"=>$key, "name"=>$key));
    }
  
  }
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
      
        $function =  tokenReplace($function, $row) . ";"; 
        //echo $function . "<P>";
        try{
          eval('$value = ' . $function);
        }
        catch(Exception  $err){
          //echo $err;

        }
        ;
        
      }
      if (gvfa("liveChangeable", $headerItem)) {
        if($row[$name] == 1){
          $checkedString = " checked ";
          
        }

        if($type == "color"  || $type == "text"  || $type == "number" || $type == "string"){
          $out .= "<input onchange='genericListActionBackend(\"" . $name . "\",  this.value ,\"" . $tableName  . "\",\"" . $primaryKeyName  . "\",\"" . $row[$primaryKeyName]  . "\")' value='" . $value . "'  name='" . $name . "' type='" . $type . "' />\n";
        } else if($type == "checkbox" || $type == "bool") {

          $out .= "<input onchange='genericListActionBackend(\"" . $name . "\",this.checked,\"" . $tableName  . "\",\"" . $primaryKeyName  . "\",\"" . $row[$primaryKeyName]  . "\")' name='" . $name . "' type='checkbox' value='1' " . $checkedString . "/>\n";
        }
      } else {
        if($template != "") {
          $out .=  "<a href=\"" . tokenReplace($template, $row) . "\">" . htmlspecialchars($value) . "</a>";
        } else {
          $out .=  htmlspecialchars($value);
        }
        
      }
      
      $out .= "</span>\n";
    }
    if($toolsTemplate) {
      
      $out .= "<span>" . tokenReplace($toolsTemplate,  $row) . "</span>\n";
    }
    $out .= "</div>\n";
  }
  //$out .= "</div>\n";
  $out .= "</div>\n";
  return $out;
}


function tokenReplace($template, $data, $strDelimiterBegin = "<", $strDelimiterEnd = "/>"){
  foreach($data as $key => $value) {
    $template = str_replace($strDelimiterBegin . $key . $strDelimiterEnd, $value, $template);
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
  	$sql = "INSERT INTO user(email, password, created) VALUES ('" . $email . "','" .  mysqli_real_escape_string($conn, $encryptedPassword) . "','" .$formatedDateTime . "')"; 
	  //echo $sql;
    //die();
    $result = mysqli_query($conn, $sql);
    $id = mysqli_insert_id($conn);
    updateTablesFromTemplate($id);
    //die("*" . $id);
    loginUser($_POST);
    header("Location: ?");
  } else {
  	return $errors;
  
  }
  return false;
 
}


function saveX($userId){
  Global $conn;
  $sql = "SELECT * from word_list WHERE user_id=" . intval($userId) . " AND name='" . mysqli_real_escape_string($conn, $_POST["name"])   . "'";

 
  $result = mysqli_query($conn, $sql);
  $wordListId = gvfa("word_id", $_POST);
  if($result) {
 
	  $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);

    if(count($rows) > 0) {
      $row = $rows[0];
      //die($row["word_id"] . "*" . $wordListId);
      if($row["word_list_id"] != $wordListId){
        $errors["name" ] = "The word list '" . $_POST["name"] . "' already exists.";
        return $errors;
      }

    }
    if( $_POST["name"] == ""){
      $errors["name" ] = "The word list needs an actual name.";

    }
  }
	  
  
  $data = $_POST;
  $data["user_id"] = $userId;
  unset($data["word_list_id"]);
  unset($data["action"]);
  $pk = NULL;
  if($wordId){
    $pk = array("word_list_id"=>$wordListId);
  }
  $sql = insertUpdateSql($conn, "word_list", $pk , $data);
  //die($sql);
  $result = mysqli_query($conn, $sql);
  $id = mysqli_insert_id($conn);
  header("Location: ?table=word_list");
}

  
function deleteX($userId, $xxx){
  Global $conn;
  $sql = "DELETE FROM document WHERE document_id=" . intval($documentId) . "  AND user_id=" . intval($userId);
  //die($sql);
  $result = mysqli_query($conn, $sql);
  header("Location: ?table=document");
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
 

function stringToAscii($input) {
  $asciiCodes = [];
  $length = strlen($input);

  for ($i = 0; $i < $length; $i++) {
      echo $input[$i]  . " " . ord($input[$i]) . "<br>";
  }

  return $asciiCodes;
}

function eliminateExtraLinefeeds($input) {
  // Convert all line endings to Unix style (LF)
  $input = str_replace("\r", "\n", $input);
  //$input = str_replace(chr(10), "X", $input);
  //$input = str_replace(["\r\n", "\r"], "\n", $input);
  //$input = str_replace(["\n\r", "\r"], "\n", $input);
  // Replace consecutive linefeeds with a maximum of two in a row
  $input = preg_replace("/\n{3,}/", "\n\n", $input);
  return $input;
}
 

function insertUpdateSql($conn, $tableName, $primaryKey, $data) {
  $_dataRaw = gvfa("_data", $data); 
  if($_dataRaw){
    $_data = json_decode($_dataRaw, true);
  }
  // Check if a primary key is provided
  $sanitizedKeys = [];
  $dataToScan = $_data;
  if(!$_data){
    $dataToScan = $data;
    die();//don't worry about this case
  }
 
  foreach ($dataToScan as $datum) {
    $column = $datum["name"];
    $value = gvfa($column, $data, "");
    if($column  != "_data") {
      if($column == "created") {

        $date = new DateTime("now", new DateTimeZone('America/New_York'));//obviously, you would use your timezone, not necessarily mine
        $formatedDateTime =  $date->format('Y-m-d H:i:s'); 
        $sanitizedData[] = $formatedDateTime;
        
      } else {
        $sanitized = mysqli_real_escape_string($conn, $value);
        $sanitizedData[] = $sanitized;
        
      }
      //echo $column . "<BR>";
      $sanitizedKeys[] = $column;
    }
  }
 
  if (!empty($primaryKey) && implode(",", array_values($primaryKey)) != "") {
      // Update the existing record
      $sanitizedData = [];
      $updateFields = [];
      //$sanitizedKeys = [];
      foreach ($dataToScan as $datum) {
        $column = $datum["name"];
        $type =  gvfa("type", $datum, "");
        $value = gvfa($column, $data, "");

        //echo  $column . "=" . $value . ", " . $type . "<BR>";
        if($column != "created" && $column != "_data" && array_key_exists($column, $primaryKey) == false) {
          if(($type == "bool"  || $type == "checkbox") && !$value){
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

      $sql = "UPDATE `$tableName` SET $updateFieldsString WHERE $whereClauseString;";
  } else {
      // Insert a new record
      $columns = implode(', ', $sanitizedKeys);
      $values = implode("', '", $sanitizedData);

      $sql = "INSERT INTO `$tableName` ($columns) VALUES ('$values');";
  }
  
  //die($sql);
  return $sql;
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

 
