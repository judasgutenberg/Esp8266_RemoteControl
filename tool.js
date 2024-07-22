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



//report-related graphing:
///////////////////////////////////////

function displayReport(reportId, canvasId) {
  displayViewOption(canvasId, reportData, reportOutput , reportId);

}

//does adjustments to colors based on plot parameters and record data
//this allows data to change colors dynamically in graphs and maps
function manipulateColor(plotColor, plot, record) {
	//do color effects from data if specified
	var darkenByDivisor = 1;
	var lightenByDivisor = 1;
	var saturateByDivisor = 1;
	var desaturateByDivisor = 1;

	if(plot.darkenByDivisor) {
		darkenByDivisor = plot.darkenByDivisor;
	}
	if(plot.lightenByDivisor) {
		lightenByDivisor = plot.lightenByDivisor;
	}
	if(plot.saturateByDivisor) {
		saturateByDivisor = plot.saturateByDivisor;
	}
	if(plot.desaturateByDivisor) {
		desaturateByDivisor = plot.desaturateByDivisor;
	}
	if(plot.darkenBy) {
		//console.log(plotLineColor + " " + tinycolor(plotLineColor).darken(30) + " "+ record[plot.darkenBy]);
		plotColor = tinycolor(plotColor.toString()).darken(parseInt(record[plot.darkenBy]/darkenByDivisor)).toHexString()
	}
	if(plot.lightenBy) {
		//console.log(plotLineColor + " " + tinycolor(plotLineColor).darken(30) + " "+ record[plot.darkenBy]);
		plotColor = tinycolor(plotColor.toString()).lighten(parseInt(record[plot.lightenBy]/lightenByDivisor)).toHexString()
	}
	if(plot.saturateBy) {
		//console.log(plotLineColor + " " + tinycolor(plotLineColor).darken(30) + " "+ record[plot.darkenBy]);
		plotColor = tinycolor(plotColor.toString()).saturate(parseInt(record[plot.saturateBy]/saturateByDivisor)).toHexString()
	}
	if(plot.desaturateBy) {
		//console.log(plotLineColor + " " + tinycolor(plotLineColor).darken(30) + " "+ record[plot.darkenBy]);
		plotColor = tinycolor(plotColor.toString()).desaturate(parseInt(record[plot.desaturateBy]/desaturateByDivisor)).toHexString()
	}
	return plotColor;
}

function reportsDivSet(divToShow, reportId) {
	var allDivs = ["googleMapsDiv","reportVisualization", "searchResults-" . reportId, "searchResults"];
	//console.log(allDivs.length);
	if(reportId) {
		for(var i=0; i<allDivs.length; i++) {
			var divInQuestion = document.getElementById(allDivs[i]);
			if(divInQuestion) {
				//console.log(divInQuestion);
				if(allDivs[i] != divToShow) {
					//console.log(allDivs[i]);
					divInQuestion.style.display = 'none';
				} else {
					//console.log("ok:" + allDivs[i]);
					divInQuestion.style.display = 'block';
				}
			}
		}
	}
}



//values can be a reportId if you want to read a live form
function genericParamReplace(stringIn, values, paramPrefix) {
	if(!paramPrefix) {
		paramPrefix = "@";
	}
	if((typeof values == "string")){
		var formIn;
		//values is actually a reportId, so read the live values off the form
		if(document.getElementById("reportForm-" + values)) {
			formIn = document.getElementById("reportForm-" + values).elements;
		} else if(document.getElementById("searchForm")) {
			formIn = document.getElementById("searchForm").elements;
		} else {
			return stringIn;
		}
		values = new Array();
		//console.log(formIn.length);
		for(var i=0; i<formIn.length; i++) {
			var element = formIn[i];
			//console.log(element);
			//if(formItem["name"]) {
				values[element.name] = decodeURIComponent(element.value);
			//}
		}
	}
	for(var key in values) {
		var value = values[key];
		if(key.trim() != "") {
			//console.log(value);
			//apparently there are errors in this process at times, so:
			try {
				var decodedUriComponent = decodeURIComponent(value);
				stringIn = stringIn.replace(paramPrefix + key, decodedUriComponent);
				//console.log(decodedJsonData);
			}
			catch (err) { 
				//dont bother
			}
 			
 		}
	}
	return stringIn;
}

