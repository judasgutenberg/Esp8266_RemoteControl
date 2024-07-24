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
      sortTable(event, currentSortColumn);
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

function initGoogleMap() {
	//don't do anything
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

function formatJSON(jsonString, indent = 2) {
  try {
      // Parse the JSON string into an object
      const jsonObj = JSON.parse(jsonString);
      
      // Convert the object back into a formatted JSON string
      return JSON.stringify(jsonObj, null, indent);
  } catch (error) {
      // Handle errors in case of invalid JSON
      console.error("Invalid JSON string:", error);
      return null;
  }
}

function formatSQL(sql) {
  const indentString = "  "; // Two spaces for indentation
  const keywords = [
      "SELECT", "FROM", "WHERE", "AND", "OR", "INSERT", "INTO", "VALUES",
      "UPDATE", "SET", "DELETE", "JOIN", "INNER JOIN", "LEFT JOIN", "RIGHT JOIN",
      "ON", "GROUP BY", "ORDER BY", "LIMIT", "OFFSET", "HAVING", "DISTINCT"
  ];
  
  const keywordSet = new Set(keywords.map(k => k.toUpperCase()));
  let formattedSQL = "";
  let indentLevel = 0;
  let isSelectClause = false;

  const tokens = sql.split(/\s+/);
  let i = 0;

  while (i < tokens.length) {
      const token = tokens[i].trim();
      if (!token) {
          i++;
          continue;
      }

      const upperToken = token.toUpperCase();
      if (keywordSet.has(upperToken)) {
          if (formattedSQL.trim().length > 0) {
              formattedSQL += "\n";
          }

          if (upperToken === "SELECT") {
              isSelectClause = true;
          } else {
              isSelectClause = false;
          }

          if (upperToken === "FROM" || upperToken === "WHERE" || upperToken === "JOIN" || upperToken === "ON" || upperToken === "GROUP BY" || upperToken === "ORDER BY" || upperToken === "LIMIT") {
              indentLevel = 1;
          } else {
              indentLevel = 0;
          }

          formattedSQL += `${indentString.repeat(indentLevel)}${upperToken}`;
      } else {
          if (isSelectClause) {
              // Handle columns in SELECT clause
              formattedSQL += `\n${indentString.repeat(indentLevel + 1)}${token}`;
              if (tokens[i + 1] && tokens[i + 1] !== ',' && tokens[i + 1].toUpperCase() !== 'FROM') {
                  formattedSQL += ",";
              }
          } else {
              formattedSQL += (i > 0 && formattedSQL.slice(-1) !== "\n" ? " " : "") + token;
          }
      }

      if (upperToken === "SELECT" || upperToken === "INSERT" || upperToken === "UPDATE" || upperToken === "DELETE") {
          indentLevel = 1;
      }

      i++;
  }

  return formattedSQL.trim();
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

  if(!caption){
    caption = "";
  }
	
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

function plotOnMap(map, records, viewOptionInfo) {
	var place = document.getElementById("googleMapper");
	if(!place) {
		return;
	}
	/*
	setTimeout(function() {
	google.maps.event.trigger(map, "resize");

	}, 4000);
	*/
	place.style.display = 'block';

	if(typeof(viewOptionInfo["width"]) != "undefined") {
		place.style.width = viewOptionInfo.width + "px";
	} else {
		//place.style.width = "1000px";
	}
	if(viewOptionInfo.height) {
		canvas.style.height= viewOptionInfo.height + "px";
	} else {

		//place.style.height = "600px";
	}
	var degreeWiggle = 0;
	var degreeWiggleLatColumn = null;
	var degreeWiggleLonColumn = null;
	var markerArray = new Array();
	var markerCursor = 0;
	var defaultOpacity = "0.1";
	
	var reportVisualization = document.getElementById("reportVisualization");
	if(reportVisualization) {
		reportVisualization.style.display = 'none';
	}
	var strStockColors = "ff0099,ff9900,99ff99,9999ff,990099,00ff00,00ffff,ffff00,0000ff,999900,990000,ff0000,000000,ffffff,330000,990033";
	
	if(viewOptionInfo.colors) {
		strStockColors = viewOptionInfo.colors;
	}
	var colorArray = strStockColors.split(",");
	var defaultColor = colorArray[0];
	if(viewOptionInfo.color) {
		defaultColor = viewOptionInfo.color;

	}
 
	var zoom = 0;
	if(viewOptionInfo.zoom) {
		zoom = viewOptionInfo.zoom;
	}
	if(viewOptionInfo.degreeWiggle) {
		degreeWiggle = viewOptionInfo.degreeWiggle;
	}

	if(viewOptionInfo.degreeWiggleLatColumn) {
		degreeWiggleLatColumn = viewOptionInfo.degreeWiggleLatColumn;
	}	
	if(viewOptionInfo.degreeWiggleLonColumn) {
		degreeWiggleLonColumn = viewOptionInfo.degreeWiggleLonColumn;
	}	
	var mapType = 'terrain';
	if(viewOptionInfo.mapType) {
		mapType = viewOptionInfo.mapType;
	}
	var lat = viewOptionInfo.lat;
	var lon = viewOptionInfo.lon;
	if(!lat || !lon || !zoom) {
		var center = centerOfGeoPlot(viewOptionInfo, records);
		//console.log(viewOptionInfo.plots[0]);
		console.log(center);
		if(!lat) {
			lat = center.lat;
		}
		if(!lon) {
			lon = center.lon;
		}
		if(!zoom) {
			zoom = center.estimatedScale;
		}
		if(!zoom) {
			zoom = 5;
		}

	}
 

	 map = new google.maps.Map(place, {
          zoom: zoom,
          center: new google.maps.LatLng(lat, lon),
          mapTypeId: mapType
        });


	//console.log(viewOptionInfo);
	for (var j = 0; j < viewOptionInfo.plots.length; j++) {
	 
		var plot = viewOptionInfo.plots[j];
		if(plot.degreeWiggle) {
			degreeWiggle = plot.degreeWiggle;
		}
		if(plot.degreeWiggleLatColumn) {
			degreeWiggleLatColumn = plot.degreeWiggleLatColumn;
		}	
		if(plot.degreeWiggleLonColumn) {
			degreeWiggleLonColumn = plot.degreeWiggleLonColumn;
		}

		var defaultWeight = 1;
		if(plot.lineWeight) {
			defaultWeight = plot.lineWeight;
		}
		if(!plot.lineWeight && viewOptionInfo.lineWeight) {
			defaultWeight = viewOptionInfo.lineWeight;
		}

		var latColumn = plot.latColumn;
		var lonColumn = plot.lonColumn;
		var iconName = plot.iconName;
   	  	var plotColor = defaultColor;
		var plotOpacity = defaultOpacity;
		var magnitudeColumn = plot.magnitudeColumn;
		var labelTemplate = plot.labelTemplate;
		var linkTemplate = plot.linkTemplate;
		var infoWindowTemplate = plot.infoWindowTemplate;
		if(plot.color) {
			plotColor = plot.color;
		}
		if(plot.opacity) {
			plotOpacity = plot.opacity;
		}

	    for (var i = 0; i < records.length; i++) {
	    	var didPlot = false;
	    	var record = records[i];
	    	var iconPath = "icons/default.png";
	    	var markerUrl = null;
	    	if(plot[iconName]) {
	    		iconPath = "icons/" + plot[iconName] + ".png";
	    	}

	    	//lat/lon wiggle make sure that not all the map
	    	//markers don't stack on top of each other if all
	    	//the addresses in a zipcode get the same lat/lon
	    	//values.  degreeWiggleLatColumn/degreeWiggleLonColumn
	    	//make it so the markers always end up in the same random
	    	//place on the map
	    	var latWiggleAmount = 0;
	    	var lonWiggleAmount = 0;
			if(degreeWiggleLatColumn  && record[degreeWiggleLatColumn]) {
				latWiggleAmount = (record[degreeWiggleLatColumn] % 1000)/1000;
			} else if(degreeWiggle) {
				latWiggleAmount = Math.random();
			}
			if(degreeWiggleLonColumn && record[degreeWiggleLonColumn]) {
				lonWiggleAmount = (record[degreeWiggleLonColumn] % 1000)/1000 ;
			} else if(degreeWiggle) {
				lonWiggleAmount = Math.random();
			} 

	    	var degreeWiggleDeltaLat = degreeWiggle * latWiggleAmount - (0.5 * degreeWiggle);
	    	var degreeWiggleDeltaLon = degreeWiggle * lonWiggleAmount - (0.5 * degreeWiggle);
	      	//console.log(degreeWiggleDeltaLat + ' ' + degreeWiggleDeltaLon);
	      	var latLng = new google.maps.LatLng(parseFloat(record[latColumn]) + degreeWiggleDeltaLat,parseFloat(record[lonColumn]) + degreeWiggleDeltaLon);
	      	//console.log(latLng);
	      	
	      	var markerConfig = {
		        position: latLng,
		        map: map
	      	}
	      	if(plot[iconName]) {
	      		markerConfig["icon"] = iconPath;
	      	}
	      	var quantity =  record[magnitudeColumn]; 
			if(!quantity) {
				quantity=0;
			} else {
				quantity = parseFloat(quantity);
			}
			if(plot.quantityDivisor) {
				quantity = quantity/plot.quantityDivisor;
			}
			if(labelTemplate) {
				markerConfig["label"] = genericParamReplace(labelTemplate, record, "@");
			}
			if(linkTemplate) {
				markerUrl = genericParamReplace(linkTemplate, record, "@");
			}

			plotColor = manipulateColor(plotColor, plot, record);

			//console.log(defaultColor);
			var marker = null;
			var mapShape = null;
	      	if(plot["shape"]) {
	      		if(plot["shape"].radius) {
	      			quantity = plot["shape"].radius;
	      		}
	      		if(plot["shape"].type) {
	      			if(plot["shape"].type == "circle") {
	      				circleConfig = {

							strokeColor: colorFix(defaultColor),
							strokeOpacity: defaultOpacity,
							strokeWeight: defaultWeight,

							fillColor: colorFix(plotColor),
							fillOpacity: plotOpacity,
							center: latLng,
							map: map,
							icon: "default",
							radius: quantity

	      				}
	      				//console.log(circleConfig);
	      				mapShape = new google.maps.Circle(circleConfig);
	      				 
	      				var didPlot = true;
	      			}
	      		}
	      	}
	      	if(!didPlot || plot["alwaysShowIcon"]) {

	      		markerArray[markerCursor] = new google.maps.Marker(markerConfig);
	      		if(markerUrl) {
	      			//console.log(markerUrl);
	      			//markerArray[markerCursor].addListener("click", function() {navigateToPage(markerUrl)});
	      			attachHyperlinkToMapMarker(markerArray[markerCursor], markerUrl);

	      		}
	      		if(plot["infoWindowTemplate"]) {
	      		 
	      			//markerArray[markerCursor].addListener("mouseover", function() {console.log(markerCursor)});
	      			attachHoverMapMarker(markerArray[markerCursor], plot["infoWindowTemplate"], record);
	      		}
	      		var didPlot = true;
	      		markerCursor++;
	     	}
	    }
	}
}

//looks at records and tries to find the center of the data, as well as a zoom for google maps to use
//plots can now be viewOption to get the center of all plots
function centerOfGeoPlot(plot, records) {
	var latSum = 0;
	var lonSum = 0;
	var count = 0;
	var minLat = 100;
	var minLon = 400;
	var maxLat = 0;
	var maxLon = -300;
	var plots = [plot];
	if(plot.plots) {
		plots = plot.plots;
	}
	for(var j=0; j<plots.length; j++) {
		var plot = plots[j];
		if(plot) {
			//console.log(plot);
		    for (var i = 0; i < records.length; i++) {
		    	var record = records[i];
		    	if(isNumeric(record[plot.latColumn]) && isNumeric(record[plot.lonColumn])) {
			    	var lat = parseFloat(record[plot.latColumn]);
			    	var lon = parseFloat(record[plot.lonColumn]);
			    	//console.log(lat + " " + lon);
			      	latSum += lat;
			      	lonSum += lon;
			      	if(lon < minLon) {
			      		minLon = lon;
			      	}
			      	if(lon > maxLon) {
			      		maxLon = lon;
			      	}
			      	if(lat < minLat) {
			      		minLat = lat;
			      	}
			      	if(lat > maxLat) {
			      		maxLat = lat;
			      	}
			      	count++;
		      	}
		     }
		     //12 is about 15 miles wide 12 diag
		    //8 is about 200 miles wide   150 diag
		    //6 is about 1200 miles wide  900 diag
		    //4 is about 4000 miles across 3000 diag
		    //basic pythagorean stuff here:
		    var diagonalLength = Math.pow(Math.pow((maxLon-minLon), 2) + Math.pow((maxLat-minLat), 2), 0.5);
		    //i know, 60 miles per degree is only true of latitude, but for these purposes this is close enough:
		    var estimatedDiagonalInMiles = parseFloat(diagonalLength) * 60;
		    var estimatedScale = 2;
		    //var estimatedDiagonalInMiles = diagonalLength * 60;
		    //after some experimentation, i've found this calculates a good Google Maps zoom factor from a max quadrangle diagonal length:
			estimatedScale = parseInt(16-(Math.log2(estimatedDiagonalInMiles * 1.20)));
			if(estimatedScale < 1) {
				estimatedScale = 1;
			}
		    return {"lat": latSum/count, "lon":lonSum/count,  "maxLat": maxLat, "maxLon": maxLon,  "minLat": minLat, "minLon": minLon, "diagonalLength":diagonalLength, "distanceInMiles":estimatedDiagonalInMiles, "estimatedScale":estimatedScale}
		}
	}

}