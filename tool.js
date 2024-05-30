function managementRuleTool(item) {
  let xmlhttp = new XMLHttpRequest();
  xmlhttp.onreadystatechange = function() {
    console.log(xmlhttp.responseText);
    let data = JSON.parse(xmlhttp.responseText);
    showDataInPanelTool(data);
    //console.log(data);
  }

  let url = "?table=management_rule&action=json&management_rule_id=" + item.value;
  xmlhttp.open("GET", url, true);
  xmlhttp.send();
 
}




function formSubmitTasks() {
  if(onSubmitManyToManyItems){
    for(const item of onSubmitManyToManyItems){
      if(document.getElementById("dest_" + item)){
        for(const option of document.getElementById("dest_" + item).options) {
          //values.push(option.value);
          option.selected = true;
          console.log(option);
        }
      }
    }
  }
  
}


function showDataInPanelTool(data){
  let html = "<div class='list'>";
  let panelId = "";
  for (let key in data) {
    console.log(key);
    html += "<div class='listrow'><span><b>" + key + "</b></span><span>" + data[key] + "</span></div>";
    if(document.getElementById("panel_" + key)) {
      panelId = "panel_" + key;
    }
  }
  html += "</div>";
  document.getElementById(panelId).innerHTML = html;
}

function autoUpdate(encryptedSql, headerData, tableId){
  var decodedHeaderData = JSON.parse(headerData);
  var xmlhttp = new XMLHttpRequest();
  xmlhttp.onreadystatechange = function() {
 
    if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
      //console.log(xmlhttp.responseText);
      var data = JSON.parse(xmlhttp.responseText);
      let tableRows = document.getElementsByClassName("listrow");
      
      rowCounter = 0;
      for (const row of tableRows) {
        var htmlRow = tableRows[rowCounter];
        var dataRecord = data[rowCounter];
        if(htmlRow) {
          //console.log(htmlRow);
          var spans = htmlRow.getElementsByTagName("span");
          cellCounter = 0;
          for (const column of decodedHeaderData){
            //console.log(column);
            let key = column["name"];
            let newcolumnData = dataRecord[key];
            //console.log(key, newcolumnData);
            if(spans[cellCounter].innerHTML.indexOf("<input") == -1) {
              spans[cellCounter].textContent = newcolumnData;
            }
            cellCounter++;
          }
        }
        rowCounter++;

      }
    }
    //gotta reapply latest sort:
    //nah, let's not do it this way
    /*
    if(currentSortColumn > -1) {
      ascending = -ascending;
      sortTable(currentSortColumn);
    }
    */
  }
  const params = new URLSearchParams();
  params.append("direction", ascending);
  params.append("sortColumn", currentSortColumn);
  params.append("action", "runencryptedsql");
  params.append("headerData",  headerData.replace(/'/g, "\\'"));
  params.append("value",  encryptedSql);

  let url = "tool.php"; 
  //console.log(url);
  xmlhttp.open("POST", url, true);
  xmlhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
  xmlhttp.send(params);
  setTimeout(function() { 
    autoUpdate(encryptedSql, headerData, tableId);
  }, 7000);

}

function copyManyToMany(sourceId, destinationId){
  let source = document.getElementById(sourceId);
  let dest = document.getElementById(destinationId);
  for(var i = source.options.length - 1; i >= 0; i--) {
    var option = source.options[i];
    if(option.selected) {
        dest.appendChild(option.cloneNode(true)); // Clone the option before appending
        source.remove(i);
    }
  }
  return false;
}

function copy(id){
  var href = document.getElementById("href" + id.toString());
  var contentDiv = document.getElementById("clip" + id.toString());
  var clipContent = "";
  if(href) {
    clipContent = href.innerHTML;
  } else if(contentDiv) {
    clipContent = contentDiv.innerHTML;
  }
  if(clipContent) {
    copyHack(clipContent);
    var backgroundColor = document.body.style.backgroundColor;
    document.body.style.backgroundColor = '#ccffcc';
    setTimeout(function () {document.body.style.backgroundColor = backgroundColor}, 200);
  }
}

function copyHack(value){ //such a hack!!
    var copyTextarea = document.createElement("textarea");
    copyTextarea.style.position = "fixed";
    copyTextarea.style.opacity = "0";
    copyTextarea.textContent = value;
 
    document.body.appendChild(copyTextarea);
    copyTextarea.select();
    document.execCommand("copy");
    document.body.removeChild(copyTextarea);
}

function cleanText(inputText) {
  const tempDiv = document.createElement('div');
  tempDiv.innerHTML = inputText;
  const cleanedText = tempDiv.innerText.trim();
  return cleanedText;
}

function getFormValues(formId) {
  var form = document.getElementById(formId);
  var formData = {};

  if (form) {
      var elements = form.elements;

      for (var i = 0; i < elements.length; i++) {
          var element = elements[i];
          var elementType = element.type;
          var elementName = element.name;

          if (elementName) {
              switch (elementType) {
                  case 'select-one':
                      formData[elementName] = element.options[element.selectedIndex].value;
                      break;
                  case 'select-multiple':
                      formData[elementName] = getSelectedOptions(element);
                      break;
                  case 'checkbox':
                  case 'radio':
                      formData[elementName] = element.checked ? element.value : null;
                      break;
                  default:
                      formData[elementName] = element.value;
              }
          }
      }
  }

  return formData;
}

function getSelectedOptions(selectElement) {
  var selectedOptions = [];
  for (var i = 0; i < selectElement.options.length; i++) {
      if (selectElement.options[i].selected) {
          selectedOptions.push(selectElement.options[i].value);
      }
  }
  return selectedOptions;
}

function startWaiting(message){
  var waitingBody = document.getElementById("waitingouter");
  waitingBody.style.display = 'block';
  var waitingMessage = document.getElementById("waitingmessage");
  waitingMessage.innerHTML = message;
}
function stopWaiting(){
  var waitingBody = document.getElementById("waitingouter");
  waitingBody.style.display = 'none';
  
}

function deleteListRows(idOfParent, classToKill) { //thanks chatgpt!
  var parentDiv = document.getElementById(idOfParent); 
  var listRows = parentDiv.getElementsByClassName(classToKill);
  while (listRows.length > 0) {
      parentDiv.removeChild(listRows[0]);
  }
}


if(document.getElementById('file')) {
  document.getElementById('file').onchange = function() {
    startWaiting();
  };
}
window.addEventListener('resize', function() {
  window.location.reload();
});

function genericListActionBackend(name, value, tableName, primaryKeyName, primaryKeyValue, hashedEntities){ //this is a security nightmare;  need to improve!!
  var xmlhttp = new XMLHttpRequest();
  xmlhttp.onreadystatechange = function() {
    if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
      //alert(xmlhttp.responseText);
      //var data = JSON.parse(xmlhttp.responseText);
 
    }
  };
  let url = "?action=genericFormSave&table=" + encodeURIComponent(tableName) + "&primary_key_name=" + encodeURIComponent(primaryKeyName) + "&primary_key_value=" + encodeURIComponent(primaryKeyValue) + "&value=" + encodeURIComponent(value) + "&name=" + encodeURIComponent(name) + "&hashed_entities="  + encodeURIComponent(hashedEntities); 
  xmlhttp.open("GET", url, true);
  xmlhttp.send();
}

