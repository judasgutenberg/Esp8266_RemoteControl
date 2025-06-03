const values = new Array();

function toggleDeviceFeature(deviceFeatureId, hashedEntries,  actuallySetState) { //used to set state or to just retrieve existing state
  let url = 'tool.php?action=genericFormSave&table=device_feature&primary_key_name=device_feature_id&primary_key_value=' + deviceFeatureId + '&value=' + !values[deviceFeatureId] + '&name=value&hashed_entities=' + hashedEntries;
  if(!actuallySetState) {
    url = "data:application/json,{}";
  }
  let bgColor = "#ccccff";
  
  let urlGet = 'tool.php?action=json&table=device_feature';
  if(deviceFeatureId) {
    urlGet += '&device_feature_id=' + deviceFeatureId;
  }
  console.log(urlGet);
  fetch(url)
    .then(response => {
      if (response.ok) {
        fetch(urlGet).then(response => {
          if(response.ok) {
            //console.log(response);
            return response.json();

          }
        }).then(data => {
            //console.log(data);
            if("value" in data) {
              changeButton(data)
            } else {
              Object.keys(data).forEach(key => {
                changeButton(data[key])
              });
            }
        });
      } else {
        alert('Failed to send command');
      }
    })
    .catch(err => alert('Error: ' + err));

}

function changeButton(data) {
  bgColor = '#cccccc';
  let state = false;
  stateDisplay = "Unknown or non-existent";
  if(data["value"] === "1") {
    state = true;
    stateDisplay = "Just Switched ON";
    bgColor = "#ffe7cc"
  } else if (data["value"] === "0"){
    state = false;
    stateDisplay = "Just Switched OFF";
    bgColor = "#e7e7ff"
  }
  if(data["last_known_device_value"] === "1" && data["value"] === "1") {
    bgColor = "#ffffcc"
    stateDisplay = "Confirmed ON";
  }
  if(data["last_known_device_value"] === "0" && data["value"] === "0") {
    bgColor = "#ccccff"
    stateDisplay = "Confirmed OFF";
  }
  let deviceFeatureId = data["device_feature_id"];
  if(document.getElementById("statedisplay_" + deviceFeatureId)) {
    document.getElementById("statedisplay_" + deviceFeatureId).innerHTML = stateDisplay;
    document.getElementById("toggleButton_" + deviceFeatureId).style.backgroundColor  = bgColor;
    document.getElementById("toggleButton_" + deviceFeatureId).innerHTML = data["name"];
  }
  values[deviceFeatureId] = state;
  return state
}

function updateFeatureDetailButtons() {
  setTimeout(()=>{
  toggleDeviceFeature(null, null,  false);
  updateFeatureDetailButtons();
  },
  5000);
}