function isHexDec(inVal) {
	var re = /[0-9A-Fa-f]{6}/g;
	if(re.test(inVal)) {
	    return true;
	} else {
	    return false;
	}
}

//returns either a string beginning with # or a possibly-acceptable color name based on the input. 
//returns an array if given one or if the colors are a string list 
function colorFix(strIn) {
	//console.log(strIn);
	if(strIn) {
		if((typeof strIn == "string")){
			strIn = strIn.replace('#', '').trim();
			if(strIn.indexOf(" ")) {
				strIn = strIn.split(" "); //handle below
			} else {
				if(isHexDec(strIn)) {
					return "#" + strIn.trim();
				} else {

					return strIn.trim();
				}
			}
		} 
		if (strIn instanceof Array) {
			for(var i=0; i<strIn.length; i++) {
				//console.log(strIn[i]);
				strIn[i] = strIn[i].replace('#', '');
				if(isHexDec(strIn)) {
					strIn[i] = "#" + strIn[i].trim();
				} else {

					strIn[i] = strIn[i].trim();
				}
			}
			return strIn;

		}
	}

}


//uses charts.js library to make all sorts of graphs. also a serves as the launch point for other viewOptions like google maps and calendar
function displayViewOption(canvasId, records, viewOptionInfo, reportId) {
	if(!viewOptionInfo) {
		return;
	}
	//allow us to manipulate the graphing process with report form inputs (just put @variables in the JSON anywhere JSON will allow it)
	if(reportId) {
		var encodedViewOptions = genericParamReplace(JSON.stringify(viewOptionInfo), reportId, "@");
		//console.log(encodedViewOptions);
		viewOptionInfo =  JSON.parse(encodedViewOptions);
	}
	var type = viewOptionInfo.type;
	if(!type) {
		type = "line";
	}

	//map is handled by google maps, not charts.js, so catch that here:
	if(type == "map") {
		reportsDivSet("googleMapsDiv", reportId);
		return plotOnMap(globalMap, records, viewOptionInfo);
	} else if(type == "email") {
		reportsDivSet("searchResults-" + reportId, reportId);
		return sendEmailConfirm(records, viewOptionInfo, reportId);
	}	if(type == "calendar") {
		reportsDivSet("searchResults-" + reportId, reportId);
		return displayCalendar(records, viewOptionInfo, reportId);
	}

	reportsDivSet("reportVisualization", reportId);
	//in case we need to draw pie charts and no colors were supplied in the root of the viewOptionInfo:
	var strStockColors = "ff0099,ff9900,99ff99,9999ff,990099,00ff00,00ffff,ffff00,0000ff,999900,990000,ff0000,000000,ffffff,330000,990033";
	if(viewOptionInfo.colors) {
		strStockColors = viewOptionInfo.colors;
	}

	if(viewOptionInfo.colors) {
		strStockColors = viewOptionInfo.colors;
	}
	var stockColors = strStockColors.split(",");
	var defaultColor = stockColors[0];
	var noAxes = ["doughnut", "pie"];
	var plots = viewOptionInfo.plots;
	var caption = viewOptionInfo.caption;
	var dataSets = new Array();
	var labels = new Array();
	var labelsMade = false;
	var colorIndex = 0;
	
	var borderColor;
	if(plots) {
		for(var i=0; i<plots.length; i++) {
			var plot = plots[i];
			var colorsForPies = new Array();
			defaultWeight = 1;
			if(plot.lineWeight) {
				defaultWeight = plot.lineWeight;

			}
			if(!plot.lineWeight && viewOptionInfo.lineWeight) {
				defaultWeight = viewOptionInfo.lineWeight;
			}

			//console.log(plot);
			//narrow the data:
			var narrowedData = new Array();
			var colorToUse = new Array();
			if(records && records.length) {
				for(var j=0; j<records.length; j++) {
					var record = records[j];
					if(!viewOptionInfo.excludeValue || (viewOptionInfo.excludeValue && viewOptionInfo.excludeValue!=record[viewOptionInfo.labelColumn])) {
						//console.log(colorFix(thisColor));
						if(!labelsMade) {
							labels.push(record[viewOptionInfo.labelColumn]);
							var thisColor = stockColors[colorIndex];

							//console.log((thisColor));
							if(colorFix(thisColor) instanceof Array) {
								colorsForPies.push(colorFix(thisColor)[0]);
								
							} else {
								colorsForPies.push(colorFix(thisColor));
							}
							colorIndex++;
							if(colorIndex >= stockColors.length) {
								colorIndex = 0;
							}
						}
						if(plot.column) {
							narrowedData.push(parseFloat(record[plot.column]));
						}
					}
					//need a default in case color is not set
					var plotLineColor = stockColors[colorIndex];
					if(plot.color) {
						plotLineColor = plot.color;
					}

					plotLineColor = manipulateColor(plotLineColor, plot, record);

					if(!(plotLineColor instanceof Array)) {
						colorToUse.push(plotLineColor);
					}
			 
					//borderColor = colorToUse;
					if (viewOptionInfo.color) {
						borderColor = viewOptionInfo.color;
					}

				}
			}
			labelsMade = true;

			if (noAxes.indexOf(type) > -1) {
				console.log(colorsForPies);
				
				colorToUse = colorsForPies;
			}

			//colorToUse = colorsForPies;
			//console.log(colorFix(colorToUse));
			//
			borderColor = colorFix(borderColor);
			//console.log(colorFix(borderColor));
			if ((borderColor instanceof Array)) {
				borderColor = borderColor[0];
			}
			if(!borderColor) {
				borderColor = colorFix(defaultColor);
			}
			//borderColor = '#000000';
			//colorToUse = '#ff0000';
			//console.log(colorToUse[0]);
	 
			if(type == 'line' && colorToUse instanceof Array) {
				colorToUse = colorToUse[0];
			}
			//console.log(colorToUse);
			var plotShape = "circle";
			var radius = 4;
			if(plot["shape"]) {
				if(plot["shape"].type) {
					plotShape = plot["shape"].type;
				}
				if(plot["shape"].radius) {
					radius = plot["shape"].radius;
				}
				//allows the circle size to be changed by data
				if(plot["shape"].alternativeMagnitude) {
					//i also want to have a synonymn for alternativeMagnitude
					//harmonize how this works with gooogle maps
					radius = record[plot["shape"].alternativeMagnitude];
				}
		  
				radius = parseFloat(radius);
				if(plot.shape && plot.shape.alternativeMagnitudeDivisor) {
					radius = radius/plot.shape.alternativeMagnitudeDivisor;
				}

			}
			dataSets.push(
			{
	            label: plot.label,
	            backgroundColor: colorFix(colorToUse),
	            borderColor: borderColor,
	            data: narrowedData,
	            borderWidth: defaultWeight,
	            pointStyle : plotShape,
	            radius : radius,
	            fill: false,
	        });
		}
	}
	var scaleDisplay = true;
	if (noAxes.indexOf(type) > -1) {
   		scaleDisplay = false;
   	}

	var scales = {
        xAxes: [{
            display: scaleDisplay,
            scaleLabel: {
                display: true,
                labelString: viewOptionInfo.labelColumn
            }
        }],
        yAxes: [{
            display: scaleDisplay,
            scaleLabel: {
                display: true,
                labelString: 'Value'
            }
        }]
    }

    var config = {
        type: type,
        data: {
            labels: labels,
            datasets: dataSets
        },
        options: {
            responsive: true,
            title:{
                display:true,
                text: caption
            },
            tooltips: {
                mode: 'index',
                intersect: false,
            },
            hover: {
                mode: 'nearest',
                intersect: true
            },
            scales: scales
        }
    };
  
	// console.log(config);
	var canvas = false;
	if(document.getElementById(canvasId)) {
		canvas = document.getElementById(canvasId).getContext("2d");
	}
	//var canvas = document.getElementById(canvasId);
	if(canvas) {
		//canvas.style.display='block';
	} else {
		return;
	}
	window.graphics = null;
	window.graphics = new Chart(canvas, config);
	/*
	if(typeof(viewOptionInfo["width"]) != "undefined") {
		canvas.style.width= viewOptionInfo.width + "px";
	}
	if(viewOptionInfo.height) {
		canvas.style.height= viewOptionInfo.height + "px";
	}
	*/

	window.graphics.update();
}