function genericSelect(id, name, defaultValue, data, event = "", handler = "") {
  let out = "";
  out += `<select name="${name}" id="${id}" ${event}="${handler}">\n`;
  out += "<option></option>";
  console.log(data);
  data.forEach(datum => {
      let value = datum.value !== undefined ? datum.value : datum;
      let text = datum.text !== undefined ? datum.text : datum;
      let selected = defaultValue == value ? " selected='true'" : "";

      out += `<option${selected} value="${value}">`;
      out += text;
      out += "</option>";
  });

  out += "</select>";
  return out;
}

let managementToolTableName = "";
let managementToolColumnName = "";
let managementToolTableHasLocationIdColumn = false;

function managementRuleTableChange() {
  managementToolTableName = document.getElementById("tableNameForManagementRule")[document.getElementById("tableNameForManagementRule").selectedIndex].value;
  var xmlhttp = new XMLHttpRequest();
  let mrColumn =  document.getElementById("mr_column");
  xmlhttp.onreadystatechange = function() {
    if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
      //alert(xmlhttp.responseText);
      let columns = JSON.parse(xmlhttp.responseText);
      managementToolTableHasLocationIdColumn = columns.includes("location_id");
      mrColumn.innerHTML = "<span>Column:</span><span> " + genericSelect("columnNameForManagementRule", "columnNameForManagementRule", "", columns, "onchange", "managementRuleColumnChange()"  )  + "</span>";
    }
  }

  let url = "?action=getcolumns&table=" + encodeURIComponent(managementToolTableName); 
  xmlhttp.open("GET", url, true);
  xmlhttp.send();
}

function managementRuleColumnChange() {
  managementToolColumnName = document.getElementById("columnNameForManagementRule")[document.getElementById("columnNameForManagementRule").selectedIndex].value;
  var xmlhttp = new XMLHttpRequest();
  let mrLocation =  document.getElementById("mr_location");
  if(!managementToolTableHasLocationIdColumn){
    tag = "<"  + managementToolTableName + "[]." + managementToolColumnName + "/>";
    managementRuleDisplayTag(tag);
  } else {
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function() {
      if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
        console.log(xmlhttp.responseText);
        let crudeLocations = JSON.parse(xmlhttp.responseText);
        let locations = crudeLocations.map(row => {
          return {
              value: row.device_id,
              text: row.name
          };
        });
        let lastTagScript =  "makeManagementRuleTagFromLocation(document.getElementById('locationForManagementRule')[document.getElementById('locationForManagementRule').selectedIndex].value)";
        mrLocation.innerHTML = "<span>Location:</span><span>" + genericSelect("locationForManagementRule", "locationForManagementRule", "",  locations, "onchange", lastTagScript ) + "</span>";

      }
    }

    let url = "?action=getdevices"; 
    xmlhttp.open("GET", url, true);
    xmlhttp.send();

  }

}

function unescapeHtml(html) {
  let textArea = document.createElement('textarea');
  textArea.innerHTML = html;
  return textArea.value;
}

function managementRuleDisplayTag(tag) {
  let mrTag =  document.getElementById("mr_tag");
  mrTag.innerHTML = tag.replace(/</g, '&lt;').replace(/>/g, '&gt;');
  let mrButton =  document.getElementById("mr_button");
  mrButton.style.display = 'block';
}

function makeManagementRuleTagFromLocation(location) {
  let mrTag =  document.getElementById("mr_tag");
  let tag = "<"  + managementToolTableName + "[" + location + "]." + managementToolColumnName + "/>";
  mrTag.innerHTML = tag.replace(/</g, '&lt;').replace(/>/g, '&gt;');
  let mrButton =  document.getElementById("mr_button");
  mrButton.style.display = 'block';
}

function managementConditionsAddTag() {
  let mrTag =  document.getElementById("mr_tag");
  let tag = unescapeHtml(mrTag.innerHTML);
  let formItemName = "conditions";
  let textareas = document.getElementsByName(formItemName);
  if (textareas.length > 0) {
    let textarea = textareas[0];

    // Append the text to the existing content
    textarea.value += tag;
  } else {
      console.warn(`Textarea with name "${formItemName}" not found.`);
  }
}
