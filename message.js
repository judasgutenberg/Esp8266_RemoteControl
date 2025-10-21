
function sendMessage(textAreaId) {
    let textArea = document.getElementById(textAreaId);
    let targetDeviceDropdown = document.getElementById("target_device_id");
    if(textArea){
        let targetDeviceId = targetDeviceDropdown[targetDeviceDropdown.selectedIndex].value
        let content = textArea.value;
    	  let xmlhttp = new XMLHttpRequest();
          xmlhttp.onreadystatechange = function() {
              console.log(xmlhttp.responseText);
              textArea.value = "";
              updateGridNow(true);
          }
          let url = "tool.php?action=sendmessage&target_device_id=" + targetDeviceId + "&content=" + encodeURIComponent(content);
          xmlhttp.open("GET", url, true);
          xmlhttp.send();
    }
}

currentSortColumn = 0;