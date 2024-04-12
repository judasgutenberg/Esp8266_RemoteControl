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
      'label' => 'power on',
      'name' => 'value',
      'liveChangeable' => true,
      'type' => 'bool'
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
    $toolsTemplate .= "<a onclick='return confirm(\"Are you sure you want to delete this " . $table . "?\")' href='?table=" . $table . "&action=delete&" . $table . "_id=<" . $table . "_id/>'>Delete</a>";
    if($result) {
      $rows = mysqli_fetch_all($result, MYSQLI_ASSOC);
      
      if($rows) {
        $out .= genericTable($rows, $headerData, $toolsTemplate, null,  $table, $table . "_id", $sql);
      }
    }
    return $out;

}
